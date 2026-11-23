// tank_test.c: plays Tank Siege (app/tanks.c) on a computer.
//
//   cc -O1 -g -fsanitize=address,undefined -Iapp tools/tank_test.c app/tanks.c -o build/tank_test
//   build/tank_test build/tanks
//
// Checks: the drawings and stages are well formed; every stage lets the
// enemies reach the base and the player reach the top (bricks can be shot,
// steel and water can't); random games with the state checked every frame;
// every stage cleared in a row with a cheat, and the stages after them; a
// snapshot plays on exactly as the original; the time a frame takes.
// Saves pictures of every screen into the folder given.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tanks.h"

static uint16_t fb[TK_W * TK_H];
static int fails;

#define FAIL(...) do { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } while (0)

static uint32_t rng = 12345;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void shot(const char *dir, const char *name)
{
    char p[256];
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    FILE *f = fopen(p, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", TK_W, TK_H);
    for (int i = 0; i < TK_W * TK_H; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void frame(uint8_t keys)
{
    tk_frame(keys, fb);
    const char *e = tk_check();
    if (e) {
        FAIL("%s (stage %d, mode %d)", e, tk_stage(), tk_mode());
        exit(1);
    }
}

static void press(uint8_t k)
{
    frame(k);
    frame(0);
}

// ---------------------------------------------------------------------------
// Can a 16 x 16 tank get from a to b on the 8-pixel grid? Bricks and trees
// count as open (a tank shoots through bricks), steel, water and the base
// don't.
static int open_at(int x, int y)
{
    for (int cy = y / 8; cy < y / 8 + 2; cy++) {
        for (int cx = x / 8; cx < x / 8 + 2; cx++) {
            int t = tk_cell(cx, cy) >> 4;
            if (t == 2 || t == 3 || t == 5) {
                return 0;
            }
        }
    }
    return 1;
}

static int reach(int ax, int ay, int bx, int by)
{
    static uint8_t seen[19][16];
    static int q[19 * 16][2];
    memset(seen, 0, sizeof seen);
    int h = 0, n = 0;
    if (!open_at(ax, ay)) {
        return 0;
    }
    q[n][0] = ax / 8, q[n][1] = ay / 8, n++;
    seen[ax / 8][ay / 8] = 1;
    while (h < n) {
        int x = q[h][0], y = q[h][1];
        h++;
        if (bx < 0 ? (y == 12 && x >= 7 && x <= 11) || ((x == 6 || x == 12) && y >= 12) : x == bx / 8 && y == by / 8) {
            return 1;                   // bx < 0: anywhere next to the base's wall
        }
        static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++) {
            int nx = x + d[k][0], ny = y + d[k][1];
            if (nx < 0 || ny < 0 || nx > 18 || ny > 15 || seen[nx][ny] || !open_at(nx * 8, ny * 8)) {
                continue;
            }
            seen[nx][ny] = 1;
            q[n][0] = nx, q[n][1] = ny, n++;
        }
    }
    return 0;
}

static void test_stages(const char *dir)
{
    for (int s = 1; s <= tk_stages(); s++) {
        tk_load_stage(s);
        static const int sx[3] = {0, 72, 144};
        for (int k = 0; k < 3; k++) {
            if (!reach(sx[k], 0, -1, 0)) {
                FAIL("stage %d: enemies from start %d can't reach the base", s, k);
            }
        }
        if (!reach(48, 120, 72, 0)) {
            FAIL("stage %d: the player can't reach the top", s);
        }
        frame(0);
        char name[16];
        snprintf(name, sizeof name, "stage%02d", s);
        shot(dir, name);
    }
}

// ---------------------------------------------------------------------------

static uint8_t random_keys(uint8_t held)
{
    static const uint8_t dirs[4] = {TK_UP, TK_RIGHT, TK_DOWN, TK_LEFT};
    if (rnd() % 24 == 0) {
        held = dirs[rnd() % 4];
        if (rnd() % 5 == 0) {
            held = 0;
        }
    }
    uint8_t k = held;
    if (rnd() % 6 == 0) {
        k |= TK_A;
    }
    if (rnd() % 400 == 0) {
        k |= TK_START;
    }
    return k;
}

static void test_random(void)
{
    int clears = 0, overs = 0, best_stage = 0;
    uint32_t best = 0;
    for (int seed = 1; seed <= 24; seed++) {
        rng = (uint32_t)seed * 7919u;
        tk_reset();
        frame(0);
        press(TK_A);
        uint8_t held = 0;
        int last_mode = -1;
        for (int f = 0; f < 30000; f++) {
            held = random_keys(held);
            uint8_t k = held;
            if (tk_mode() != TK_PLAY && f % 50 == 0) {
                k = TK_A;               // move on from the screens between stages
            }
            frame(k);
            int m = tk_mode();
            if (m != last_mode) {
                clears += m == TK_CLEAR;
                overs += m == TK_OVER;
                last_mode = m;
            }
            if (tk_stage() > best_stage) {
                best_stage = tk_stage();
            }
            if (tk_score() > best) {
                best = tk_score();
            }
        }
    }
    printf("random play: 24 games x 30000 frames, %d stages cleared, %d game overs, best stage %d, best score %u\n",
           clears, overs, best_stage, (unsigned)best);
    if (!overs) {
        FAIL("random play never lost: the enemies don't fight");
    }
}

static void test_all_stages(const char *dir)
{
    tk_load_stage(1);
    tk_cheat(2);
    uint8_t held = 0;
    int stage = 1, frames_in_stage = 0, shots_taken = 0;
    for (int f = 0; f < 400000 && stage <= 32; f++) {
        held = random_keys(held);
        if (tk_mode() == TK_PLAY && f % 25 == 0) {
            tk_cheat(1);
        }
        frame(tk_mode() == TK_CLEAR && f % 40 == 0 ? TK_A : held);
        frames_in_stage++;
        if (tk_mode() == TK_CLEAR && !shots_taken && stage == 1) {
            for (int i = 0; i < 200; i++) {
                frame(0);
            }
            shot(dir, "clear");
            shots_taken = 1;
        }
        if (tk_stage() != stage) {
            if (tk_stage() != stage + 1) {
                FAIL("stage went from %d to %d", stage, tk_stage());
            }
            if (getenv("TK_VERBOSE")) printf("stage %d: %d frames\n", stage, frames_in_stage);
            if (frames_in_stage > 9000) {   // random keys pause the game now and then
                FAIL("stage %d took %d frames with every enemy blown up", stage, frames_in_stage);
            }
            stage = tk_stage();
            frames_in_stage = 0;
            tk_cheat(2);
        }
        if (tk_mode() == TK_OVER) {
            FAIL("lost at stage %d while the player can't be hit (the base fell?)", stage);
            tk_load_stage(stage);
            tk_cheat(2);
        }
    }
    printf("cleared stages 1 to %d in a row\n", stage - 1);
    if (stage <= 32) {
        FAIL("only reached stage %d of 32", stage);
    }
}

// A simple defender: from above the base, line up with the nearest enemy and
// shoot, never down at its own base. The stages were tuned so it often wins
// the first stages: a person who defends can.
static uint8_t defender(int f)
{
    int px, py, pd;
    if (!tk_tank(0, &px, &py, &pd)) {
        return 0;
    }
    int best = -1, bd = 1 << 30, bx = 0, by = 0;
    for (int i = 1; i <= 4; i++) {
        int x, y, d;
        if (!tk_tank(i, &x, &y, &d)) {
            continue;
        }
        int dd = abs(x - px) + abs(y - py) * 2 - y * 2;
        if (dd < bd) {
            bd = dd, best = i, bx = x, by = y;
        }
    }
    if (best < 0) {
        return py > 96 ? TK_UP : px < 72 ? TK_RIGHT : px > 72 ? TK_LEFT : 0;
    }
    uint8_t k;
    if (abs(bx - px) < 6) {
        k = by < py ? TK_UP : TK_DOWN;
        if ((f & 1) && !(by > py && px > 52 && px < 92)) {
            k |= TK_A;
        }
        return k;
    }
    if (abs(by - py) < 6) {
        return (uint8_t)((bx < px ? TK_LEFT : TK_RIGHT) | ((f & 1) ? TK_A : 0));
    }
    if (py > 100) {
        return (uint8_t)(TK_UP | ((f & 7) == 0 ? TK_A : 0));
    }
    return (uint8_t)((bx < px ? TK_LEFT : TK_RIGHT) | ((f & 15) == 0 ? TK_A : 0));
}

static const char *preview_dir = ".";
static int preview_taken;

static void test_defender(void)
{
    int total = 0, first = 0;
    for (int s = 1; s <= tk_stages(); s++) {
        int wins = 0;
        for (int seed = 0; seed < 10; seed++) {
            tk_reset();
            for (int i = 0; i < seed * 53 + 1; i++) {
                tk_frame(0, NULL);
            }
            tk_load_stage(s);
            for (int f = 0; tk_mode() == TK_PLAY && f < 12000; f++) {
                frame(defender(f));
                int x, y, d, foes = 0;
                for (int i = 1; i <= 4; i++) {
                    foes += tk_tank(i, &x, &y, &d);
                }
                if (s == 2 && seed == 0 && f >= 700 && !preview_taken && foes >= 3 && tk_tank(0, &x, &y, &d)) {
                    shot(preview_dir, "preview");   // the picture in the app's list
                    preview_taken = 1;
                }
            }
            wins += tk_mode() == TK_CLEAR;
        }
        total += wins;
        if (s <= 3) {
            first += wins;
        }
    }
    printf("defender bot: cleared %d of 30 tries at stages 1-3, %d of %d in all\n", first, total, tk_stages() * 10);
    if (first < 10) {
        FAIL("the first stages got too hard: the defender bot cleared only %d of 30", first);
    }
}

static void test_snapshot_exact(void)
{
    // Record keys, play, restore, replay the same keys: the same pictures.
    static uint8_t keys[2000];
    tk_reset();
    frame(0);
    press(TK_A);
    rng = 4242;
    uint8_t held = 0;
    for (int f = 0; f < 900; f++) {
        held = random_keys(held);
        frame(held);
    }
    static uint8_t saved[65536], ram_saved[64];
    memcpy(saved, tk_machine.state, tk_machine.state_size);
    memcpy(ram_saved, tk_machine.ram, tk_machine.ram_size);
    for (int f = 0; f < 2000; f++) {
        held = random_keys(held);
        keys[f] = held;
    }
    uint32_t a = 0, b = 0;
    for (int f = 0; f < 2000; f++) {
        frame(keys[f]);
        for (int i = 0; i < TK_W * TK_H; i += 5) {
            a = a * 31 + fb[i];
        }
    }
    memcpy(tk_machine.state, saved, tk_machine.state_size);
    memcpy(tk_machine.ram, ram_saved, tk_machine.ram_size);
    for (int f = 0; f < 2000; f++) {
        frame(keys[f]);
        for (int i = 0; i < TK_W * TK_H; i += 5) {
            b = b * 31 + fb[i];
        }
    }
    if (a != b) {
        FAIL("a snapshot doesn't play on the same");
    } else {
        printf("snapshot: 2000 frames after a restore match exactly (state %u bytes)\n", (unsigned)tk_machine.state_size);
    }
}

static void test_screens(const char *dir)
{
    tk_reset();
    for (int i = 0; i < 40; i++) {
        frame(0);
    }
    shot(dir, "title");
    press(TK_A);
    for (int i = 0; i < 50; i++) {
        frame(0);
    }
    shot(dir, "intro");
    for (int i = 0; i < 60; i++) {
        frame(0);
    }
    // drive up and shoot a while, for the preview
    rng = 777;
    for (int i = 0; i < 520; i++) {
        uint8_t k = i < 90 ? TK_UP : i < 140 ? TK_LEFT : i < 300 ? TK_UP : i < 360 ? TK_RIGHT : TK_UP;
        if (i % 20 == 0 && i > 60) {
            k |= TK_A;
        }
        if (tk_mode() != TK_PLAY) {
            FAIL("the game ended in the first %d frames of driving about", i);
            break;
        }
        frame(k);
    }
    shot(dir, "play");
    press(TK_START);
    for (int i = 0; i < 20; i++) {
        frame(0);
    }
    shot(dir, "pause");
    press(TK_START);
    // lose: stand still until the enemies win
    for (int i = 0; i < 60000 && tk_mode() == TK_PLAY; i++) {
        frame(0);
    }
    for (int i = 0; i < 90; i++) {
        frame(0);
    }
    shot(dir, "over");
    if (tk_mode() != TK_OVER) {
        FAIL("standing still never lost");
    }
    press(TK_A);
    if (tk_mode() != TK_TITLE) {
        FAIL("game over didn't go back to the title");
    }
}

static void test_speed(void)
{
    tk_load_stage(9);
    tk_cheat(2);
    uint8_t held = 0;
    clock_t t0 = clock();
    int n = 20000;
    for (int f = 0; f < n; f++) {
        held = random_keys(held);
        tk_frame(held, fb);
    }
    double us = (double)(clock() - t0) * 1e6 / CLOCKS_PER_SEC / n;
    printf("speed: %.1f us a frame with drawing (this computer, sanitizers on)\n", us);
}

// Pocket Arcade's secrets read the game from outside: the star level, the
// high score and the furthest stage; all secrets paint the player's tank.
static void test_secret_hooks(void)
{
    static uint16_t a[TK_W * TK_H], b[TK_W * TK_H];
    tk_reset();
    tk_load_stage(1);
    for (int i = 0; i < 90; i++) {
        tk_frame(0, NULL);              // the tank appears out of its sparkle
    }
    if (tk_level() != 0) {
        FAIL("a new life starts with no stars (level %d)", tk_level());
    }
    tk_set_skin(0);
    tk_frame(0, a);
    tk_set_skin(1);
    tk_frame(0, b);
    tk_set_skin(0);
    int diff = 0;
    for (int i = 0; i < TK_W * TK_H; i++) {
        diff += a[i] != b[i];
    }
    if (diff < 20) {
        FAIL("the Solana skin should repaint the player's tank (%d pixels differ)", diff);
    }
    if (tk_furthest() < 1) {
        FAIL("furthest stage %d", tk_furthest());
    }
    tk_cheat(2);
    for (int n = 0; n < 6000 && tk_mode() != TK_CLEAR; n++) {
        if (n % 60 == 0) {
            tk_cheat(1);
        }
        tk_frame(0, NULL);
    }
    if (tk_mode() != TK_CLEAR) {
        FAIL("stage 1 not cleared with the cheats (mode %d)", tk_mode());
    } else if (tk_hi() < tk_score() || tk_hi() == 0) {
        FAIL("high score %u should cover the score %u", (unsigned)tk_hi(), (unsigned)tk_score());
    }
    printf("secret hooks: skin repaints %d pixels, hi %u, furthest %d\n", diff, (unsigned)tk_hi(), tk_furthest());
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    tk_reset();
    frame(0);                           // tk_check() checks the drawings and stages in every mode
    test_stages(dir);
    test_screens(dir);
    test_random();
    test_all_stages(dir);
    preview_dir = dir;
    test_defender();
    if (!preview_taken) {
        FAIL("no picture for the preview");
    }
    test_snapshot_exact();
    test_speed();
    test_secret_hooks();
    printf(fails ? "%d FAILED\n" : "all tank tests passed\n", fails);
    return fails != 0;
}
