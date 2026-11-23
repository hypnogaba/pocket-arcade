// test_pack.c: unpack every bundled game from app/games/ with app/pack.c,
// compare it with roms/<file> (padded as the builder pads) and its CRC,
// decode its preview, run it a few seconds through app/emu.c, and check
// what a broken pack does (a cut file, flipped bytes): an error, never a
// crash or a game that looks fine.
//
//   test_pack <app/games dir> <roms dir> <games.txt: "id romfile" lines> <preview out dir>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app/emu.h"
#include "../app/pack.h"

static uint8_t *files[8];
static uint32_t sizes[8];
static int corrupt_file = -1;
static uint32_t cut_to = 0xFFFFFFFF;

static uint8_t *slurp(const char *p, uint32_t *n)
{
    FILE *f = fopen(p, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)s + 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) {
        fclose(f);
        free(b);
        return NULL;
    }
    fclose(f);
    *n = (uint32_t)s;
    return b;
}

static int32_t rd(void *ud, int n, uint32_t off, void *dst, uint32_t len)
{
    (void)ud;
    if (n < 0 || n >= 8 || !files[n]) {
        return -1;
    }
    uint32_t size = sizes[n] < cut_to ? sizes[n] : cut_to;
    if (off >= size) {
        return 0;
    }
    uint32_t k = size - off < len ? size - off : len;
    memcpy(dst, files[n] + off, k);
    if (n == corrupt_file) {
        for (uint32_t i = 0; i < k; i += 97) {
            ((uint8_t *)dst)[i] ^= 0x5A;
        }
    }
    return (int32_t)k;
}

static void write_ppm(const char *path, const uint16_t *fb)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n160 144\n255\n");
    for (int i = 0; i < 160 * 144; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: test_pack <games dir> <roms dir> <list> <out dir>\n");
        return 2;
    }
    char p[512];
    uint32_t ilen;
    snprintf(p, sizeof p, "%s/index.bin", argv[1]);
    uint8_t *index = slurp(p, &ilen);
    if (!index) {
        printf("FAIL no index\n");
        return 1;
    }
    for (int i = 0; i < 8; i++) {
        snprintf(p, sizeof p, "%s/p%d.bin", argv[1], i);
        files[i] = slurp(p, &sizes[i]);
    }
    static pack_game games[PACK_MAX_GAMES];
    int n = pack_index(index, ilen, games, PACK_MAX_GAMES);
    if (n <= 0) {
        printf("FAIL index %d\n", n);
        return 1;
    }
    // a cut or wrong index is refused
    if (pack_index(index, 100, games, PACK_MAX_GAMES) != -1 || pack_index(index, ilen, games, 3) != -1) {
        printf("FAIL short index accepted\n");
        return 1;
    }
    n = pack_index(index, ilen, games, PACK_MAX_GAMES);

    FILE *list = fopen(argv[3], "r");
    char id[64], romf[128];
    int fails = 0, checked = 0;
    static uint16_t fb[160 * 144];
    while (list && fscanf(list, "%63s %127s", id, romf) == 2) {
        const pack_game *g = NULL;
        for (int i = 0; i < n; i++) {
            if (!strcmp(games[i].id, id)) {
                g = &games[i];
            }
        }
        if (!g) {
            printf("FAIL %s not in the index\n", id);
            fails++;
            continue;
        }
        uint32_t size = (uint32_t)g->banks * 16384, rlen;
        uint8_t *rom = malloc(size);
        // unpack in uneven steps, as the app does across ticks
        int ok = 0;
        for (uint32_t b = 0; b < g->banks; b += 3) {
            ok |= pack_unpack(g, b, b + 3, rom, rd, NULL);
        }
        snprintf(p, sizeof p, "%s/%s", argv[2], romf);
        uint8_t *orig = slurp(p, &rlen);
        uint8_t *pad = malloc(size);
        memset(pad, 0xFF, size);
        memcpy(pad, orig, rlen);
        int same = !ok && !memcmp(rom, pad, size) && emu_crc32(rom, size, 0) == g->crc;
        int pv = pack_preview(g, fb, rd, NULL);
        snprintf(p, sizeof p, "%s/%s.ppm", argv[4], id);
        if (!pv) {
            write_ppm(p, fb);
        }
        int e = emu_open_mem(rom, size);
        int frames_ok = !e;
        for (int f = 0; f < 600 && frames_ok; f++) {
            emu_frame(0xFF, f % 10 ? NULL : fb);
            frames_ok = !emu_error();
        }
        int cgb_flag = (g->flags & PACK_CGB) != 0;
        int cgb_hdr = rom[0x143] == 0x80 || rom[0x143] == 0xC0;
        // a snapshot taken now and loaded back draws the same next frame
        static uint8_t st[300000];
        static uint16_t fa[160 * 144], fb2[160 * 144];
        uint32_t sl = emu_state_save(st, sizeof st, g->crc);
        emu_frame(0xFF, fa);
        int sload = emu_state_load(st, sl, g->crc);
        emu_frame(0xFF, fb2);
        int snap_ok = sl && !sload && !memcmp(fa, fb2, sizeof fa) && emu_state_load(st, sl, g->crc ^ 1) == -1 &&
                      emu_state_load(st, sl - 1, g->crc) == -1;
        int good = same && !pv && frames_ok && cgb_flag == cgb_hdr && snap_ok;
        printf("%-10s %s%s%s%s%s\n", id, good ? "ok" : "FAIL", same ? "" : " rom-differs", pv ? " preview" : "",
               frames_ok ? "" : " core-error", snap_ok ? "" : " snapshot");
        fails += !good;
        checked++;
        free(rom);
        free(orig);
        free(pad);
    }
    // broken packs: a flipped byte or a cut file must be an error
    int broke_ok = 1;
    for (int mode = 0; mode < 2; mode++) {
        corrupt_file = mode == 0 ? 0 : -1;
        cut_to = mode == 1 ? 1000 : 0xFFFFFFFF;
        const pack_game *g = &games[0];
        uint8_t *rom = malloc((size_t)g->banks * 16384);
        extern int pack_unpack(const pack_game *, uint32_t, uint32_t, uint8_t *, pack_read_fn, void *);
        int r = pack_unpack(&games[1], 0, 1, rom, rd, NULL);   // another game first: reload the table
        r = pack_unpack(g, 0, g->banks, rom, rd, NULL);
        int crc_same = emu_crc32(rom, (uint32_t)g->banks * 16384, 0) == g->crc;
        if (r == 0 && crc_same) {
            broke_ok = 0;
        }
        printf("broken pack (%s): unpack %d, crc %s\n", mode ? "cut" : "flipped", r, crc_same ? "SAME" : "differs");
        free(rom);
    }
    corrupt_file = -1;
    cut_to = 0xFFFFFFFF;
    printf("%d games checked, %d failed, broken packs %s\n", checked, fails, broke_ok ? "refused" : "NOT REFUSED");
    return fails || !broke_ok || checked != n;
}
