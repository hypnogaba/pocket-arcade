// pack.h: the bundled games (games/index.bin and games/p<N>.bin, made by
// tools/build_pack.py). No badge API here: reads go through a callback, so
// tools/test_pack.c checks every game on a computer.
#pragma once
#include <stdint.h>

#define PACK_MAX_GAMES 64
#define PACK_REC 256

typedef struct {
    const char *id, *title, *genre, *author, *license, *source, *blurb;   // into the index buffer
    uint8_t pack, flags, palette;
    uint16_t banks;
    uint32_t off, preview_off, preview_len, crc;
} pack_game;

#define PACK_CGB 1
#define PACK_NATIVE 2      // a game written for the badge (no ROM, banks 0): only the preview is in the pack

// Read len bytes at off of pack file n into dst; bytes read, or < 0.
typedef int32_t (*pack_read_fn)(void *ud, int n, uint32_t off, void *dst, uint32_t len);

// Parse index.bin (the buffer must stay alive); the number of games, or -1.
int pack_index(const uint8_t *buf, uint32_t len, pack_game *games, int cap);

// Unpack banks [from, to) of g into rom (rom is g->banks * 16 KB). The bank
// table is read the first time and kept for the same game. 0, or -1.
int pack_unpack(const pack_game *g, uint32_t from, uint32_t to, uint8_t *rom, pack_read_fn rd, void *ud);

// The preview: 160 x 144 big-endian RGB565 into fb. 0, or -1.
int pack_preview(const pack_game *g, uint16_t *fb, pack_read_fn rd, void *ud);
