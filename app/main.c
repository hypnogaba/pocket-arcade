// Pocket Arcade: a Game Boy and Game Boy Color player for the Solana BP26
// badge, with 16 free games inside, Tank Siege (tanks.c, written for the
// badge) and room for your own on the card.
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
#include "tanks.h"

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

static uint8_t index_buf[8 + PACK_MAX_GAMES * PACK_REC + 1];   // + the NUL fs_read() adds
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
    int native;              // a game written for the badge (tanks.c), not a cartridge
} cur;

static uint8_t *rom, *cache;
static uint32_t rom_size, load_pos, load_crc;
static int resume_after_load;

static uint16_t fb[2][EMU_W * EMU_H] __attribute__((aligned(64)));
static int fbi;

// Settings (storage)
static int big, glow = 1, swap_ab, palette;
static int page_about, page_board;      // which page is up (SELECT means credits / refresh there)
static int board_send(void);

// Running
static uint32_t acc_us, chord_ms, keys_before, save_check_ms;
static int start_frames, select_frames, shown_count, chord_lock;
// SELECT + CANCEL as one key: pressed together (within CHORD_TOGETHER_MS of
// each other) they are START, held a second the menu. Pressed one after the
// other (hold B to run, then A to jump) they stay A + B for the game. A key
// that drops out for a tick or two doesn't end the chord.
#define CHORD_TOGETHER_MS 150
#define CHORD_GAP_MS 80
static uint32_t run_ms, sel_at, back_at, chord_gap;
static int chord_on, chord_lit;    // chord_lit: the LEDs show the hold, not the picture
static uint32_t stat_t0, stat_frames, stat_shown, stat_emu_us;
static uint32_t ram_sum, ram_seen, save_quiet_s;

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
#define BOARD_ROWS 9
static ui_obj game_list, menu_list, credit_list, set_list, load_bar, info[4], board_lbl[BOARD_ROWS + 1];

static void forget_ui(void)
{
    ui_obj *keep[] = {&game_list, &menu_list, &credit_list, &set_list, &load_bar, &info[0], &info[1], &info[2], &info[3]};
    for (unsigned i = 0; i < sizeof keep / sizeof keep[0]; i++) {
        if (keep[i]->ref) {
            badge_obj_free(keep[i]);
        }
    }
    for (int i = 0; i <= BOARD_ROWS; i++) {
        if (board_lbl[i].ref) {
            badge_obj_free(&board_lbl[i]);
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

// A card game's saves are named after its whole file name (Zelda.gb and
// Zelda.gbc stay apart); a long name is cut and gets a hash, and the end may
// not be a space or a dot (the files API refuses those).
static uint32_t fnv(const uint8_t *p, uint32_t n);
static void card_key(char *out, size_t cap, const char *name)
{
    size_t len = strlen(name);
    if (len <= 44) {
        snprintf(out, cap, "%s", name);
    } else {
        snprintf(out, cap, "%.39s~%04x", name, (unsigned)(fnv((const uint8_t *)name, (uint32_t)len) & 0xFFFF));
    }
    for (size_t n = strlen(out); n > 0 && (out[n - 1] == ' ' || out[n - 1] == '.'); n--) {
        out[n - 1] = 0;
    }
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
    // Same size: overwrite in place, so a failed write never leaves the
    // player with no file. Another size: a shorter file must not keep an old
    // tail, so the old one goes first (only resume.st changes size).
    if (files_size(name) != (int32_t)n) {
        files_remove(name);
    }
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

// ---------------------------------------------------------------------------
// Secrets: the list's "Secrets" page has a hint for each (where, and how it
// starts) and, once found, how it was done. Each one found opens one more
// colour set for Game Boy games; all five turn the Tank Siege tank Solana purple.

enum { S_IBRL, S_TIME, S_COLLECT, S_FLAWLESS, S_STARS, S_N };
static const struct {
    const char *name, *hint, *how;
} secrets[S_N] = {
    {"IBRL MODE", "Game list: up up down down, then...", "UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT"},
    {"TIME TRAVEL", "Game menu: save a state, then load it.", "Save state, then Load state"},
    {"COLLECTOR", "Game list: play 5 different games.", "5 different games started"},
    {"FLAWLESS", "Tank Siege: clear a stage, lose no tank.", "a stage cleared without a hit"},
    {"STEEL SHOTS", "Tank Siege: take 3 stars in one life.", "3 stars: shots that break steel"},
};
static int secret_bits;                 // storage "sec"

static int secrets_found(void)
{
    int n = 0;
    for (int i = 0; i < S_N; i++) {
        n += (secret_bits >> i) & 1;
    }
    return n;
}

static int palettes_open(void)
{
    int n = EMU_BASE_PALETTES + secrets_found();
    return n < emu_n_palettes ? n : emu_n_palettes;
}

static void find_secret(int s)
{
    if (s < 0 || s >= S_N || (secret_bits >> s) & 1) {
        return;
    }
    secret_bits |= 1 << s;
    storage_set_int("sec", secret_bits);
    int n = secrets_found();
    const char *colours = emu_palettes[palettes_open() - 1].name;
    char m[120];
    if (n == S_N) {
        tk_set_skin(1);
        snprintf(m, sizeof m, "ALL 5 SECRETS! New colours: %s. Tank Siege: a Solana tank.", colours);
    } else {
        snprintf(m, sizeof m, "SECRET %d/%d: %s! New colours: %s (game menu)", n, S_N, secrets[s].name, colours);
    }
    ui_toast(m);
}

// Tank Siege, watched from outside: FLAWLESS, STEEL SHOTS and the best score
// for the share screen. Called after the frames of a tick.
static int ts_mode = -1, ts_lives, ts_hit;

static void tank_watch(void)
{
    int m = tk_mode(), l = tk_lives();
    if (m == TK_PLAY) {
        if (ts_mode != TK_PLAY) {
            ts_lives = l;               // a stage (or a resumed one) starts
            ts_hit = 0;
        }
        if (l < ts_lives) {
            ts_hit = 1;
        }
        ts_lives = l;
        if (tk_level() >= 3) {
            find_secret(S_STARS);
        }
    }
    if (m != ts_mode) {
        if (m == TK_CLEAR && ts_mode == TK_PLAY && !ts_hit) {
            find_secret(S_FLAWLESS);
        }
        int over = m == TK_OVER && ts_mode != TK_OVER;
        uint32_t hi = tk_hi();
        if (hi > (uint32_t)storage_get_int("ts_hi", 0) && hi < 0x7FFFFFFF) {
            storage_set_int("ts_hi", (int32_t)hi);
        }
        int far = tk_furthest();
        if (far > storage_get_int("ts_st", 1)) {
            storage_set_int("ts_st", far);
        }
        if (over) {
            board_send();               // a new best goes on the leaderboard (when online)
        }
        ts_mode = m;
    }
}

// Original Game Boy games are four shades of one colour; each bundled game
// has its own set in the pack (tools/games.json "palette", its preview drawn
// in it) so the list isn't all green. The menu changes it.
static int default_palette(const pack_game *g)
{
    return g->palette ? g->palette : 2;     // Solana
}

static void apply_palette(void)
{
    if (palette < 0 || palette >= palettes_open()) {
        palette = 0;                    // a secret colour set from another badge's storage, say
    }
    emu_set_palette(palette);
}

static int open_game(void)
{
    if (cur.native) {
        return emu_open_native(&tk_machine);
    }
    emu_force_dmg(!cur.sd && (games[cur.idx].flags & PACK_DMG));
    return cur.paged ? emu_open_paged(rom_size, card_rd, NULL, cache, load_pos) : emu_open_mem(rom, rom_size);
}

static void begin_run(void)
{
    int e = open_game();
    if (e) {
        fail_screen(e == EMU_BAD_HEADER ? "That isn't a Game Boy game (bad header)."
                                        : "This cartridge type isn't supported.");
        return;
    }
    char k[64];
    storage_key(k, sizeof k, "p_");
    palette = storage_get_int(k, cur.sd ? 2 : default_palette(&games[cur.idx]));
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
    run_ms = sel_at = back_at = chord_gap = 0;
    chord_lock = chord_on = chord_lit = 0;
    save_quiet_s = 0;
    ram_seen = ram_sum;
    start_frames = select_frames = shown_count = 0;
    stat_t0 = now_us();
    stat_frames = stat_shown = stat_emu_us = 0;
    sys_tick(16);
    state = ST_RUN;
    ui_toast("START: tap SELECT+CANCEL together. Hold both 1 s: menu, exit");
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
        int played = storage_get_int("played", 0) | (1 << (idx & 31));
        storage_set_int("played", played);
        int kinds = 0;
        for (int i = 0; i < 32; i++) {
            kinds += (played >> i) & 1;
        }
        if (kinds >= 5) {
            find_secret(S_COLLECT);
        }
        snprintf(cur.key, sizeof cur.key, "%s", g->id);
        snprintf(cur.title, sizeof cur.title, "%s", g->title);
        rom_size = (uint32_t)g->banks * EMU_BANK;
        cur.crc = g->crc;
        if (g->flags & PACK_NATIVE) {
            cur.native = 1;
            ts_mode = -1;
            begin_run();
            return;
        }
    } else {
        snprintf(cur.title, sizeof cur.title, "%s", sd_name[idx]);
        char *dot = strrchr(cur.title, '.');
        if (dot) {
            *dot = 0;
        }
        card_key(cur.key, sizeof cur.key, sd_name[idx]);
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
    (void)t0;
    if (load_pos < real) {              // one 256 KB read a tick: about 65,000 of the 200,000 host steps
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
    if (chord_lock) {
        s = c = 0;                      // a chord, until both are up: letting go one by one isn't A or B
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

// 1.5x (240 x 216, the app area's height) scaled here, nearest, into two
// buffers in turn: an unscaled big-endian frame in a 64-byte-aligned buffer
// goes to the screen with no copy, and the next ui_frame waits for the last.
// Left to the badge, a scaled frame is converted into one buffer of its own
// while the screen may still be reading the last one: the picture flickered.
#define BIG_W 240
#define BIG_H 216
static uint16_t big_fb[2][BIG_W * BIG_H] __attribute__((aligned(64)));
static int big_i;

static void scale_big(const uint16_t *s, uint16_t *d)
{
    for (int sy = 0; sy < EMU_H; sy++) {
        const uint16_t *r = s + sy * EMU_W;
        uint16_t *o = d;
        for (int sx = 0; sx < EMU_W; sx += 2) {   // pixels a b -> a a b
            uint16_t a = r[sx], b = r[sx + 1];
            o[0] = a;
            o[1] = a;
            o[2] = b;
            o += 3;
        }
        d += BIG_W;
        if (!(sy & 1)) {                // rows likewise: even ones twice
            memcpy(d, d - BIG_W, BIG_W * 2);
            d += BIG_W;
        }
    }
}

static void show_frame(const uint16_t *px)
{
    int r;
    if (big) {
        scale_big(px, big_fb[big_i]);
        r = ui_frame(big_fb[big_i], BIG_W, BIG_H, 40, 1, BIG_W, BIG_H, UI_FRAME_BE);
        big_i ^= 1;
    } else {
        r = ui_frame(px, EMU_W, EMU_H, 80, 37, EMU_W, EMU_H, UI_FRAME_BE);
    }
    stat_shown += r > 0;
    if (glow && !chord_lit && ++shown_count % 8 == 0) {
        led_glow(px);
    }
}

// While SELECT + CANCEL are held, the top LEDs fill up: all six, the menu.
static void chord_leds(uint32_t need)
{
    int n = (int)(chord_ms * 6 / need);
    chord_lit = 1;
    for (int i = 0; i < 6; i++) {
        if (i <= n) {
            led_set(i, 181, 140, 255);
        } else {
            led_set(i, 0, 0, 0);
        }
    }
}

// SELECT + CANCEL: START, the menu (1) or nothing (0). Pressed together they
// are START tapped and the menu held a second. Pressed one after the other
// (B held to run, then A to jump) the game gets A + B, and only two seconds
// of both opens the menu, so there is always a way out.
static int chord_tick(uint32_t k, int32_t ms)
{
    int s = KEY_HELD(k, KEY_SELECT), c = KEY_HELD(k, KEY_BACK);
    run_ms += (uint32_t)ms;
    if (s && !KEY_HELD(keys_before, KEY_SELECT)) {
        sel_at = run_ms;
    }
    if (c && !KEY_HELD(keys_before, KEY_BACK)) {
        back_at = run_ms;
    }
    keys_before = k;
    int menu = 0;
    if (s && c) {
        uint32_t apart = sel_at > back_at ? sel_at - back_at : back_at - sel_at;
        if (!chord_on && !chord_lock && apart <= CHORD_TOGETHER_MS) {
            chord_on = chord_lock = 1;
        }
        chord_gap = 0;
        chord_ms += (uint32_t)ms;
        uint32_t need = chord_on ? 1000 : 2000;
        if (chord_ms >= need) {
            chord_on = 0;
            chord_ms = 0;
            chord_lock = 1;             // back in the game, letting go isn't A or B
            menu = 1;
        } else if (chord_on || chord_ms >= 500) {
            chord_leds(need);
        }
    } else if (chord_ms) {
        // A key that drops out for a tick doesn't end a chord. Pressed apart
        // it ends at once: A tapped fast while B is held must not add up.
        chord_gap += (uint32_t)ms;
        if (!chord_on || chord_gap >= CHORD_GAP_MS) {
            if (chord_on) {
                start_frames = 6;       // a tap of both: START
            }
            chord_on = 0;
            chord_ms = 0;
            if (chord_lit) {
                chord_lit = 0;
                leds_game();
            }
        }
    }
    if (!s && !c) {
        chord_lock = 0;
    }
    return menu;
}

static void run_tick(int32_t ms)
{
    uint32_t k = sys_keys();
    if (chord_tick(k, ms)) {
        open_menu();
        return;
    }

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
    if (cur.native) {
        tank_watch();
    }
    // Battery save while playing: at most every 30 s, and only once the RAM
    // has held still for 2 s (games that use it as work RAM change it every
    // frame; theirs is saved from the menu, on leaving and on HOME).
    save_check_ms += (uint32_t)ms;
    if (save_check_ms >= 2000) {
        save_check_ms = 0;
        uint32_t size;
        uint8_t *ram = emu_cart_ram(&size);
        uint32_t h = size ? fnv(ram, size) : 0;
        save_quiet_s += 2;
        if (h == ram_seen && save_quiet_s >= 30) {
            save_battery(0);
            save_quiet_s = 0;
        }
        ram_seen = h;
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

enum { M_RESUME, M_LIST, M_START, M_SELECT, M_SAVE, M_LOAD, M_COLOURS, M_SCREEN, M_GLOW, M_SWAP, M_RESET, M_N };
// The menu opens while SELECT and CANCEL are still held: their key repeats
// would pick a row or close it at once, so it takes no key until both are up.
static int menu_ids[M_N], menu_n, menu_pick = -1, menu_armed;
static char menu_right[M_N][24];
static ui_row menu_rows[M_N];

static void menu_fill(void)
{
    static const char *const titles[M_N] = {"Back to the game", "Exit to the game list", "Press START",
                                            "Press SELECT", "Save state", "Load state", "Colours", "Screen",
                                            "LED glow", "Swap A and B", "Restart the game"};
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
    menu_armed = 0;
    menu_pick = -1;
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
    chord_on = chord_lit = 0;
    chord_lock = KEY_HELD(keys_before, KEY_SELECT) || KEY_HELD(keys_before, KEY_BACK);   // their release isn't A or B
    leds_game();
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
            find_secret(S_TIME);
        }
        break;
    case M_COLOURS: {
        palette = (palette + 1) % palettes_open();
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
        if (open_game()) {
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

enum { R_CONTINUE, R_GAME, R_SD, R_SECRETS, R_BOARD, R_SHARE, R_SETTINGS, R_HELP, R_INFO, R_ABOUT };
typedef struct {
    uint8_t kind;
    int16_t idx;
} row_t;
static row_t rows[MAX_ROWS];
static ui_row list_rows[MAX_ROWS];
static char row_sub[MAX_ROWS][28];
static char continue_title[NAME_MAX_ + 12];
static char secrets_title[24];
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
    } else if (row.kind == R_SECRETS) {
        cartridge_picture(px, 0xFF4FD8);
        top = "Five secrets";
        wrap("Hints for each. Every one opens new colours; all five, a Solana tank.", lines, 3, 22);
    } else if (row.kind == R_BOARD) {
        cartridge_picture(px, 0xFFD040);
        top = "Leaderboard";
        wrap("The best Tank Siege scores of every badge. Needs Wi-Fi.", lines, 3, 22);
    } else if (row.kind == R_SETTINGS) {
        cartridge_picture(px, 0x8A8A96);
        top = "Settings";
        wrap("Screen size, LED glow, swap A and B.", lines, 3, 22);
    } else if (row.kind == R_INFO) {
        cartridge_picture(px, 0x14F195);
        top = "About";
        wrap("Pocket Arcade, and a QR to its maker on X.", lines, 3, 22);
    } else if (row.kind == R_SHARE) {
        cartridge_picture(px, 0x1D9BF0);
        top = "Share on X";
        wrap("A QR for your phone: X opens with the post and a picture.", lines, 3, 22);
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

// The Konami code on the list. The focused list takes UP and DOWN before
// on_key, so the keys are read from sys_keys() each tick: a new press is a
// key held now and not on the last tick.
static const int32_t konami[8] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT};
static int konami_at;
static uint32_t list_keys_before;

static void konami_watch(void)
{
    static const int32_t dirs[4] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};
    uint32_t k = sys_keys();
    for (int i = 0; i < 4; i++) {
        if (!KEY_HELD(k, dirs[i]) || KEY_HELD(list_keys_before, dirs[i])) {
            continue;
        }
        if (dirs[i] == konami[konami_at]) {
            konami_at++;
        } else {
            konami_at = dirs[i] == KEY_UP ? (konami_at == 2 ? 2 : 1) : 0;   // up up up down...: still on
        }
        if (konami_at == 8) {
            konami_at = 0;
            find_secret(S_IBRL);
        }
    }
    list_keys_before = k;
}

static void list_selected(void *user, int index)
{
    (void)user;
    list_pick = index;
}

static void show_list(void)
{
    state = ST_LIST;
    page_about = page_board = 0;
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
    snprintf(secrets_title, sizeof secrets_title, "Secrets %d/%d", secrets_found(), S_N);
    rows[n_rows] = (row_t){R_SECRETS, 0};
    list_rows[n_rows++] = (ui_row){secrets_title, "Hints: where to look", NULL};
    rows[n_rows] = (row_t){R_BOARD, 0};
    list_rows[n_rows++] = (ui_row){"Leaderboard", "Tank Siege, online", NULL};
    rows[n_rows] = (row_t){R_SHARE, 0};
    list_rows[n_rows++] = (ui_row){"Share on X", "Your Tank Siege best", NULL};
    rows[n_rows] = (row_t){R_SETTINGS, 0};
    list_rows[n_rows++] = (ui_row){"Settings", "Screen, LEDs, A/B", NULL};
    rows[n_rows] = (row_t){R_HELP, 0};
    list_rows[n_rows++] = (ui_row){"How to play", "START, menu, exit", NULL};
    rows[n_rows] = (row_t){R_INFO, 0};
    list_rows[n_rows++] = (ui_row){"About", "Made by @hypnogaba", NULL};
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
        "START: tap SELECT + CANCEL together",
        "  (or menu > Press START): start, pause",
        "Menu: hold SELECT + CANCEL 1 second:",
        "  START, save, colours, exit to list",
        "SELECT = A    CANCEL = B    D-pad",
        "HOME: leave, the game is kept",
        "Secrets: see Secrets in the list",
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

// Our worker (server/): the leaderboard, and the page X shows for a shared post.
#define SHARE_HOST "pocket-arcade.hypnogaba.workers.dev"

// Settings, from the list (the game menu has the same per game).
enum { SET_SCREEN, SET_GLOW, SET_SWAP, SET_N };
static ui_row set_rows[SET_N];
static char set_right[SET_N][24];
static int set_pick = -1;

static void set_selected(void *user, int index)
{
    (void)user;
    set_pick = index;
}

static void settings_fill(void)
{
    snprintf(set_right[SET_SCREEN], sizeof set_right[0], "%s", big ? "full (1.5x)" : "small (1x)");
    snprintf(set_right[SET_GLOW], sizeof set_right[0], "%s", glow ? "on" : "off");
    snprintf(set_right[SET_SWAP], sizeof set_right[0], "%s", swap_ab ? "on" : "off");
    set_rows[SET_SCREEN] = (ui_row){"Screen", "Game picture size", set_right[SET_SCREEN]};
    set_rows[SET_GLOW] = (ui_row){"LED glow", "LEDs follow the picture", set_right[SET_GLOW]};
    set_rows[SET_SWAP] = (ui_row){"Swap A and B", "SELECT = B, CANCEL = A", set_right[SET_SWAP]};
}

static void show_settings(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    settings_fill();
    set_list = ui_list_rows(0, 0, UI_W, UI_H - 16, set_rows, SET_N, set_selected, NULL);
    ui_hint("SELECT change   CANCEL back");
}

static void settings_do(int i)
{
    if (i == SET_SCREEN) {
        big = !big;
        storage_set_int("big", big);
    } else if (i == SET_GLOW) {
        glow = !glow;
        storage_set_int("glow", glow);
    } else if (i == SET_SWAP) {
        swap_ab = !swap_ab;
        storage_set_int("swap", swap_ab);
    }
    settings_fill();
    ui_list_set_rows(set_list, set_rows, SET_N);
    ui_list_select(set_list, i);
}

// About: who made it, and a QR to the maker on X.

static void show_about(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    page_about = 1;
    text(8, 4, "Pocket Arcade", &(ui_label_opts){.size = 20, .color = COL_PURPLE});
    text(8, 32, "Game Boy player for the\nSolana Breakpoint badge.\n16 free games and Tank\nSiege, made for it.", &(ui_label_opts){.size = 12});
    text(8, 104, "Made by @hypnogaba", &(ui_label_opts){.size = 14, .color = COL_YELLOW});
    text(8, 124, "Scan: x.com/hypnogaba", &(ui_label_opts){.size = 12, .color = COL_DIM});
    ui_obj q = ui_qr(170, 8, 140, "https://x.com/hypnogaba");
    badge_obj_free(&q);
    ui_hint("SELECT credits   CANCEL back");
}

// Leaderboard: the best Tank Siege scores, from our worker (server/). Every
// request is signed by the badge server, so the board knows whose score it is.
#define BOARD_URL "https://" SHARE_HOST

static int json_int(const char *p, const char *key, int *out)
{
    const char *k = strstr(p, key);
    if (!k) {
        return 0;
    }
    *out = atoi(k + strlen(key));
    return 1;
}

static void board_show(const char *body)
{
    if (!page_board || state != ST_PAGE) {
        return;                         // the page was left before the answer came
    }
    const char *me = strstr(body, "\"me\":");
    const char *p = body;
    int row = 0;
    while (row < BOARD_ROWS && (p = strstr(p, "\"n\":\"")) && (!me || p < me)) {
        p += 5;
        char name[20];
        int n = 0;
        while (*p && *p != '"' && n < 16) {
            name[n++] = *p++;
        }
        name[n] = 0;
        int sc = 0, st = 0;
        json_int(p, "\"s\":", &sc);
        json_int(p, "\"st\":", &st);
        char line[48];
        snprintf(line, sizeof line, "%d. %-16s %7d  st %d", row + 1, name, sc, st);
        if (board_lbl[row].ref) {
            ui_label_text(board_lbl[row], line);
        }
        row++;
    }
    char foot[48] = "No scores yet: play Tank Siege.";
    int r = 0, sc = 0;
    if (me && json_int(me, "\"r\":", &r) && json_int(me, "\"s\":", &sc)) {
        snprintf(foot, sizeof foot, "You: #%d   %d", r, sc);
    } else if (row) {
        snprintf(foot, sizeof foot, "Play Tank Siege to get on it.");
    }
    if (board_lbl[BOARD_ROWS].ref) {
        ui_label_text(board_lbl[BOARD_ROWS], foot);
    }
}

static void board_answer(void *user, const http_response *res, const char *err)
{
    int sent = (int)(intptr_t)user;     // the score posted (0: just reading the board)
    if (err || !res || res->status != 200) {
        if (page_board && state == ST_PAGE && board_lbl[0].ref) {
            ui_label_text(board_lbl[0], err && !strcmp(err, "offline") ? "Offline: join Wi-Fi to see the board."
                                                                       : "The board didn't answer. SELECT to retry.");
        }
        return;
    }
    if (sent > storage_get_int("ts_sent", 0)) {
        storage_set_int("ts_sent", sent);   // what the board has; a newer best is sent next time
    }
    static char body[2048];
    http_body(res, body, sizeof body);
    board_show(body);
}

// Sends the Tank Siege best if the board hasn't got it yet; the answer is the board.
static int board_send(void)
{
    int hi = storage_get_int("ts_hi", 0);
    if (hi <= 0 || hi <= storage_get_int("ts_sent", 0)) {
        return 0;
    }
    char name[24];
    if (badge_owner(name, sizeof name) <= 0) {
        snprintf(name, sizeof name, "anon");
    }
    char js[96];
    snprintf(js, sizeof js, "{\"score\":%d,\"stage\":%d,\"name\":\"%s\"}", hi,
             storage_get_int("ts_st", 1) > 99 ? 99 : storage_get_int("ts_st", 1), name[0] == '@' ? name : "anon");
    return http_post_json(BOARD_URL "/score", js, board_answer, (void *)(intptr_t)hi) == 0;
}

static void board_fetch(void)
{
    if (board_lbl[0].ref) {
        ui_label_text(board_lbl[0], "Loading...");
    }
    if (!board_send()) {
        http_get(BOARD_URL "/top?n=9", board_answer, NULL);
    }
}

static void show_board(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    page_board = 1;
    text(8, 4, "Tank Siege leaderboard", &(ui_label_opts){.size = 20, .color = COL_PURPLE});
    for (int i = 0; i < BOARD_ROWS; i++) {
        board_lbl[i] = ui_label(8, 32 + i * 15, "", &(ui_label_opts){.size = 12, .color = i < 3 ? COL_YELLOW : COL_TEXT});
    }
    board_lbl[BOARD_ROWS] = ui_label(8, 172, "", &(ui_label_opts){.size = 12, .color = COL_GREEN});
    ui_hint("SELECT refresh   CANCEL back");
    board_fetch();
}

static void show_secrets(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    char t[32];
    snprintf(t, sizeof t, "Secrets %d/%d", secrets_found(), S_N);
    text(8, 4, t, &(ui_label_opts){.size = 20, .color = COL_PURPLE});
    for (int i = 0; i < S_N; i++) {
        int got = (secret_bits >> i) & 1;
        char name[32];
        snprintf(name, sizeof name, "%s %s", got ? "+" : "?", secrets[i].name);
        int y = 32 + i * 30;
        text(8, y, name, &(ui_label_opts){.size = 12, .color = got ? COL_GREEN : COL_YELLOW});
        text(20, y + 13, got ? secrets[i].how : secrets[i].hint,
             &(ui_label_opts){.size = 12, .color = got ? COL_DIM : COL_TEXT});
    }
    text(8, 184, secrets_found() == S_N ? "All found: the Solana tank is yours." : "Each: new colours. All 5: a Solana tank.",
         &(ui_label_opts){.size = 12, .color = COL_PINK});
    ui_hint("CANCEL back");
}


static void url_add(char *out, size_t cap, const char *in)
{
    size_t n = strlen(out);
    for (const unsigned char *c = (const unsigned char *)in; *c && n + 4 < cap; c++) {
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || strchr("-._~", *c)) {
            out[n++] = (char)*c;
        } else if (*c == ' ') {
            out[n++] = '+';
        } else {
            n += (size_t)snprintf(out + n, cap - n, "%%%02X", *c);
        }
    }
    out[n] = 0;
}

static char share_url[260];

static void show_share(void)
{
    state = ST_PAGE;
    clear_screen(COL_BG);
    int hi = storage_get_int("ts_hi", 0), st = storage_get_int("ts_st", 1);
    char post[140], link[96];
    if (hi > 0) {
        snprintf(post, sizeof post, "My Tank Siege best: %d, stage %d. Pocket Arcade on my Solana Breakpoint badge. @SolanaFndn", hi, st);
    } else {
        snprintf(post, sizeof post, "16 free games and a new tank game on my Solana Breakpoint badge: Pocket Arcade. @SolanaFndn");
    }
    snprintf(link, sizeof link, "https://" SHARE_HOST "/share?s=%d&st=%d", hi, st);
    snprintf(share_url, sizeof share_url, "https://x.com/intent/post?text=");
    url_add(share_url, sizeof share_url, post);
    strncat(share_url, "&url=", sizeof share_url - strlen(share_url) - 1);
    url_add(share_url, sizeof share_url, link);
    text(8, 4, "Share on X", &(ui_label_opts){.size = 20, .color = COL_PURPLE});
    char line[40];
    snprintf(line, sizeof line, hi > 0 ? "Tank Siege %d" : "Pocket Arcade", hi);
    text(8, 40, line, &(ui_label_opts){.size = 14, .color = COL_YELLOW});
    if (hi > 0) {
        snprintf(line, sizeof line, "stage %d", st);
        text(8, 60, line, &(ui_label_opts){.size = 12, .color = COL_DIM});
    }
    text(8, 90, "Scan with your\nphone: X opens\nwith the post\nand a picture.", &(ui_label_opts){.size = 12, .color = COL_DIM});
    badge_print(share_url);             // the simulator checks its length
    ui_obj q = ui_qr(146, 8, 168, share_url);
    if (!q.ref) {
        text(146, 80, "Update Solana OS\nto show the QR", &(ui_label_opts){.size = 14, .color = COL_PINK});
    }
    badge_obj_free(&q);
    ui_hint("CANCEL back");
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
    big = storage_get_int("big", 1);           // full screen (1.5x) unless changed
    glow = storage_get_int("glow", 1);
    swap_ab = storage_get_int("swap", 0);
    secret_bits = storage_get_int("sec", 0) & ((1 << S_N) - 1);
    tk_set_skin(secrets_found() == S_N);
    int32_t n = fs_read("games/index.bin", index_buf, sizeof index_buf);
    n_games = n > 0 ? pack_index(index_buf, (uint32_t)n, games, PACK_MAX_GAMES) : -1;
    if (n_games < 0) {
        n_games = 0;
        badge_print("Pocket Arcade: games/index.bin is missing or damaged");
    }
    files_list(found, NULL);
    show_list();                          // the list first; How to play is a row in it
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
            } else if (row.kind == R_SECRETS) {
                show_secrets();
            } else if (row.kind == R_SHARE) {
                show_share();
            } else if (row.kind == R_BOARD) {
                show_board();
            } else if (row.kind == R_SETTINGS) {
                show_settings();
            } else if (row.kind == R_INFO) {
                show_about();
            } else if (row.kind == R_HELP) {
                show_help();
            } else {
                show_credits();
            }
            return;
        }
        konami_watch();
        show_info(ui_list_selected(game_list));
        break;
    case ST_LOAD:
        load_tick();
        break;
    case ST_RUN:
        run_tick(ms);
        break;
    case ST_MENU:
        if (!menu_armed) {
            uint32_t k = sys_keys();
            menu_armed = !KEY_HELD(k, KEY_SELECT) && !KEY_HELD(k, KEY_BACK);
            menu_pick = -1;             // a repeat of the held SELECT isn't a choice
            break;
        }
        if (menu_pick >= 0) {
            int r = menu_pick;
            menu_pick = -1;
            menu_do(r);
        }
        break;
    case ST_PAGE:
        if (set_pick >= 0) {
            int r = set_pick;
            set_pick = -1;
            settings_do(r);
        }
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
    if (key == KEY_SELECT && state == ST_PAGE && !repeat) {
        if (page_about) {
            page_about = 0;
            show_credits();
            return 1;
        }
        if (page_board) {
            board_fetch();
            return 1;
        }
    }
    if (key != KEY_BACK) {
        return state == ST_RUN;           // in a game every key is the game's
    }
    switch (state) {
    case ST_RUN:
        return 1;                         // CANCEL is B
    case ST_MENU:
        if (menu_armed && !repeat) {
            back_to_game();
        }
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
