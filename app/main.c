// Pocket Arcade: a Game Boy and Game Boy Color player for the Solana BP26
// badge, with 30 free games inside and room for your own on the card.
//
// The machine is Walnut-CGB (walnut/, MIT) behind emu.c; the bundled games
// come from games/ (pack.c, made by tools/build_pack.py); this file is the
// badge's side: the game list with a preview of each game, loading, the
// frames (ui_frame straight to the screen), keys, the menu, saves.
//
// Keys in a game: d-pad = d-pad, SELECT = A, CANCEL = B. SELECT and CANCEL
// tapped together = START; held together for a second = the menu. HOME
// leaves, and the game is kept: it is first in the list next time.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "badge.h"
#include "emu.h"
#include "pack.h"

#define FRAME_US 16743          // 70224 clocks at 4.194304 MHz
#define NAME_MAX_ 64
#define MAX_SD 24
#define MAX_ROWS (PACK_MAX_GAMES + MAX_SD + 4)
#define WHOLE_MAX (2u * 1024 * 1024)   // card games up to this are read whole; bigger ones page
#define LOAD_BUDGET_US 40000           // per tick while loading

// ---------------------------------------------------------------------------
// State

enum { ST_LIST, ST_LOAD, ST_RUN, ST_MENU, ST_PAGE, ST_ERROR };
static int state = -1;

static uint8_t index_buf[8 + PACK_MAX_GAMES * PACK_REC];
static pack_game games[PACK_MAX_GAMES];
static int n_games;

static char sd_name[MAX_SD][NAME_MAX_];
static int32_t sd_size[MAX_SD];
static int n_sd;

// The game being played.
static struct {
    int sd;                  // 0: bundled (idx into games), 1: from the card (idx into sd_name)
    int idx;
    char key[56];            // names its saves: save/<key>.sav, save/<key>.st
    char title[NAME_MAX_];
    uint32_t crc;
    int paged;
} cur;

static uint8_t *rom, *cache;
static uint32_t rom_size, load_pos, load_crc;
static int resume_after_load;

static uint16_t fb[2][EMU_W * EMU_H] __attribute__((aligned(64)));
static int fbi;

// Settings (storage)
static int big, glow = 1, swap_ab, palette;

// Running
static uint32_t acc_us, chord_ms, keys_before, save_check_ms;
static int start_frames, select_frames, shown_count;
static uint32_t stat_t0, stat_frames, stat_shown, stat_emu_us;
static uint32_t ram_sum;

static uint32_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000);
}

static void show_list(void);
static void open_menu(void);

// Handles we keep, freed before every ui_clear() (which removes the objects
// but not the handles); labels we never touch again are freed at once.
static ui_obj game_list, menu_list, credit_list, load_bar, info[4];

static void forget_ui(void)
{
    ui_obj *keep[] = {&game_list, &menu_list, &credit_list, &load_bar, &info[0], &info[1], &info[2], &info[3]};
    for (unsigned i = 0; i < sizeof keep / sizeof keep[0]; i++) {
        if (keep[i]->ref) {
            badge_obj_free(keep[i]);
        }
    }
}

static void clear_screen(uint32_t bg)
{
    forget_ui();
    ui_clear();
    ui_bg(bg);
}

static void text(int x, int y, const char *t, const ui_label_opts *o)
{
    ui_obj l = ui_label(x, y, t, o);
    badge_obj_free(&l);
}

// ---------------------------------------------------------------------------
// Reading the bundled games and the card

static const char *const pack_files[] = {"games/p0.bin", "games/p1.bin", "games/p2.bin", "games/p3.bin",
                                         "games/p4.bin", "games/p5.bin", "games/p6.bin", "games/p7.bin"};

// fs_read_at() hands back a string: at most cap - 1 bytes and a NUL, so it
// reads into a buffer one byte longer than the piece and copies from there.
#define BOUNCE 32768
static uint8_t bounce[BOUNCE + 1];

static int32_t pack_rd(void *ud, int n, uint32_t off, void *dst, uint32_t len)
{
    (void)ud;
    if (n < 0 || n >= 8) {
        return -1;
    }
    uint32_t done = 0;
    while (done < len) {
        uint32_t k = len - done < BOUNCE ? len - done : BOUNCE;
        int32_t r = fs_read_at(pack_files[n], (int32_t)(off + done), (int32_t)k, bounce, sizeof bounce);
        if (r < 0) {
            return -1;
        }
        uint32_t got = (uint32_t)r < k ? (uint32_t)r : k;
        memcpy((uint8_t *)dst + done, bounce, got);
        done += got;
        if (got < k) {
            break;                      // the end of the file
        }
    }
    return (int32_t)done;
}

static int32_t card_rd(void *ud, uint32_t off, uint8_t *dst, uint32_t len)
{
    (void)ud;
    return files_read(sd_name[cur.idx], off, dst, len);
}

static int is_rom(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot || strchr(name, '/')) {
        return 0;
    }
    char e[5] = {0};
    for (int i = 0; i < 4 && dot[1 + i]; i++) {
        e[i] = (char)(dot[1 + i] | 0x20);
    }
    return !strcmp(e, "gb") || !strcmp(e, "gbc") || !strcmp(e, "cgb");
}

static void found(void *user, const char *name, int32_t size)
{
    (void)user;
    if (n_sd < MAX_SD && is_rom(name) && strlen(name) < NAME_MAX_ && size >= 0x8000) {
        snprintf(sd_name[n_sd], NAME_MAX_, "%s", name);
        sd_size[n_sd++] = size;
    }
}

// ---------------------------------------------------------------------------
// Saves: the cartridge's battery RAM in save/<key>.sav, snapshots in
// save/<key>.st and save/resume.st (the game you left last).

static void path_of(char *out, size_t cap, const char *key, const char *ext)
{
    snprintf(out, cap, "save/%s.%s", key, ext);
}

static uint32_t fnv(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static int write_file(const char *name, const uint8_t *p, uint32_t n)
{
    files_remove(name);                    // a shorter file must not keep an old tail
    for (uint32_t off = 0; off < n; off += 65536) {
        uint32_t k = n - off < 65536 ? n - off : 65536;
        if (files_write(name, off, p + off, k)) {
            return -1;
        }
    }
    return 0;
}

static void save_battery(int quiet)
{
    uint32_t size;
    uint8_t *ram = emu_cart_ram(&size);
    if (!size || fnv(ram, size) == ram_sum) {
        return;
    }
    char p[80];
    path_of(p, sizeof p, cur.key, "sav");
    if (write_file(p, ram, size)) {
        if (!quiet) {
            ui_toast("Couldn't save: the card's save folder is full");
        }
        return;
    }
    ram_sum = fnv(ram, size);
}

static void load_battery(void)
{
    uint32_t size;
    uint8_t *ram = emu_cart_ram(&size);
    if (size) {
        char p[80];
        path_of(p, sizeof p, cur.key, "sav");
        if (files_size(p) == (int32_t)size) {
            files_read(p, 0, ram, size);
        }
    }
    ram_sum = fnv(ram, size);
}

static int save_state(const char *name)
{
    uint32_t n = emu_state_size();
    uint8_t *buf = malloc(n);
    if (!buf) {
        return -1;
    }
    int r = emu_state_save(buf, n, cur.crc) == n ? write_file(name, buf, n) : -1;
    free(buf);
    return r;
}

static int load_state(const char *name)
{
    int32_t n = files_size(name);
    if (n <= 0 || (uint32_t)n != emu_state_size()) {
        return -1;
    }
    uint8_t *buf = malloc((size_t)n);
    if (!buf) {
        return -1;
    }
    int r = -1;
    int32_t got = 0;
    while (got < n) {
        int32_t k = files_read(name, (uint32_t)got, buf + got, (uint32_t)(n - got > 262144 ? 262144 : n - got));
        if (k <= 0) {
            break;
        }
        got += k;
    }
    if (got == n) {
        r = emu_state_load(buf, (uint32_t)n, cur.crc);
    }
    free(buf);
    return r;
}

static void remember_resume(void)
{
    if (save_state("save/resume.st") == 0) {
        char k[NAME_MAX_ + 4];
        snprintf(k, sizeof k, "%c:%s", cur.sd ? 's' : 'b', cur.sd ? sd_name[cur.idx] : games[cur.idx].id);
        storage_set_str("resume", k);
    }
}

// ---------------------------------------------------------------------------
// LEDs: a power light on the right edge, and the six on top glow with the
// colours along the top of the picture.

static void led_glow(const uint16_t *px)
{
    for (int z = 0; z < 6; z++) {
        uint32_t r = 0, g = 0, b = 0, n = 0;
        for (int y = 0; y < 48; y += 4) {
            for (int x = z * 26 + 2; x < z * 26 + 26; x += 3) {
                uint16_t be = px[y * EMU_W + x];
                uint16_t c = (uint16_t)((be >> 8) | (be << 8));
                r += (c >> 11) << 3;
                g += ((c >> 5) & 63) << 2;
                b += (c & 31) << 3;
                n++;
            }
        }
        led_set(z, (int)(r / n), (int)(g / n), (int)(b / n));
    }
}

static void leds_game(void)
{
    led_off();
    led_set(6, 255, 0, 0);
}

// ---------------------------------------------------------------------------
// Starting a game

static void fail_screen(const char *msg)
{
    free(rom);
    rom = NULL;
    free(cache);
    cache = NULL;
    clear_screen(COL_BG);
    led_off();
    sys_tick(30);
    text(0, 70, msg, &(ui_label_opts){.size = 14, .w = UI_W, .align = UI_CENTER});
    ui_hint("CANCEL back to the list");
    state = ST_ERROR;
}

static void storage_key(char *out, size_t cap, const char *what)
{
    // storage keys are short: a prefix and the game's key, cut to fit
    snprintf(out, cap, "%s%.40s", what, cur.key);
}

static void apply_palette(void)
{
    emu_set_palette(palette);
}

static void begin_run(void)
{
    int e;
    if (cur.paged) {
        e = emu_open_paged(rom_size, card_rd, NULL, cache, (uint32_t)(load_pos));
    } else {
        e = emu_open_mem(rom, rom_size);
    }
    if (e) {
        fail_screen(e == EMU_BAD_HEADER ? "That isn't a Game Boy game (bad header)."
                                        : "This cartridge type isn't supported.");
        return;
    }
    char k[64];
    storage_key(k, sizeof k, "p_");
    palette = storage_get_int(k, cur.sd ? 0 : games[cur.idx].palette);
    apply_palette();
    load_battery();
    emu_set_time(sys_time());
    if (resume_after_load) {
        resume_after_load = 0;
        if (load_state("save/resume.st")) {
            ui_toast("Couldn't continue: starting the game fresh");
        }
    }
    clear_screen(COL_BLACK);
    leds_game();
    acc_us = chord_ms = keys_before = save_check_ms = 0;
    start_frames = select_frames = shown_count = 0;
    stat_t0 = now_us();
    stat_frames = stat_shown = stat_emu_us = 0;
    sys_tick(16);
    state = ST_RUN;
}


static void start_loading(int sd, int idx, int resume)
{
    memset(&cur, 0, sizeof cur);
    cur.sd = sd;
    cur.idx = idx;
    resume_after_load = resume;
    free(rom);
    rom = NULL;
    free(cache);
    cache = NULL;
    if (!sd) {
        const pack_game *g = &games[idx];
        snprintf(cur.key, sizeof cur.key, "%s", g->id);
        snprintf(cur.title, sizeof cur.title, "%s", g->title);
        rom_size = (uint32_t)g->banks * EMU_BANK;
        cur.crc = g->crc;
    } else {
        snprintf(cur.title, sizeof cur.title, "%s", sd_name[idx]);
        char *dot = strrchr(cur.title, '.');
        if (dot) {
            *dot = 0;
        }
        snprintf(cur.key, sizeof cur.key, "%.50s", cur.title);
        rom_size = (uint32_t)sd_size[idx];
        if (rom_size > EMU_MAX_BANKS * EMU_BANK) {
            fail_screen("Bigger than any Game Boy cartridge.");
            return;
        }
        cur.paged = rom_size > WHOLE_MAX;
    }
    if (cur.paged) {
        uint32_t banks = (rom_size + EMU_BANK - 1) / EMU_BANK, slots = banks < 128 ? banks : 128;
        while (slots >= 8 && !(cache = malloc((size_t)slots * EMU_BANK))) {
            slots = slots * 3 / 4;
        }
        if (!cache) {
            fail_screen("Not enough memory for this game.");
            return;
        }
        load_pos = slots;              // begin_run passes it on as the number of slots
        uint8_t head[EMU_BANK];
        int32_t n = files_read(sd_name[idx], 0, head, EMU_BANK);
        cur.crc = emu_crc32(head, n > 0 ? (uint32_t)n : 0, rom_size);
        begin_run();
        return;
    }
    // Whole in memory: a card game is padded to a power of two, as the bus wraps.
    uint32_t cap = 0x8000;
    while (cap < rom_size) {
        cap <<= 1;
    }
    rom = malloc(cap);
    if (!rom) {
        fail_screen("Not enough memory for this game.");
        return;
    }
    memset(rom, 0xFF, cap);
    if (sd) {
        rom_size = cap;
    }
    load_pos = 0;
    load_crc = 0;
    clear_screen(COL_BG);
    text(0, 80, cur.title, &(ui_label_opts){.size = 20, .w = UI_W, .align = UI_CENTER});
    load_bar = ui_bar(60, 116, 200, 8, 0);
    led_off();
    state = ST_LOAD;
}

static void load_tick(void)
{
    uint32_t t0 = now_us();
    if (!cur.sd) {
        const pack_game *g = &games[cur.idx];
        while (load_pos < g->banks && now_us() - t0 < LOAD_BUDGET_US) {
            if (pack_unpack(g, load_pos, load_pos + 1, rom, pack_rd, NULL)) {
                fail_screen("The game's data is damaged. Reinstall the app.");
                return;
            }
            load_crc = emu_crc32(rom + (size_t)load_pos * EMU_BANK, EMU_BANK, load_crc);
            load_pos++;
        }
        ui_bar_set(load_bar, (int)(load_pos * 100 / g->banks));
        if (load_pos >= g->banks) {
            if (load_crc != g->crc) {
                fail_screen("The game's data is damaged. Reinstall the app.");
                return;
            }
            begin_run();
        }
        return;
    }
    uint32_t real = (uint32_t)sd_size[cur.idx];
    while (load_pos < real && now_us() - t0 < LOAD_BUDGET_US) {
        uint32_t k = real - load_pos < 262144 ? real - load_pos : 262144;
        int32_t n = files_read(sd_name[cur.idx], load_pos, rom + load_pos, k);
        if (n <= 0) {
            fail_screen("Couldn't read the game from the card.");
            return;
        }
        load_pos += (uint32_t)n;
    }
    ui_bar_set(load_bar, (int)((uint64_t)load_pos * 100 / real));
    if (load_pos >= real) {
        cur.crc = emu_crc32(rom, EMU_BANK, rom_size);
        begin_run();
    }
}

// ---------------------------------------------------------------------------
// Running

static uint8_t joypad(uint32_t k)
{
    int s = KEY_HELD(k, KEY_SELECT), c = KEY_HELD(k, KEY_BACK);
    if (s && c) {
        s = c = 0;                      // the chord is START (or the menu), not A + B
    }
    int a = swap_ab ? c : s, b = swap_ab ? s : c;
    uint8_t p = 0;                      // bits: A B SELECT START RIGHT LEFT UP DOWN
    p |= a ? 0x01 : 0;
    p |= b ? 0x02 : 0;
    p |= select_frames ? 0x04 : 0;
    p |= start_frames ? 0x08 : 0;
    p |= KEY_HELD(k, KEY_RIGHT) ? 0x10 : 0;
    p |= KEY_HELD(k, KEY_LEFT) ? 0x20 : 0;
    p |= KEY_HELD(k, KEY_UP) ? 0x40 : 0;
    p |= KEY_HELD(k, KEY_DOWN) ? 0x80 : 0;
    return (uint8_t)~p;                 // the Game Boy's are active low
}

static void show_frame(const uint16_t *px)
{
    int r = big ? ui_frame(px, EMU_W, EMU_H, 40, 1, 240, 216, UI_FRAME_BE)
                : ui_frame(px, EMU_W, EMU_H, 80, 37, EMU_W, EMU_H, UI_FRAME_BE);
    stat_shown += r > 0;
    if (glow && ++shown_count % 8 == 0) {
        led_glow(px);
    }
}

static void run_tick(int32_t ms)
{
    uint32_t k = sys_keys();
    int chord = KEY_HELD(k, KEY_SELECT) && KEY_HELD(k, KEY_BACK);
    int was = KEY_HELD(keys_before, KEY_SELECT) && KEY_HELD(keys_before, KEY_BACK);
    if (chord) {
        chord_ms += (uint32_t)ms;
        if (chord_ms >= 1000) {
            chord_ms = 0;
            keys_before = k;
            open_menu();
            return;
        }
    } else if (was && chord_ms) {
        start_frames = 6;               // a tap of both: START
        chord_ms = 0;
    }
    keys_before = k;

    acc_us += (uint32_t)ms * 1000;
    if (acc_us > 4 * FRAME_US) {
        acc_us = 4 * FRAME_US;          // behind after a pause: don't race to catch up
    }
    uint32_t t = now_us();
    int n = 0;
    while (acc_us >= FRAME_US && n < 3) {
        acc_us -= FRAME_US;
        int last = acc_us < FRAME_US || n == 2;
        emu_frame(joypad(k), last ? fb[fbi] : NULL);   // only the frame shown is drawn
        start_frames -= start_frames > 0;
        select_frames -= select_frames > 0;
        n++;
        if (emu_error()) {
            char m[80];
            snprintf(m, sizeof m, "The game stopped (error %d at %04X).", emu_error(), (unsigned)emu_error_addr());
            save_battery(1);
            fail_screen(m);
            return;
        }
        if (last) {
            show_frame(fb[fbi]);
            fbi ^= 1;
        }
    }
    stat_emu_us += now_us() - t;
    stat_frames += (uint32_t)n;
    save_check_ms += (uint32_t)ms;
    if (save_check_ms > 30000) {
        save_check_ms = 0;
        save_battery(0);
    }
    uint32_t el = now_us() - stat_t0;
    if (el >= 5000000) {
        char line[120];
        snprintf(line, sizeof line, "PASTAT %s fps=%u shown=%u emu_ms=%u misses=%u", cur.key,
                 (unsigned)(stat_frames * 1000000ull / el), (unsigned)(stat_shown * 1000000ull / el),
                 (unsigned)(stat_frames ? stat_emu_us / 1000 / stat_frames : 0), (unsigned)emu_misses());
        badge_print(line);
        stat_t0 = now_us();
        stat_frames = stat_shown = stat_emu_us = 0;
    }
}

static void leave_game(int to_list)
{
    save_battery(1);
    remember_resume();
    free(rom);
    rom = NULL;
    free(cache);
    cache = NULL;
    led_off();
    if (to_list) {
        show_list();
    }
}

// ---------------------------------------------------------------------------
// The menu (SELECT + CANCEL held a second)

enum { M_RESUME, M_START, M_SELECT, M_SAVE, M_LOAD, M_COLOURS, M_SCREEN, M_GLOW, M_SWAP, M_RESET, M_LIST, M_N };
static int menu_ids[M_N], menu_n, menu_pick = -1;
static char menu_right[M_N][24];
static ui_row menu_rows[M_N];

static void menu_fill(void)
{
    static const char *const titles[M_N] = {"Back to the game", "Press START", "Press SELECT", "Save state",
                                            "Load state", "Colours", "Screen", "LED glow", "Swap A and B",
                                            "Restart the game", "Save and go to the list"};
    menu_n = 0;
    for (int i = 0; i < M_N; i++) {
        if (i == M_COLOURS && emu_is_cgb()) {
            continue;                   // Color games bring their own colours
        }
        menu_right[i][0] = 0;
        if (i == M_COLOURS) {
            snprintf(menu_right[i], sizeof menu_right[i], "%s", emu_palettes[palette].name);
        } else if (i == M_SCREEN) {
            snprintf(menu_right[i], sizeof menu_right[i], "%s", big ? "1.5x" : "1x (faster)");
        } else if (i == M_GLOW) {
            snprintf(menu_right[i], sizeof menu_right[i], "%s", glow ? "on" : "off");
        } else if (i == M_SWAP) {
            snprintf(menu_right[i], sizeof menu_right[i], "%s", swap_ab ? "on" : "off");
        }
        menu_rows[menu_n] = (ui_row){titles[i], NULL, menu_right[i]};
        menu_ids[menu_n++] = i;
    }
}

static void menu_selected(void *user, int index)
{
    (void)user;
    menu_pick = index;
}

static void open_menu(void)
{
    state = ST_MENU;
    save_battery(1);
    clear_screen(COL_BG);
    menu_fill();
    menu_list = ui_list_rows(0, 0, UI_W, UI_H - 16, menu_rows, menu_n, menu_selected, NULL);
    ui_hint("SELECT choose   CANCEL back to the game");
    sys_tick(30);
}

static void back_to_game(void)
{
    ui_obj_remove(&menu_list);
    clear_screen(COL_BLACK);
    acc_us = 0;
    keys_before = sys_keys();
    chord_ms = 0;
    sys_tick(16);
    state = ST_RUN;
}

static void menu_refresh(int row)
{
    menu_fill();
    ui_list_set_rows(menu_list, menu_rows, menu_n);
    ui_list_select(menu_list, row);
}

static void menu_do(int row)
{
    char p[80];
    switch (menu_ids[row]) {
    case M_RESUME:
        back_to_game();
        break;
    case M_START:
        back_to_game();
        start_frames = 6;
        break;
    case M_SELECT:
        back_to_game();
        select_frames = 6;
        break;
    case M_SAVE:
        path_of(p, sizeof p, cur.key, "st");
        ui_toast(save_state(p) ? "Couldn't save: the card's save folder is full" : "Saved");
        break;
    case M_LOAD:
        path_of(p, sizeof p, cur.key, "st");
        if (load_state(p)) {
            ui_toast("No saved state for this game yet");
        } else {
            back_to_game();
        }
        break;
    case M_COLOURS: {
        palette = (palette + 1) % emu_n_palettes;
        apply_palette();
        char k[64];
        storage_key(k, sizeof k, "p_");
        storage_set_int(k, palette);
        menu_refresh(row);
        break;
    }
    case M_SCREEN:
        big = !big;
        storage_set_int("big", big);
        menu_refresh(row);
        break;
    case M_GLOW:
        glow = !glow;
        storage_set_int("glow", glow);
        if (!glow) {
            leds_game();
        }
        menu_refresh(row);
        break;
    case M_SWAP:
        swap_ab = !swap_ab;
        storage_set_int("swap", swap_ab);
        menu_refresh(row);
        break;
    case M_RESET:
        save_battery(1);
        if (cur.paged ? emu_open_paged(rom_size, card_rd, NULL, cache, load_pos) : emu_open_mem(rom, rom_size)) {
            fail_screen("Couldn't restart the game.");
            return;
        }
        apply_palette();
        load_battery();
        emu_set_time(sys_time());
        back_to_game();
        break;
    case M_LIST:
        leave_game(1);
        break;
    }
}

// ---------------------------------------------------------------------------
// The game list: games on the left, a picture of the selected one on the
// right, who made it and what it is below.

enum { R_CONTINUE, R_GAME, R_SD, R_HELP, R_ABOUT };
typedef struct {
    uint8_t kind;
    int16_t idx;
} row_t;
static row_t rows[MAX_ROWS];
static ui_row list_rows[MAX_ROWS];
static char row_sub[MAX_ROWS][28];
static char continue_title[NAME_MAX_ + 12];
static int n_rows, list_pick = -1, shown_row = -1;
static int resume_sd, resume_idx = -1;

static int find_resume(void)
{
    char k[NAME_MAX_ + 4];
    resume_idx = -1;
    if (storage_get_str("resume", k, sizeof k) < 2 || files_size("save/resume.st") <= 0) {
        return 0;
    }
    if (k[0] == 'b') {
        for (int i = 0; i < n_games; i++) {
            if (!strcmp(games[i].id, k + 2)) {
                resume_sd = 0;
                resume_idx = i;
            }
        }
    } else {
        for (int i = 0; i < n_sd; i++) {
            if (!strcmp(sd_name[i], k + 2)) {
                resume_sd = 1;
                resume_idx = i;
            }
        }
    }
    return resume_idx >= 0;
}

static void wrap(const char *text, char lines[][24], int max_lines, int width)
{
    for (int i = 0; i < max_lines; i++) {
        lines[i][0] = 0;
    }
    int line = 0;
    const char *p = text;
    while (*p && line < max_lines) {
        while (*p == ' ') {
            p++;
        }
        int len = (int)strlen(p);
        int take = len <= width ? len : width;
        if (len > width) {
            int cut = take;
            while (cut > 0 && p[cut] != ' ') {
                cut--;
            }
            if (cut > 0) {
                take = cut;
            }
        }
        snprintf(lines[line], 24, "%.*s", take, p);
        p += take;
        line++;
    }
}

static void cartridge_picture(uint16_t *px, uint32_t tint)
{
    // a plain drawn cartridge for games without a preview
    uint16_t bg = RGB565_BE(RGB565_HEX(0x14141C)), body = RGB565_BE(RGB565_HEX(tint)),
             label = RGB565_BE(RGB565_HEX(0xE8E8F0)), dark = RGB565_BE(RGB565_HEX(0x2A2A36));
    for (int y = 0; y < EMU_H; y++) {
        for (int x = 0; x < EMU_W; x++) {
            uint16_t c = bg;
            if (x >= 44 && x < 116 && y >= 20 && y < 128) {
                c = body;
                if (x >= 54 && x < 106 && y >= 36 && y < 86) {
                    c = label;
                }
                if (y >= 116 && (x - 44) % 6 < 3) {
                    c = dark;
                }
                if (x >= 100 && y < 30) {
                    c = bg;              // the cut corner
                }
            }
            px[y * EMU_W + x] = c;
        }
    }
}

static void show_info(int r)
{
    if (r < 0 || r >= n_rows || r == shown_row) {
        return;
    }
    shown_row = r;
    char lines[4][24];
    const char *top = "";
    uint16_t *px = fb[fbi];
    fbi ^= 1;
    row_t row = rows[r];
    int gi = row.kind == R_GAME ? row.idx : (row.kind == R_CONTINUE && !resume_sd) ? resume_idx : -1;
    if (gi >= 0) {
        const pack_game *g = &games[gi];
        if (pack_preview(g, px, pack_rd, NULL)) {
            cartridge_picture(px, 0x6B2FD9);
        }
        top = g->author;
        wrap(g->blurb, lines, 3, 22);
        if (row.kind == R_CONTINUE) {
            snprintf(lines[2], 24, "%s", "SELECT to carry on.");
        }
    } else if (row.kind == R_SD || row.kind == R_CONTINUE) {
        cartridge_picture(px, 0x14F195);
        top = "Your game, from the card";
        wrap("A game you copied to the microSD card.", lines, 3, 22);
    } else if (row.kind == R_HELP) {
        cartridge_picture(px, 0xFFB45C);
        top = "Keys and your own games";
        wrap("How to play, and how to add games you own.", lines, 3, 22);
    } else {
        cartridge_picture(px, 0x4E6CC8);
        top = "Credits and licences";
        wrap("Every game here is free software. Thank you to their makers.", lines, 3, 22);
    }
    ui_frame(px, EMU_W, EMU_H, 160, 0, EMU_W, EMU_H, UI_FRAME_BE);
    ui_label_text(info[0], top);
    for (int i = 0; i < 3; i++) {
        ui_label_text(info[1 + i], lines[i]);
    }
}

static void list_selected(void *user, int index)
{
    (void)user;
    list_pick = index;
}

static void show_list(void)
{
    state = ST_LIST;
    sys_tick(30);
    led_off();
    clear_screen(COL_BG);
    n_rows = 0;
    if (find_resume()) {
        snprintf(continue_title, sizeof continue_title, "> %s",
                 resume_sd ? sd_name[resume_idx] : games[resume_idx].title);
        rows[n_rows] = (row_t){R_CONTINUE, 0};
        list_rows[n_rows] = (ui_row){continue_title, "Continue where you left", NULL};
        n_rows++;
    }
    for (int i = 0; i < n_games; i++) {
        rows[n_rows] = (row_t){R_GAME, (int16_t)i};
        list_rows[n_rows] = (ui_row){games[i].title, games[i].genre, NULL};
        n_rows++;
    }
    for (int i = 0; i < n_sd; i++) {
        rows[n_rows] = (row_t){R_SD, (int16_t)i};
        snprintf(row_sub[n_rows], sizeof row_sub[0], "Your game, %d KB", (int)(sd_size[i] / 1024));
        list_rows[n_rows] = (ui_row){sd_name[i], row_sub[n_rows], NULL};
        n_rows++;
    }
    rows[n_rows] = (row_t){R_HELP, 0};
    list_rows[n_rows++] = (ui_row){"How to play", "Keys, your own games", NULL};
    rows[n_rows] = (row_t){R_ABOUT, 0};
    list_rows[n_rows++] = (ui_row){"Credits", "Who made the games", NULL};

    game_list = ui_list_rows(0, 0, 158, UI_H - 16, list_rows, n_rows, list_selected, NULL);
    info[0] = ui_label(162, 147, "", &(ui_label_opts){.size = 12, .color = COL_DIM});
    for (int i = 0; i < 3; i++) {
        info[1 + i] = ui_label(162, 161 + i * 13, "", &(ui_label_opts){.size = 12});
    }
    ui_hint("SELECT play   CANCEL quit");
    int start = storage_get_int("row", 0);
    if (rows[0].kind == R_CONTINUE) {
        start = 0;
    }
    if (start < 0 || start >= n_rows) {
        start = 0;
    }
    ui_list_select(game_list, start);
    shown_row = -1;
    show_info(start);
}

// ---------------------------------------------------------------------------
// Pages: how to play, credits

static ui_row credit_rows[PACK_MAX_GAMES + 3];
static int credit_pick = -1;

static void credit_selected(void *user, int index)
{
    (void)user;
    credit_pick = index;
}

static void show_help(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    static const char *const lines[] = {
        "D-pad          D-pad",
        "SELECT         A",
        "CANCEL         B",
        "SELECT+CANCEL  START (tap both)",
        "hold both 1 s  menu: SELECT, save,",
        "               colours, screen, LEDs",
        "HOME           leave (game is kept)",
    };
    text(8, 4, "How to play", &(ui_label_opts){.size = 20, .color = COL_PURPLE});
    for (int i = 0; i < 7; i++) {
        text(8, 32 + i * 15, lines[i], &(ui_label_opts){.size = 12});
    }
    char dir[64], msg[160];
    if (files_dir(dir, sizeof dir) < 0) {
        snprintf(dir, sizeof dir, "hh/files/pocket-arcade/");
    }
    snprintf(msg, sizeof msg, "Your own games: copy .gb or .gbc\nfiles you own to %s\non the microSD card.", dir);
    text(8, 144, msg, &(ui_label_opts){.size = 12, .color = COL_DIM});
    ui_hint("CANCEL back");
    storage_set_int("seen", 1);
}

static void show_credits(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    static char rights[PACK_MAX_GAMES][24];
    int n = 0;
    for (int i = 0; i < n_games; i++) {
        snprintf(rights[i], sizeof rights[i], "%.20s", games[i].license);
        credit_rows[n++] = (ui_row){games[i].title, games[i].author, rights[i]};
    }
    credit_rows[n++] = (ui_row){"Walnut-CGB emulator", "Mr-PauI, Peanut-GB, SameBoy", "MIT"};
    credit_rows[n++] = (ui_row){"tinf decompressor", "Joergen Ibsen", "zlib"};
    credit_rows[n++] = (ui_row){"Pocket Arcade", "@hypnogaba", "MIT"};
    credit_list = ui_list_rows(0, 0, UI_W, UI_H - 16, credit_rows, n, credit_selected, NULL);
    ui_hint("SELECT where to get it   CANCEL back");
}

static void credit_do(int i)
{
    if (i >= 0 && i < n_games) {
        char m[96];
        snprintf(m, sizeof m, "Source: %s", games[i].source);
        ui_toast(m);
    } else if (i == n_games) {
        ui_toast("github.com/Mr-PauI/Walnut-CGB");
    } else if (i == n_games + 1) {
        ui_toast("github.com/jibsen/tinf");
    } else {
        ui_toast("Game Boy is a trademark of Nintendo. Not made or endorsed by Nintendo.");
    }
}

// ---------------------------------------------------------------------------
// Lifecycle

void on_start(void)
{
    big = storage_get_int("big", 0);
    glow = storage_get_int("glow", 1);
    swap_ab = storage_get_int("swap", 0);
    int32_t n = fs_read("games/index.bin", index_buf, sizeof index_buf);
    n_games = n > 0 ? pack_index(index_buf, (uint32_t)n, games, PACK_MAX_GAMES) : -1;
    if (n_games < 0) {
        n_games = 0;
        badge_print("Pocket Arcade: games/index.bin is missing or damaged");
    }
    files_list(found, NULL);
    if (!storage_get_int("seen", 0)) {
        show_help();
        return;
    }
    show_list();
}

void on_tick(int32_t ms)
{
    switch (state) {
    case ST_LIST:
        if (list_pick >= 0) {
            int r = list_pick;
            list_pick = -1;
            storage_set_int("row", r);
            row_t row = rows[r];
            if (row.kind == R_GAME) {
                start_loading(0, row.idx, 0);
            } else if (row.kind == R_SD) {
                start_loading(1, row.idx, 0);
            } else if (row.kind == R_CONTINUE) {
                start_loading(resume_sd, resume_idx, 1);
            } else if (row.kind == R_HELP) {
                show_help();
            } else {
                show_credits();
            }
            return;
        }
        show_info(ui_list_selected(game_list));
        break;
    case ST_LOAD:
        load_tick();
        break;
    case ST_RUN:
        run_tick(ms);
        break;
    case ST_MENU:
        if (menu_pick >= 0) {
            int r = menu_pick;
            menu_pick = -1;
            menu_do(r);
        }
        break;
    case ST_PAGE:
        if (credit_pick >= 0) {
            int r = credit_pick;
            credit_pick = -1;
            credit_do(r);
        }
        break;
    }
}

int32_t on_key(int32_t key, int32_t repeat)
{
    (void)repeat;
    if (key != KEY_BACK) {
        return state == ST_RUN;           // in a game every key is the game's
    }
    switch (state) {
    case ST_RUN:
        return 1;                         // CANCEL is B
    case ST_MENU:
        back_to_game();
        return 1;
    case ST_PAGE:
    case ST_ERROR:
        show_list();
        return 1;
    case ST_LOAD:
        free(rom);
        rom = NULL;
        show_list();
        return 1;
    default:
        return 0;                         // the list: CANCEL leaves
    }
}

void on_exit(void)
{
    if (state == ST_RUN || state == ST_MENU) {
        leave_game(0);
    }
    led_off();
}
