// harness.c: runs a Game Boy ROM through app/emu.c on a computer, with no
// badge: presses keys from a script, saves screenshots, times the core.
//
//   harness <rom> <outdir> [frames] [script] [palette]
//
// script: comma-separated "frame:keys:len" presses, keys from "abseudlr"
// (A B Select start(e) Up Down Left Right). Without one, a default that gets
// most games past their title screen and then plays at random.
// Prints one JSON line: errors, timing per frame, how much of the picture
// changes, and the screenshots it wrote (PPM, frame number in the name).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../app/emu.h"

static uint16_t fb[EMU_W * EMU_H];

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", EMU_W, EMU_H);
    for (int i = 0; i < EMU_W * EMU_H; i++) {
        uint16_t be = fb[i];
        uint16_t c = (uint16_t)((be >> 8) | (be << 8));
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static uint8_t keybits(const char *k)
{
    uint8_t b = 0;
    for (; *k && *k != ':' && *k != ','; k++) {
        switch (*k) {
        case 'a': b |= 1; break;
        case 'b': b |= 2; break;
        case 's': b |= 4; break;
        case 'e': b |= 8; break;
        case 'r': b |= 16; break;
        case 'l': b |= 32; break;
        case 'u': b |= 64; break;
        case 'd': b |= 128; break;
        }
    }
    return b;
}

#define MAXP 256
static int p_at[MAXP], p_len[MAXP];
static uint8_t p_keys[MAXP];
static int n_p;

static void parse_script(const char *s)
{
    while (*s && n_p < MAXP) {
        int at = atoi(s);
        const char *c = strchr(s, ':');
        if (!c) {
            break;
        }
        uint8_t k = keybits(c + 1);
        const char *c2 = strchr(c + 1, ':');
        int len = c2 ? atoi(c2 + 1) : 6;
        p_at[n_p] = at;
        p_keys[n_p] = k;
        p_len[n_p] = len;
        n_p++;
        const char *comma = strchr(s, ',');
        if (!comma) {
            break;
        }
        s = comma + 1;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: harness <rom> <outdir> [frames] [script] [palette]\n");
        return 2;
    }
    int frames = argc > 3 ? atoi(argv[3]) : 3600;
    if (argc > 4 && argv[4][0]) {
        parse_script(argv[4]);
    } else {
        parse_script("90:e:6,180:a:6,270:e:6,360:a:6,450:e:6,540:a:6,660:a:6,780:e:6,900:a:6");
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("{\"error\":\"open\"}\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t cap = 0x8000;
    while (cap < (uint32_t)sz) {
        cap <<= 1;
    }
    uint8_t *rom = malloc(cap);
    memset(rom, 0xFF, cap);
    if (fread(rom, 1, (size_t)sz, f) != (size_t)sz) {
        printf("{\"error\":\"read\"}\n");
        return 1;
    }
    fclose(f);

    int e = emu_open_mem(rom, cap);
    if (e) {
        printf("{\"error\":\"open %d\"}\n", e);
        return 1;
    }
    emu_set_palette(argc > 5 ? atoi(argv[5]) : 0);
    emu_set_time(1700000000);

    srand(1234);
    double t_cpu = 0, t_draw = 0, worst_cpu = 0;
    int n_cpu = 0, n_draw = 0;
    uint32_t prev_sum = 0, changed = 0, lit = 0;
    uint32_t all_crc = 0;
    char shots[2048] = "";
    int shot_at[] = {150, 400, 900, 1500, 2400, 3500, 6000, 9000};
    for (int fr = 0; fr < frames; fr++) {
        uint8_t k = 0;
        for (int i = 0; i < n_p; i++) {
            if (fr >= p_at[i] && fr < p_at[i] + p_len[i]) {
                k |= p_keys[i];
            }
        }
        if (fr > 1000 && n_p && fr > p_at[n_p - 1] + 60) {
            // play at random: hold a direction for a while, tap A and B
            static uint8_t dir;
            if (fr % 20 == 0) {
                dir = (uint8_t)(16 << (rand() % 4));
            }
            k |= dir;
            if (fr % 13 < 3) {
                k |= 1;
            }
            if (fr % 29 < 2) {
                k |= 2;
            }
        }
        int draw = (fr % 4) == 0;
        double t0 = now_s();
        emu_frame((uint8_t)~k, draw ? fb : NULL);
        double dt = now_s() - t0;
        if (draw) {
            t_draw += dt;
            n_draw++;
            uint32_t s = emu_crc32((const uint8_t *)fb, sizeof fb, 0);
            all_crc = emu_crc32((const uint8_t *)fb, sizeof fb, all_crc);
            changed += s != prev_sum;
            prev_sum = s;
            for (int i = 0; i < EMU_W * EMU_H; i += 97) {
                if (fb[i] != fb[0]) {
                    lit++;
                    break;
                }
            }
        } else {
            t_cpu += dt;
            n_cpu++;
            if (dt > worst_cpu && fr > 60) {
                worst_cpu = dt;
            }
        }
        if (emu_error()) {
            printf("{\"error\":\"core %d at %04X\",\"frame\":%d}\n", emu_error(), emu_error_addr(), fr);
            return 1;
        }
        for (unsigned i = 0; i < sizeof shot_at / sizeof shot_at[0]; i++) {
            if (fr == shot_at[i] + (shot_at[i] % 4 ? 4 - shot_at[i] % 4 : 0)) {
                char p[512];
                snprintf(p, sizeof p, "%s/f%05d.ppm", argv[2], fr);
                save_ppm(p);
                size_t l = strlen(shots);
                snprintf(shots + l, sizeof shots - l, "%s\"%s\"", l ? "," : "", p);
            }
        }
    }
    printf("{\"cgb\":%d,\"cpu_us\":%.1f,\"worst_cpu_us\":%.1f,\"draw_us\":%.1f,\"changed\":%u,\"drawn\":%d,\"lit\":%u,"
           "\"state_bytes\":%u,\"frames_crc\":\"%08x\",\"shots\":[%s]}\n",
           emu_is_cgb(), n_cpu ? t_cpu / n_cpu * 1e6 : 0, worst_cpu * 1e6, n_draw ? t_draw / n_draw * 1e6 : 0,
           changed, n_draw, lit, emu_state_size(), all_crc, shots);
    return 0;
}
