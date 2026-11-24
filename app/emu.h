// emu.h: the Game Boy side of Pocket Arcade, with no badge API in it, so the
// same file runs on the badge and in tools/harness.c on a computer.
//
// One machine at a time (a global): open a ROM, run frames, read the battery
// RAM, save and load a snapshot of the whole machine.
#pragma once
#include <stdint.h>

#define EMU_W 160
#define EMU_H 144
#define EMU_BANK 16384
#define EMU_MAX_BANKS 512                 // 8 MB, the biggest cartridge

// Read len bytes of the ROM at off into dst (paged ROMs only); bytes read.
typedef int32_t (*emu_read_fn)(void *ud, uint32_t off, uint8_t *dst, uint32_t len);

enum { EMU_OK = 0, EMU_BAD_HEADER = 1, EMU_BAD_MBC = 2, EMU_NO_MEMORY = 3 };

// The whole ROM in memory (rom stays the caller's). size: a multiple of 16 KB.
int emu_open_mem(const uint8_t *rom, uint32_t size);
// A ROM read bank by bank through read() into a cache of slots * 16 KB.
int emu_open_paged(uint32_t size, emu_read_fn read, void *ud, uint8_t *cache, uint32_t slots);

// One emulated frame (16.74 ms of the machine). joypad: the Game Boy's
// active-low byte (bit 0 A, 1 B, 2 SELECT, 3 START, 4 RIGHT, 5 LEFT, 6 UP,
// 7 DOWN). fb: 160 x 144 big-endian RGB565 to draw into, or NULL to skip
// drawing (the machine still runs).
void emu_frame(uint8_t joypad, uint16_t *fb);

int emu_is_cgb(void);                      // the game runs in Color mode
int emu_error(void);                       // non-zero once the core reported an error
uint16_t emu_error_addr(void);
uint32_t emu_misses(void);                 // paged ROM: banks read since open

// Palettes for original (non-Color) games: BG, OBJ0, OBJ1, 4 shades each,
// 0xRRGGBB, lightest first.
typedef struct {
    const char *name;
    uint32_t c[3][4];
} emu_palette;
extern const emu_palette emu_palettes[];
extern const int emu_n_palettes;
#define EMU_BASE_PALETTES 6        // always open; the rest open one per secret found (main.c)
void emu_set_palette(int i);
// The next game opened runs as an original Game Boy even if it could use
// the Color's colours (its own may be just four greens): 1 on, 0 off.
void emu_force_dmg(int on);

// Battery RAM.
uint8_t *emu_cart_ram(uint32_t *size);

// A snapshot of the machine and its cartridge RAM. emu_state_load checks the
// header and refuses a snapshot from another build or another game.
uint32_t emu_state_size(void);
uint32_t emu_state_save(uint8_t *dst, uint32_t cap, uint32_t game_crc);
int emu_state_load(const uint8_t *src, uint32_t len, uint32_t game_crc);

// The real-time clock some cartridges have (MBC3): seconds since 1970.
void emu_set_time(int64_t unix_seconds);

uint32_t emu_crc32(const uint8_t *p, uint32_t n, uint32_t crc);

// A game written for the badge itself (tanks.c) behind the same calls: the
// app runs it, saves it and snapshots it like a cartridge. Its state is
// plain data (no pointers), so a snapshot is a copy of it.
typedef struct {
    void (*reset)(void);
    void (*frame)(uint8_t keys, uint16_t *fb);   // keys active high, same bits as the joypad
    uint8_t *ram;                                 // its battery RAM
    uint32_t ram_size;
    void *state;
    uint32_t state_size, build;                   // build: bumped when the state's layout changes
} emu_native;

// Open a native game (replaces any cartridge); 0.
int emu_open_native(const emu_native *m);
