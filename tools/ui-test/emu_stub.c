// emu_stub.c: emu.h without the Game Boy, for checking Pocket Arcade's screens
// in the browser emulator (where the real core runs at under 1 fps and the
// whole app is too big to paste). Draws a moving test card instead of a game.
#include <string.h>
#include "emu.h"

const emu_palette emu_palettes[] = {{"Classic green", {{0}}}, {"Pocket grey", {{0}}}};
const int emu_n_palettes = 2;
static uint32_t t;
static uint8_t ram[8192];

int emu_open_mem(const uint8_t *rom, uint32_t size) { (void)rom; (void)size; t = 0; return EMU_OK; }
int emu_open_paged(uint32_t size, emu_read_fn read, void *ud, uint8_t *cache, uint32_t slots)
{
    (void)size; (void)read; (void)ud; (void)cache; (void)slots;
    return EMU_OK;
}
void emu_frame(uint8_t joypad, uint16_t *fb)
{
    t++;
    if (!fb) return;
    for (int y = 0; y < EMU_H; y++)
        for (int x = 0; x < EMU_W; x++) {
            uint16_t c = (uint16_t)((((x + t) >> 3) & 31) << 11 | ((y >> 2) & 63) << 5 | ((x ^ y) >> 3 & 31));
            if (y < 8 || y >= EMU_H - 8 || x < 8 || x >= EMU_W - 8) c = joypad != 0xFF ? 0xF800 : 0xFFFF;
            fb[y * EMU_W + x] = (uint16_t)((c >> 8) | (c << 8));
        }
}
int emu_is_cgb(void) { return 0; }
int emu_error(void) { return 0; }
uint16_t emu_error_addr(void) { return 0; }
uint32_t emu_misses(void) { return 0; }
void emu_set_palette(int i) { (void)i; }
uint8_t *emu_cart_ram(uint32_t *size) { *size = sizeof ram; return ram; }
uint32_t emu_state_size(void) { return 64; }
uint32_t emu_state_save(uint8_t *dst, uint32_t cap, uint32_t crc) { if (cap < 64) return 0; memset(dst, 0, 64); memcpy(dst, &crc, 4); return 64; }
int emu_state_load(const uint8_t *src, uint32_t len, uint32_t crc) { return len == 64 && !memcmp(src, &crc, 4) ? 0 : -1; }
void emu_set_time(int64_t s) { (void)s; }
uint32_t emu_crc32(const uint8_t *p, uint32_t n, uint32_t crc)
{
    crc = ~crc;
    for (uint32_t i = 0; i < n; i++) { crc ^= p[i]; for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1))); }
    return ~crc;
}
