// emu.c: Walnut-CGB (walnut/, MIT) wrapped for Pocket Arcade. See emu.h.
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "emu.h"

#define ENABLE_SOUND 0 // the badge has no speaker: the sound registers still work, nothing is synthesised
struct gb_s;
#include "walnut/walnut_cgb.h"
#include "walnut/walnut_cgb_cpu.h"

#define BE16(v) ((uint16_t)((((v) >> 8) & 0xFF) | (((v) & 0xFF) << 8)))
#define RGB565(rgb) ((uint16_t)((((rgb) >> 8) & 0xF800) | (((rgb) >> 5) & 0x07E0) | (((rgb) >> 3) & 0x001F)))

static struct gb_s gb;

// The ROM: whole in memory, or paged through a cache.
static const uint8_t *rom_mem;
static uint32_t rom_size, rom_mask, rom_banks;
static emu_read_fn rom_read;
static void *rom_ud;
static uint8_t *cache;
static uint32_t n_slots, next_victim, misses;
static const uint8_t *bank_ptr[EMU_MAX_BANKS];
static int16_t slot_bank[256];

static uint8_t cart_ram[128 * 1024];
static uint32_t ram_size;

static int err;
static uint16_t err_addr;
static uint16_t *draw_fb;
static uint16_t dmg_lut[64];
static const emu_native *nat;      // a native game instead of the cartridge, or NULL
static uint8_t joy_before = 0xFF;  // the last joypad byte, for the joypad interrupt   // the core's pixel byte (palette in bits 5-4, shade in 1-0) -> big-endian RGB565

// ---------------------------------------------------------------------------
// ROM reads: the core calls these for every opcode, keep them short.

static const uint8_t *load_bank(uint32_t b)
{
    b %= rom_banks;
    if (bank_ptr[b]) {
        return bank_ptr[b];
    }
    uint32_t slot = next_victim;
    // Bank 0 is read first and keeps slot 0; the others take turns.
    next_victim = next_victim + 1 < n_slots ? next_victim + 1 : (n_slots > 1 ? 1 : 0);
    if (slot_bank[slot] >= 0) {
        bank_ptr[slot_bank[slot]] = NULL;
    }
    uint8_t *p = cache + (size_t)slot * EMU_BANK;
    int32_t n = rom_read(rom_ud, b * EMU_BANK, p, EMU_BANK);
    if (n < 0) {
        n = 0;
    }
    if (n < EMU_BANK) {
        memset(p + n, 0xFF, EMU_BANK - (uint32_t)n);
    }
    slot_bank[slot] = (int16_t)b;
    bank_ptr[b] = p;
    misses++;
    return p;
}

static inline uint8_t paged_byte(uint32_t a)
{
    uint32_t b = a >> 14;
    const uint8_t *p = b < EMU_MAX_BANKS ? bank_ptr[b] : NULL;
    if (!p) {
        p = load_bank(b);
    }
    return p[a & (EMU_BANK - 1)];
}

static uint8_t mem_rd8(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return rom_mem[a & rom_mask];
}

static uint16_t mem_rd16(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return (uint16_t)(rom_mem[a & rom_mask] | rom_mem[(a + 1) & rom_mask] << 8);
}

static uint32_t mem_rd32(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return (uint32_t)rom_mem[a & rom_mask] | (uint32_t)rom_mem[(a + 1) & rom_mask] << 8 |
           (uint32_t)rom_mem[(a + 2) & rom_mask] << 16 | (uint32_t)rom_mem[(a + 3) & rom_mask] << 24;
}

static uint8_t pg_rd8(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return paged_byte((uint32_t)a);
}

static uint16_t pg_rd16(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return (uint16_t)(paged_byte((uint32_t)a) | paged_byte((uint32_t)a + 1) << 8);
}

static uint32_t pg_rd32(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    uint32_t x = (uint32_t)a;
    return (uint32_t)paged_byte(x) | (uint32_t)paged_byte(x + 1) << 8 | (uint32_t)paged_byte(x + 2) << 16 |
           (uint32_t)paged_byte(x + 3) << 24;
}

static uint8_t ram_rd(struct gb_s *g, const uint_fast32_t a)
{
    (void)g;
    return a < sizeof cart_ram ? cart_ram[a] : 0xFF;
}

static void ram_wr(struct gb_s *g, const uint_fast32_t a, const uint8_t v)
{
    (void)g;
    if (a < sizeof cart_ram) {
        cart_ram[a] = v;
    }
}

static void on_error(struct gb_s *g, const enum gb_error_e e, const uint16_t a)
{
    (void)g;
    if (!err) {
        err = (int)e + 1;
        err_addr = a;
    }
}

// ---------------------------------------------------------------------------
// The screen

static void draw_line(struct gb_s *g, const uint8_t *px, const uint_fast8_t line)
{
    if (!draw_fb || line >= EMU_H) {
        return;
    }
    uint16_t *o = draw_fb + line * EMU_W;
    if (g->cgb.cgbMode) {
        const uint16_t *pal = g->cgb.fixPalette;
        for (int x = 0; x < EMU_W; x++) {
            o[x] = BE16(pal[px[x] & 0x3F]);
        }
    } else {
        for (int x = 0; x < EMU_W; x++) {
            o[x] = dmg_lut[px[x] & 0x3F];
        }
    }
}

// Shades are lightest first; the core's palette field is OBJ0 0, OBJ1 1, BG 2.
const emu_palette emu_palettes[] = {
    {"Classic green", {{0xE0F8D0, 0x88C070, 0x346856, 0x081820},
                       {0xE0F8D0, 0x88C070, 0x346856, 0x081820},
                       {0xE0F8D0, 0x88C070, 0x346856, 0x081820}}},
    {"Pocket grey", {{0xF2F2EC, 0xA8A8A0, 0x58585A, 0x101012},
                     {0xF2F2EC, 0xA8A8A0, 0x58585A, 0x101012},
                     {0xF2F2EC, 0xA8A8A0, 0x58585A, 0x101012}}},
    {"Solana", {{0xF6F1FF, 0xB58CFF, 0x6B2FD9, 0x15052E},
                {0xEAFFF6, 0x14F195, 0x0B8A57, 0x00241A},
                {0xFFF4E6, 0xFFB45C, 0xC2470E, 0x2A0B00}}},
    {"Arcade", {{0xFFFFFF, 0x7BFF31, 0x0063C5, 0x000000},
                {0xFFFFFF, 0xFF8484, 0x943A3A, 0x000000},
                {0xFFFFFF, 0xFFD54F, 0xB86E00, 0x000000}}},
    {"Sunset", {{0xFFF0D8, 0xF7A86B, 0xA8435F, 0x2B1035},
                {0xFFF0D8, 0xFFD36B, 0xD9572B, 0x2B1035},
                {0xFFFFFF, 0x9CE0FF, 0x3B6FD1, 0x101040}}},
    {"Night", {{0x9BBCFF, 0x4E6CC8, 0x253470, 0x05081C},
               {0xFFFFFF, 0xFFE27A, 0xC89A1E, 0x302000},
               {0xFFFFFF, 0x9CFFD0, 0x2BB57A, 0x00301C}}},
};
const int emu_n_palettes = (int)(sizeof emu_palettes / sizeof emu_palettes[0]);

void emu_set_palette(int i)
{
    if (i < 0 || i >= emu_n_palettes) {
        i = 0;
    }
    for (int k = 0; k < 64; k++) {
        int layer = (k >> 4) & 3;       // 0 OBJ0, 1 OBJ1, 2 BG, 3 unused
        int which = layer == 2 ? 0 : layer == 0 ? 1 : layer == 1 ? 2 : 0;
        uint16_t c = RGB565(emu_palettes[i].c[which][k & 3]);
        dmg_lut[k] = BE16(c);
    }
}

// ---------------------------------------------------------------------------
// Opening

static int finish_open(uint8_t (*r8)(struct gb_s *, const uint_fast32_t),
                       uint16_t (*r16)(struct gb_s *, const uint_fast32_t),
                       uint32_t (*r32)(struct gb_s *, const uint_fast32_t))
{
    err = 0;
    err_addr = 0;
    joy_before = 0xFF;
    nat = NULL;
    memset(&gb, 0, sizeof gb);
    memset(cart_ram, 0xFF, sizeof cart_ram);
    enum gb_init_error_e e = gb_init(&gb, r8, r16, r32, ram_rd, ram_wr, on_error, NULL);
    if (e == GB_INIT_INVALID_CHECKSUM) {
        return EMU_BAD_HEADER;
    }
    if (e != GB_INIT_NO_ERROR) {
        return EMU_BAD_MBC;
    }
    gb_init_lcd(&gb, draw_line);
    size_t rs = 0;
    gb_get_save_size_s(&gb, &rs);
    ram_size = rs <= sizeof cart_ram ? (uint32_t)rs : 0;
    return EMU_OK;
}

int emu_open_mem(const uint8_t *rom, uint32_t size)
{
    if (size < 0x8000 || size % EMU_BANK) {
        return EMU_BAD_HEADER;
    }
    rom_mem = rom;
    rom_size = size;
    rom_banks = size / EMU_BANK;
    // Cartridge sizes are powers of two; the mask wraps a read past the end
    // the way the bus would.
    uint32_t m = 1;
    while (m < size) {
        m <<= 1;
    }
    rom_mask = m == size ? size - 1 : 0xFFFFFFFFu;
    if (rom_mask == 0xFFFFFFFFu) {
        return EMU_BAD_HEADER;          // the pack builder pads every ROM to a power of two
    }
    rom_read = NULL;
    return finish_open(mem_rd8, mem_rd16, mem_rd32);
}

int emu_open_paged(uint32_t size, emu_read_fn read, void *ud, uint8_t *cache_mem, uint32_t slots)
{
    if (size < 0x150 || slots < 2 || slots > 256) {
        return EMU_BAD_HEADER;
    }
    rom_mem = NULL;
    rom_size = size;
    rom_banks = (size + EMU_BANK - 1) / EMU_BANK;
    if (rom_banks > EMU_MAX_BANKS) {
        return EMU_BAD_HEADER;
    }
    rom_read = read;
    rom_ud = ud;
    cache = cache_mem;
    n_slots = slots;
    next_victim = 0;
    misses = 0;
    memset(bank_ptr, 0, sizeof bank_ptr);
    for (int i = 0; i < 256; i++) {
        slot_bank[i] = -1;
    }
    return finish_open(pg_rd8, pg_rd16, pg_rd32);
}

int emu_open_native(const emu_native *m)
{
    err = 0;
    err_addr = 0;
    misses = 0;
    nat = m;
    m->reset();
    return EMU_OK;
}

void emu_frame(uint8_t joypad, uint16_t *fb)
{
    if (nat) {
        nat->frame((uint8_t)~joypad, fb);
        return;
    }
    if (err) {
        return;
    }
    draw_fb = fb;
    // A button going down raises the joypad interrupt, as on the console:
    // some games HALT until it comes (Walnut doesn't raise it itself).
    if ((uint8_t)(joy_before & ~joypad)) {
        gb.hram_io[IO_IF] |= CONTROL_INTR;
    }
    joy_before = joypad;
    gb.direct.joypad = joypad;
    gb.display.lcd_draw_line = fb ? draw_line : NULL;
    gb_run_frame_dualfetch(&gb);
    draw_fb = NULL;
}

int emu_is_cgb(void) { return nat || gb.cgb.cgbMode != 0; }   // a native game has its own colours
int emu_error(void) { return err; }
uint16_t emu_error_addr(void) { return err_addr; }
uint32_t emu_misses(void) { return misses; }

uint8_t *emu_cart_ram(uint32_t *size)
{
    if (nat) {
        *size = nat->ram_size;
        return nat->ram;
    }
    *size = ram_size;
    return cart_ram;
}

void emu_set_time(int64_t unix_seconds)
{
    if (unix_seconds <= 0 || nat) {
        return;
    }
    time_t t = (time_t)unix_seconds;
    struct tm *tm = gmtime(&t);
    if (tm) {
        gb_set_rtc(&gb, tm);
    }
}

// ---------------------------------------------------------------------------
// Snapshots: a header, the machine (its pointers put back on load) and the
// cartridge RAM.

#define STATE_MAGIC 0x31415350u   // "PSA1"

typedef struct {
    uint32_t magic, gb_size, ram_size, game_crc, build;
} state_head;

// Changes whenever the machine's layout may change: the struct's size and a
// version bumped by hand when emu.c or Walnut changes.
#define STATE_BUILD (0x00010000u ^ (uint32_t)sizeof(struct gb_s))

#define NATIVE_MAGIC 0x314E5350u  // "PSN1"

uint32_t emu_state_size(void)
{
    if (nat) {
        return (uint32_t)(sizeof(state_head) + nat->state_size + nat->ram_size);
    }
    return (uint32_t)(sizeof(state_head) + sizeof gb + ram_size);
}

uint32_t emu_state_save(uint8_t *dst, uint32_t cap, uint32_t game_crc)
{
    uint32_t need = emu_state_size();
    if (cap < need) {
        return 0;
    }
    if (nat) {
        state_head n = {NATIVE_MAGIC, nat->state_size, nat->ram_size, game_crc, nat->build};
        memcpy(dst, &n, sizeof n);
        memcpy(dst + sizeof n, nat->state, nat->state_size);
        memcpy(dst + sizeof n + nat->state_size, nat->ram, nat->ram_size);
        return need;
    }
    state_head h = {STATE_MAGIC, (uint32_t)sizeof gb, ram_size, game_crc, STATE_BUILD};
    memcpy(dst, &h, sizeof h);
    memcpy(dst + sizeof h, &gb, sizeof gb);
    memcpy(dst + sizeof h + sizeof gb, cart_ram, ram_size);
    return need;
}

int emu_state_load(const uint8_t *src, uint32_t len, uint32_t game_crc)
{
    state_head h;
    if (len < sizeof h) {
        return -1;
    }
    memcpy(&h, src, sizeof h);
    if (nat) {
        if (h.magic != NATIVE_MAGIC || h.gb_size != nat->state_size || h.ram_size != nat->ram_size ||
            h.game_crc != game_crc || h.build != nat->build || len != emu_state_size()) {
            return -1;
        }
        // the RAM is the game's record (high score): newer than any
        // snapshot, so it stays as it is
        memcpy(nat->state, src + sizeof h, nat->state_size);
        return 0;
    }
    if (h.magic != STATE_MAGIC || h.gb_size != sizeof gb || h.ram_size != ram_size || h.game_crc != game_crc ||
        h.build != STATE_BUILD || len != sizeof h + sizeof gb + ram_size) {
        return -1;
    }
    struct gb_s keep = gb;
    memcpy(&gb, src + sizeof h, sizeof gb);
    gb.gb_rom_read = keep.gb_rom_read;
    gb.gb_rom_read_16bit = keep.gb_rom_read_16bit;
    gb.gb_rom_read_32bit = keep.gb_rom_read_32bit;
    gb.gb_cart_ram_read = keep.gb_cart_ram_read;
    gb.gb_cart_ram_write = keep.gb_cart_ram_write;
    gb.gb_error = keep.gb_error;
    gb.gb_serial_tx = keep.gb_serial_tx;
    gb.gb_serial_rx = keep.gb_serial_rx;
    gb.gb_bootrom_read = keep.gb_bootrom_read;
    gb.display.lcd_draw_line = keep.display.lcd_draw_line;
    gb.direct.priv = keep.direct.priv;
    memcpy(cart_ram, src + sizeof h + sizeof gb, ram_size);
    err = 0;
    joy_before = 0xFF;
    return 0;
}

uint32_t emu_crc32(const uint8_t *p, uint32_t n, uint32_t crc)
{
    crc = ~crc;
    for (uint32_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
    }
    return ~crc;
}
