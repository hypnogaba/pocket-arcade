// pack.c: see pack.h and tools/build_pack.py for the layout.
#include <string.h>

#include "pack.h"
#include "tinf/tinf.h"

#define BANK 16384
#define PREVIEW_PX (160 * 144)

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static const char *str(const uint8_t *rec, int off, int n)
{
    // fields are NUL-padded and the builder leaves at least one NUL
    return rec[off + n - 1] == 0 ? (const char *)rec + off : "";
}

int pack_index(const uint8_t *buf, uint32_t len, pack_game *games, int cap)
{
    if (len < 8 || memcmp(buf, "PAI1", 4) || rd16(buf + 6) != 1) {
        return -1;
    }
    int n = rd16(buf + 4);
    if (n > cap || len < 8 + (uint32_t)n * PACK_REC) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        const uint8_t *r = buf + 8 + i * PACK_REC;
        pack_game *g = &games[i];
        g->id = str(r, 0, 12);
        g->title = str(r, 12, 24);
        g->genre = str(r, 36, 12);
        g->author = str(r, 48, 40);
        g->license = str(r, 88, 24);
        g->source = str(r, 112, 48);
        g->blurb = str(r, 160, 72);
        g->pack = r[232];
        g->flags = r[233];
        g->palette = r[234];
        g->banks = rd16(r + 236);
        g->off = rd32(r + 240);
        g->preview_off = rd32(r + 244);
        g->preview_len = rd32(r + 248);
        g->crc = rd32(r + 252);
        if (!g->banks || g->banks > 512 || !g->id[0]) {
            return -1;
        }
    }
    static int inited;
    if (!inited) {
        tinf_init();
        inited = 1;
    }
    return n;
}

// The bank table of the game unpacked last.
static const pack_game *table_of;
static uint32_t table[513];
static uint8_t zbuf[BANK];

int pack_unpack(const pack_game *g, uint32_t from, uint32_t to, uint8_t *rom, pack_read_fn rd, void *ud)
{
    if (table_of != g) {
        uint32_t n = (uint32_t)g->banks + 1;
        if (rd(ud, g->pack, g->off, table, n * 4) != (int32_t)(n * 4)) {
            table_of = NULL;
            return -1;
        }
        for (uint32_t i = 0; i < n; i++) {
            table[i] = rd32((const uint8_t *)&table[i]);   // little-endian on disk, whatever the host
        }
        table_of = g;
    }
    for (uint32_t b = from; b < to && b < g->banks; b++) {
        uint32_t a = table[b], z = table[b + 1];
        if (z <= a || z - a > BANK) {
            return -1;
        }
        uint32_t n = z - a;
        uint8_t *dst = rom + (size_t)b * BANK;
        if (n == BANK) {
            if (rd(ud, g->pack, g->off + a, dst, BANK) != BANK) {
                return -1;
            }
            continue;
        }
        if (rd(ud, g->pack, g->off + a, zbuf, n) != (int32_t)n) {
            return -1;
        }
        unsigned int out = BANK;
        if (tinf_uncompress(dst, &out, zbuf, n) != TINF_OK || out != BANK) {
            return -1;
        }
    }
    return 0;
}

static uint8_t pbuf[PREVIEW_PX + 512 + 2];
static uint8_t pidx[PREVIEW_PX];

int pack_preview(const pack_game *g, uint16_t *fb, pack_read_fn rd, void *ud)
{
    if (g->preview_len < 2 || g->preview_len > sizeof pbuf) {
        return -1;
    }
    if (rd(ud, g->pack, g->preview_off, pbuf, g->preview_len) != (int32_t)g->preview_len) {
        return -1;
    }
    uint32_t nc = rd16(pbuf);
    if (nc == 0 || nc > 256 || 2 + nc * 2 > g->preview_len) {
        return -1;
    }
    uint16_t pal[256];
    for (uint32_t i = 0; i < nc; i++) {
        // stored big-endian already: copy the two bytes as they are
        memcpy(&pal[i], pbuf + 2 + i * 2, 2);
    }
    unsigned int out = PREVIEW_PX;
    uint32_t head = 2 + nc * 2;
    if (tinf_uncompress(pidx, &out, pbuf + head, g->preview_len - head) != TINF_OK || out != PREVIEW_PX) {
        return -1;
    }
    for (uint32_t i = 0; i < PREVIEW_PX; i++) {
        fb[i] = pal[pidx[i] < nc ? pidx[i] : 0];
    }
    return 0;
}
