// tanks.c: Tank Siege, an original tank game made for Pocket Arcade (MIT).
//
// Guard the base at the bottom of the field. Twenty enemy tanks come down
// from the top in every stage; bricks break, steel stops shots, water stops
// tanks, trees hide them. Flashing tanks drop a bonus when hit.
//
// It draws a 160 x 144 picture like a Game Boy game, so the app shows it,
// scales it, saves it and snapshots it the same way (emu.c, tk_machine).
// The whole game is the struct g (plain data, no pointers): a snapshot is a
// copy of it. The battery RAM keeps the high score and the furthest stage.
#include <string.h>

#include "tanks.h"

// ---------------------------------------------------------------------------
// The field: 20 x 17 cells of 8 pixels under an 8-pixel bar. A brick or a
// steel cell is four 4-pixel quarters, each there or shot away.

#define FW 20
#define FH 17
#define FY 8
#define PW (FW * 8)
#define PH (FH * 8)
#define BASE_X 72
#define BASE_Y 120
#define PLAYER_X 48
#define QUEUE 20
#define MAX_EN 4
#define MAX_BUL 8
#define MAX_FX 16
#define ORPHAN 0xFF                     // a shot whose enemy is gone

enum { C_EMPTY, C_BRICK, C_STEEL, C_WATER, C_TREES, C_BASE };
enum { UP, RIGHT, DOWN, LEFT };
enum { E_BASIC, E_FAST, E_POWER, E_ARMOR };
enum { B_STAR, B_HELMET, B_CLOCK, B_SHOVEL, B_GRENADE, B_TANK, B_N };
enum { FX_SMALL = 1, FX_BIG, FX_POINTS };

typedef struct {
    int16_t x, y;            // top-left, field pixels
    uint8_t alive;           // 0 none, 1 appearing, 2 on the field
    uint8_t dir, kind, hp, speed, sub, timer, blocked, bonus, anim;
} tank_t;

typedef struct {
    int16_t x, y;            // centre
    uint8_t alive, dir, speed, owner, power;
} bullet_t;

typedef struct {
    int16_t x, y;            // centre
    uint8_t kind, t;
    uint16_t pts;
} fx_t;

typedef struct {
    uint32_t magic;
    uint32_t rng, score, next_life;
    uint16_t t, shield, freeze, shovel, respawn, bonus_t, spawn_wait, over_t, clear_t;
    uint8_t mode, prev, stage, pick, lives, level, paused, base_dead, god, new_hi;
    uint8_t spawned, killed, spawn_point, bonus_kind;
    int16_t bonus_x, bonus_y;
    uint8_t queue[QUEUE], kills[4];
    uint8_t cell[FH][FW];
    tank_t tk[1 + MAX_EN];   // 0 is the player
    bullet_t bul[MAX_BUL];
    fx_t fx[MAX_FX];
} game_t;

#define GAME_MAGIC 0x54534731u   // "TSG1"

static game_t g;

// Battery RAM: "TS", version, furthest stage, high score (little-endian).
static uint8_t ram[16];

static int ram_ok(void) { return ram[0] == 'T' && ram[1] == 'S' && ram[2] == 1; }
static uint32_t hi_score(void) { return ram[4] | ram[5] << 8 | ram[6] << 16 | (uint32_t)ram[7] << 24; }
static int max_stage(void) { return ram[3] ? ram[3] : 1; }

static void set_hi(uint32_t v)
{
    ram[4] = (uint8_t)v;
    ram[5] = (uint8_t)(v >> 8);
    ram[6] = (uint8_t)(v >> 16);
    ram[7] = (uint8_t)(v >> 24);
}

static uint32_t rnd(void)
{
    uint32_t x = g.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g.rng = x ? x : 0x9E3779B9u;
}

// ---------------------------------------------------------------------------
// Stages, drawn by hand. Each row is the left half; the right half is its
// mirror. . empty  # brick  @ steel  ~ water  % trees  u d l r half bricks
// (upper, lower, left, right). The base and its wall are added by the code.

#define N_STAGES 14
static const char *const stages[N_STAGES][FH] = {
    {"..........", "..........", ".##.##.##.", ".##.##.##.", ".##.##.##.", ".##.##.##.", ".##.##@##.",
     "..........", "##.####...", "........##", ".##.##..##", ".##.##..#.", ".##.##....", ".##.......",
     "..........", "..........", ".........."},
    {"..........", "..........", "..##..##..", "..##..##..", "..##..@@..", "..##......", "~~~~..~~..",
     "~~~~..~~..", "........##", ".%%%..##..", ".%%%..##..", ".%%%......", "..##..##..", "..##......",
     "..........", "..........", ".........."},
    {"..........", "..........", "..@@##....", "..@@##....", "..##......", "..##..####", "......#...",
     "##....#.%%", "##....#.%%", "......#...", "..##..####", "..##......", "..@@##..##", "..@@##....",
     "..........", "..........", ".........."},
    {"..........", "..........", ".###.###..", ".#.....#..", ".#.@@..#.#", ".#.....#.#", ".###.###..",
     "..........", "@@..##..##", "....##....", ".##....@@.", ".##.##....", "....##.##.", ".##....##.",
     ".##.......", ".##.......", ".........."},
    {"..........", "..........", "...~~~~...", "...~~~~...", "..........", ".##....##.", ".##.%%.##.",
     "....%%....", "~~..%%..~~", "~~......~~", "....##..##", ".##.##.##.", ".##....##.", "..........",
     "...~~.....", "...~~.....", ".........."},
    {"..........", "..........", ".#.#.#.#..", "#.#.#.#.#.", ".#.#.#.#..", "#.#.#.#.#.", "..........",
     "@@.@@.@@..", "........##", ".%%%%%%%%.", ".%%%%%%%%.", "..........", "#.#.#.#.#.", ".#.#.#.#..",
     "#.#.#.....", ".#.#......", "#.#......."},
    {"..........", "..........", "..##....##", "..##....##", "..##@@....", "..##@@....", "..........",
     "####..@@@@", "####..@@@@", "..........", "..@@..##..", "..@@..##..", "......##..", ".##.......",
     ".##.......", ".##.......", ".........."},
    {"..........", "..........", "%%%%..%%%%", "%%%%..%%%%", "%%##..##%%", "%%##..##%%", "....%%....",
     "....%%....", "##..%%..@@", "##..%%....", "%%%%..%%%%", "%%%%..%%%%", "..##..##..", "..##......",
     "%%........", "%%........", "%%........"},
    {"..........", "..........", ".@@@..@@@.", ".@##..##@.", ".@##..##@.", "..........", "..~~~~~~..",
     "........##", ".@@@..@@@.", ".@##..##@.", ".@##..##@.", "..........", "##.####.##", "..........",
     "..........", "..........", ".........."},
    {"..........", "..........", "..@@@@@@..", "..@.......", "..@.####..", "..@.#.....", "..@.#.@@@@",
     "....#.....", "######.###", "..........", "~~..@@..~~", "~~......~~", "..##..##..", "..##..##..",
     "..##......", "..##......", ".........."},
    {"..........", "..........", "##########", "..........", ".@.@.@.@..", "..........", "##########",
     "..........", ".@.@.@.@..", "..........", "##########", "..........", "....##....", ".@..##....",
     "..........", "..........", ".........."},
    {"..........", "..........", "~~~~..~~~~", "~##~..~##~", "~##~..~##~", "~~~~..~~~~", "..........",
     "..@@..%%..", "..@@..%%..", "..........", "~~~~..~~~~", "~##~..~##~", "~~~~..~~~~", "..........",
     "..........", "..........", ".........."},
    {"..........", "..........", ".##@..@##.", ".##@..@##.", ".##@..@##.", "....##....", "@@..##..@@",
     "....##....", ".##@..@##.", ".##@..@##.", ".##@..@##.", "....##....", "@@..##..@@", "..........",
     ".##.......", ".##.......", ".........."},
    {"..........", "..........", "@.@.@.@.@.", "..........", ".########.", ".#%%%%%%#.", ".#%%%%%%#.",
     ".########.", "..........", "~~~..~~~..", "........##", ".@@.##.@@.", ".@@.##....", "..........",
     "..@.......", "..@.......", ".........."},
};

// Enemies in a stage: basic, fast, power, armoured.
static const uint8_t mix[N_STAGES][4] = {
    {18, 2, 0, 0}, {14, 4, 0, 2}, {12, 4, 2, 2}, {10, 4, 4, 2}, {8, 6, 2, 4}, {6, 6, 4, 4}, {6, 4, 4, 6},
    {4, 6, 4, 6}, {4, 4, 6, 6}, {2, 6, 6, 6}, {2, 4, 6, 8}, {0, 6, 6, 8}, {0, 4, 6, 10}, {0, 4, 4, 12},
};

static const uint16_t points[4] = {100, 200, 300, 400};
static const uint8_t en_speed[4] = {8, 20, 12, 10};   // sixteenths of a pixel a frame
#define PLAYER_SPEED 14

static int loop_of(int stage) { return (stage - 1) / N_STAGES; }

static uint8_t cell_of(char ch, int mirrored)
{
    switch (ch) {
    case '#': return C_BRICK << 4 | 15;
    case '@': return C_STEEL << 4 | 15;
    case '~': return C_WATER << 4;
    case '%': return C_TREES << 4;
    case 'u': return C_BRICK << 4 | 3;
    case 'd': return C_BRICK << 4 | 12;
    case 'l': return C_BRICK << 4 | (mirrored ? 10 : 5);
    case 'r': return C_BRICK << 4 | (mirrored ? 5 : 10);
    default: return C_EMPTY << 4;
    }
}

static void base_wall(int type)
{
    // two bricks thick on top, where most shots come from
    static const uint8_t wall[12][2] = {{8, 13}, {9, 13}, {10, 13}, {11, 13}, {8, 14}, {9, 14},
                                        {10, 14}, {11, 14}, {8, 15}, {11, 15}, {8, 16}, {11, 16}};
    for (int i = 0; i < 12; i++) {
        int x = wall[i][0] * 8, y = wall[i][1] * 8, under = 0;
        for (int j = 0; j <= MAX_EN; j++) {
            const tank_t *t = &g.tk[j];
            under |= t->alive && t->x < x + 8 && x < t->x + 16 && t->y < y + 8 && y < t->y + 16;
        }
        if (!under) {                   // never build a wall on a tank: it could never get out
            g.cell[wall[i][1]][wall[i][0]] = (uint8_t)(type << 4 | 15);
        }
    }
}

static void build_stage(void)
{
    const char *const *s = stages[(g.stage - 1) % N_STAGES];
    for (int y = 0; y < FH; y++) {
        for (int x = 0; x < FW / 2; x++) {
            g.cell[y][x] = cell_of(s[y][x], 0);
            g.cell[y][FW - 1 - x] = cell_of(s[y][x], 1);
        }
    }
    for (int y = 15; y < 17; y++) {
        for (int x = 9; x < 11; x++) {
            g.cell[y][x] = C_BASE << 4;
        }
        for (int x = 6; x < 8; x++) {
            g.cell[y][x] = g.cell[y][FW - 1 - x] = 0;   // the player's start stays clear
        }
    }
    base_wall(C_BRICK);
    for (int x = 0; x < FW; x++) {                      // the enemies' starts too
        if (x < 2 || x == 9 || x == 10 || x >= 18) {
            g.cell[0][x] = g.cell[1][x] = 0;
        }
    }

    // the enemies, shuffled the same way every time for a stage
    int k = 0, si = (g.stage - 1) % N_STAGES;
    for (int e = 0; e < 4; e++) {
        for (int n = 0; n < mix[si][e] && k < QUEUE; n++) {
            g.queue[k++] = (uint8_t)e;
        }
    }
    while (k < QUEUE) {
        g.queue[k++] = E_BASIC;
    }
    uint32_t keep = g.rng;
    g.rng = 0x51ED270Bu ^ (uint32_t)g.stage * 2654435761u;
    for (int i = QUEUE - 1; i > 0; i--) {
        int j = (int)(rnd() % (uint32_t)(i + 1));
        uint8_t t = g.queue[i];
        g.queue[i] = g.queue[j];
        g.queue[j] = t;
    }
    g.rng = keep;

    memset(g.tk, 0, sizeof g.tk);
    memset(g.bul, 0, sizeof g.bul);
    memset(g.fx, 0, sizeof g.fx);
    memset(g.kills, 0, sizeof g.kills);
    g.spawned = g.killed = 0;
    g.spawn_point = 1;
    g.spawn_wait = 40;
    g.freeze = g.shovel = g.bonus_t = 0;
    g.over_t = g.clear_t = 0;
    g.base_dead = g.paused = 0;
    g.respawn = 0;
    tank_t *p = &g.tk[0];
    p->x = PLAYER_X;
    p->y = BASE_Y;
    p->dir = UP;
    p->alive = 1;
    p->timer = 48;
    p->speed = PLAYER_SPEED;
    if (g.stage > ram[3] && ram_ok()) {
        ram[3] = g.stage > 99 ? 99 : g.stage;
    }
}

// ---------------------------------------------------------------------------
// Collisions. A quarter (4 x 4 pixels) is solid for a tank if it is brick,
// steel, water or the base; for a shot, water doesn't count.

static int quarter(int qx, int qy, int shot)
{
    if (qx < 0 || qy < 0 || qx >= FW * 2 || qy >= FH * 2) {
        return C_STEEL;
    }
    uint8_t c = g.cell[qy >> 1][qx >> 1];
    int t = c >> 4;
    if (t == C_BRICK || t == C_STEEL) {
        return (c >> ((qy & 1) * 2 + (qx & 1))) & 1 ? t : 0;
    }
    if (t == C_WATER) {
        return shot ? 0 : C_WATER;
    }
    return t == C_BASE ? C_BASE : 0;
}

static int ground_free(int x, int y)
{
    if (x < 0 || y < 0 || x > PW - 16 || y > PH - 16) {
        return 0;
    }
    for (int qy = y >> 2; qy <= (y + 15) >> 2; qy++) {
        for (int qx = x >> 2; qx <= (x + 15) >> 2; qx++) {
            if (quarter(qx, qy, 0)) {
                return 0;
            }
        }
    }
    return 1;
}

static int overlap(int ax, int ay, int bx, int by) { return ax < bx + 16 && bx < ax + 16 && ay < by + 16 && by < ay + 16; }

// Would tank i at (x, y) run into another? Only a new overlap counts, so two
// tanks that touch can always drive apart.
static int tank_in_way(int i, int x, int y)
{
    const tank_t *me = &g.tk[i];
    for (int j = 0; j <= MAX_EN; j++) {
        const tank_t *o = &g.tk[j];
        if (j != i && o->alive && overlap(x, y, o->x, o->y) && !overlap(me->x, me->y, o->x, o->y)) {
            return 1;
        }
    }
    return 0;
}

static int spot_taken(int x, int y)
{
    for (int j = 0; j <= MAX_EN; j++) {
        if (g.tk[j].alive && overlap(x, y, g.tk[j].x, g.tk[j].y)) {
            return 1;
        }
    }
    return 0;
}

static const int8_t dx4[4] = {0, 1, 0, -1}, dy4[4] = {-1, 0, 1, 0};

static void drive(int i)
{
    tank_t *t = &g.tk[i];
    t->blocked = 0;
    t->sub = (uint8_t)(t->sub + t->speed);
    while (t->sub >= 16) {
        t->sub -= 16;
        int nx = t->x + dx4[t->dir], ny = t->y + dy4[t->dir];
        if (!ground_free(nx, ny) || tank_in_way(i, nx, ny)) {
            t->blocked = 1;
            t->sub = 0;
            return;
        }
        t->x = (int16_t)nx;
        t->y = (int16_t)ny;
        t->anim++;
    }
}

// Turning across lines the tank to the 8-pixel grid, so it fits gaps.
static void turn(int i, int d)
{
    tank_t *t = &g.tk[i];
    if (d == t->dir) {
        return;
    }
    if ((d ^ t->dir) & 1) {
        int x = t->x, y = t->y;
        if (d & 1) {
            y = (y + 4) & ~7;
        } else {
            x = (x + 4) & ~7;
        }
        if (ground_free(x, y) && !tank_in_way(i, x, y)) {
            t->x = (int16_t)x;
            t->y = (int16_t)y;
        }
    }
    t->dir = (uint8_t)d;
    t->sub = 0;
}

// ---------------------------------------------------------------------------
// Effects and score

static void add_fx(int x, int y, int kind, int pts)
{
    int best = 0;
    for (int i = 0; i < MAX_FX; i++) {
        if (!g.fx[i].kind) {
            best = i;
            break;
        }
        if (g.fx[i].t > g.fx[best].t) {
            best = i;                   // all busy: the oldest goes
        }
    }
    g.fx[best] = (fx_t){(int16_t)x, (int16_t)y, (uint8_t)kind, 0, (uint16_t)pts};
}

static void add_score(uint32_t n)
{
    g.score += n;
    if (g.score >= g.next_life) {
        g.next_life += g.next_life < 20000 ? 20000 : 40000;
        if (g.lives < 9) {
            g.lives++;
        }
    }
    if (g.score > hi_score() && ram_ok()) {
        set_hi(g.score);
        g.new_hi = 1;
    }
}

static void game_over(void)
{
    if (!g.over_t) {
        g.over_t = 1;
    }
}

static void drop_bonus(void)
{
    g.bonus_x = 72;                     // the middle, if every try below lands on the base
    g.bonus_y = 56;
    for (int tries = 0; tries < 24; tries++) {
        int x = (int)(rnd() % (FW - 1)) * 8, y = (int)(rnd() % (FH - 4)) * 8;
        if (overlap(x, y, BASE_X - 8, BASE_Y - 8) || overlap(x, y, BASE_X + 8, BASE_Y - 8)) {
            continue;
        }
        g.bonus_x = (int16_t)x;
        g.bonus_y = (int16_t)y;
        break;
    }
    g.bonus_kind = (uint8_t)(rnd() % B_N);
    g.bonus_t = 1200;
}

static void kill_enemy(int i, int scored)
{
    tank_t *t = &g.tk[i];
    add_fx(t->x + 8, t->y + 8, FX_BIG, 0);
    if (scored) {
        add_score(points[t->kind]);
        g.kills[t->kind]++;
        add_fx(t->x + 8, t->y + 8, FX_POINTS, points[t->kind]);
    }
    g.killed++;
    t->alive = 0;
    for (int b = 0; b < MAX_BUL; b++) {
        if (g.bul[b].alive && g.bul[b].owner == i) {
            g.bul[b].owner = ORPHAN;    // still flies, but the next tank in this slot may fire
        }
    }
}

static void hit_enemy(int i)
{
    tank_t *t = &g.tk[i];
    if (t->bonus) {
        t->bonus = 0;
        drop_bonus();
    }
    if (t->hp > 1) {
        t->hp--;
        add_fx(t->x + 8, t->y + 8, FX_SMALL, 0);
        return;
    }
    kill_enemy(i, 1);
}

static void hit_player(void)
{
    tank_t *p = &g.tk[0];
    if (g.shield || g.god) {
        return;
    }
    add_fx(p->x + 8, p->y + 8, FX_BIG, 0);
    p->alive = 0;
    g.level = 0;
    if (g.lives) {
        g.lives--;
        g.respawn = 90;
    } else {
        game_over();
    }
}

static void grenade(void)
{
    for (int i = 1; i <= MAX_EN; i++) {
        if (g.tk[i].alive == 2) {
            kill_enemy(i, 0);
        }
    }
}

static void take_bonus(void)
{
    switch (g.bonus_kind) {
    case B_STAR: g.level += g.level < 3; break;
    case B_HELMET: g.shield = 600; break;
    case B_CLOCK: g.freeze = 600; break;
    case B_SHOVEL: base_wall(C_STEEL); g.shovel = 1200; break;
    case B_GRENADE: grenade(); break;
    case B_TANK: g.lives += g.lives < 9; break;
    }
    add_score(500);
    add_fx(g.bonus_x + 8, g.bonus_y + 8, FX_POINTS, 500);
    g.bonus_t = 0;
}

// ---------------------------------------------------------------------------
// Shots

static void fire(int i)
{
    tank_t *t = &g.tk[i];
    int most = i == 0 && g.level >= 2 ? 2 : 1, n = 0, free_slot = -1;
    for (int b = 0; b < MAX_BUL; b++) {
        if (g.bul[b].alive && g.bul[b].owner == i) {
            n++;
        } else if (!g.bul[b].alive && free_slot < 0) {
            free_slot = b;
        }
    }
    if (n >= most || free_slot < 0) {
        return;
    }
    bullet_t *b = &g.bul[free_slot];
    b->alive = 1;
    b->owner = (uint8_t)i;
    b->dir = t->dir;
    b->x = (int16_t)(t->x + 8 + dx4[t->dir] * 6);
    b->y = (int16_t)(t->y + 8 + dy4[t->dir] * 6);
    if (t->dir == UP || t->dir == DOWN) {
        b->x = (int16_t)(t->x + 8);
    } else {
        b->y = (int16_t)(t->y + 8);
    }
    b->speed = i == 0 ? (g.level >= 1 ? 4 : 3) : (t->kind == E_POWER ? 4 : 2);
    b->power = i == 0 && g.level >= 3;
}

// A shot that hits a wall takes away the row of quarters across its path,
// four wide, so a tank's shots dig a tunnel it fits through.
static void blast(const bullet_t *b)
{
    int qx = b->x >> 2, qy = b->y >> 2;
    int vertical = b->dir == UP || b->dir == DOWN;
    int from = vertical ? (b->x - 7) >> 2 : (b->y - 7) >> 2;
    int to = vertical ? (b->x + 6) >> 2 : (b->y + 6) >> 2;
    for (int k = from; k <= to; k++) {
        int x = vertical ? k : qx, y = vertical ? qy : k;
        if (x < 0 || y < 0 || x >= FW * 2 || y >= FH * 2) {
            continue;
        }
        uint8_t *c = &g.cell[y >> 1][x >> 1];
        int t = *c >> 4;
        if (t == C_BRICK || (t == C_STEEL && b->power)) {
            *c = (uint8_t)(*c & ~(1 << ((y & 1) * 2 + (x & 1))));
            if (!(*c & 15)) {
                *c = 0;
            }
        }
    }
}

static void move_bullet(int bi)
{
    bullet_t *b = &g.bul[bi];
    for (int s = 0; s < b->speed && b->alive; s++) {
        b->x = (int16_t)(b->x + dx4[b->dir]);
        b->y = (int16_t)(b->y + dy4[b->dir]);
        if (b->x < 0 || b->y < 0 || b->x >= PW || b->y >= PH) {
            if (b->owner == 0) {
                add_fx(b->x, b->y, FX_SMALL, 0);
            }
            b->alive = 0;
            return;
        }
        int q = quarter(b->x >> 2, b->y >> 2, 1);
        if (q == C_BASE) {
            if (!g.base_dead && !g.god) {
                g.base_dead = (uint8_t)(1 + b->owner);   // who did it, for the tests
                add_fx(BASE_X + 8, BASE_Y + 8, FX_BIG, 0);
                game_over();
            }
            b->alive = 0;
            return;
        }
        if (q) {
            blast(b);
            add_fx(b->x, b->y, FX_SMALL, 0);
            b->alive = 0;
            return;
        }
        for (int i = 0; i <= MAX_EN; i++) {
            tank_t *t = &g.tk[i];
            if (t->alive != 2 || i == b->owner || (b->owner && i)) {
                continue;               // enemies' shots pass through enemies
            }
            if (b->x >= t->x && b->x < t->x + 16 && b->y >= t->y && b->y < t->y + 16) {
                b->alive = 0;
                if (i == 0) {
                    hit_player();
                } else {
                    hit_enemy(i);
                }
                return;
            }
        }
        for (int o = 0; o < MAX_BUL; o++) {
            bullet_t *c = &g.bul[o];
            if (o != bi && c->alive && (c->owner == 0) != (b->owner == 0) && b->x - c->x <= 3 && c->x - b->x <= 3 &&
                b->y - c->y <= 3 && c->y - b->y <= 3) {
                b->alive = c->alive = 0;   // two shots meet
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The enemies

// Tuned with a simple defending bot (stage by stage, many games): stray
// shots at the base are rare, so a player who defends can win.
#define ODDS 120                        // an enemy's shot: about 1 in this many frames
#define AT_BASE 8                       // ... that many times rarer facing the base
#define EARLY_MAX 3

// At most this many enemies on the field at once: fewer in the first stages.
static int most_enemies(void) { return g.stage <= 3 ? EARLY_MAX : MAX_EN; }

static int toward(const tank_t *t, int x, int y)
{
    int ddx = x - t->x, ddy = y - t->y;
    int ax = ddx < 0 ? -ddx : ddx, ay = ddy < 0 ? -ddy : ddy;
    if (ax > ay ? (rnd() & 3) != 0 : (rnd() & 3) == 0 && ddx) {
        return ddx > 0 ? RIGHT : LEFT;
    }
    return ddy > 0 ? DOWN : UP;
}

// Facing something, lined up with it within w pixels?
static int aimed(const tank_t *t, int x, int y, int w)
{
    int cx = t->x + 8, cy = t->y + 8;
    switch (t->dir) {
    case UP: return y < cy && x > cx - w && x < cx + w;
    case DOWN: return y > cy && x > cx - w && x < cx + w;
    case LEFT: return x < cx && y > cy - w && y < cy + w;
    default: return x > cx && y > cy - w && y < cy + w;
    }
}

static void enemy_ai(int i)
{
    tank_t *t = &g.tk[i], *p = &g.tk[0];
    if (t->timer) {
        t->timer--;
    }
    if (!t->timer) {
        int r = (int)(rnd() % 16), d;
        if (r < (g.stage <= 4 ? 3 : 4) + (loop_of(g.stage) > 0)) {
            d = toward(t, BASE_X, BASE_Y);
        } else if (r < 9 && p->alive == 2) {
            d = toward(t, p->x, p->y);
        } else {
            d = (int)(rnd() & 3);
        }
        turn(i, d);
        t->timer = (uint8_t)(24 + rnd() % 96);
    }
    drive(i);
    if (t->blocked && t->timer > 12) {
        t->timer = (uint8_t)(4 + rnd() % 12);   // stuck: turn soon
    }
    // Chance of a shot this frame: 1 in odds. More often lined up with the
    // player or stuck against a wall; more in later stages and rounds.
    int odds = ODDS - (g.stage > 10 ? 10 : g.stage) * 3 - loop_of(g.stage) * 10 - (t->kind == E_POWER ? 20 : 0);
    if (odds < 24) {
        odds = 24;                      // late rounds: as fast as it gets, before the adjustments
    }
    if (p->alive == 2 && aimed(t, p->x + 8, p->y + 8, 8)) {
        odds /= 4;
    } else if (aimed(t, BASE_X + 8, BASE_Y + 8, 16)) {
        odds *= AT_BASE;               // stray shots at the base would wear it away too soon
    } else if (t->blocked) {
        odds /= 2;
    }
    if (odds < 8) {
        odds = 8;
    }
    if (rnd() % (uint32_t)odds == 0) {
        fire(i);
    }
}

static int enemies_on_field(void)
{
    int n = 0;
    for (int i = 1; i <= MAX_EN; i++) {
        n += g.tk[i].alive != 0;
    }
    return n;
}

static void spawn_enemy(void)
{
    if (g.spawned >= QUEUE) {
        return;
    }
    if (g.spawn_wait) {
        g.spawn_wait--;
        return;
    }
    if (enemies_on_field() >= most_enemies()) {
        return;
    }
    static const int16_t sx[3] = {0, 72, 144};
    int x = sx[g.spawn_point];
    if (spot_taken(x, 0)) {
        g.spawn_point = (uint8_t)((g.spawn_point + 1) % 3);
        g.spawn_wait = 8;
        return;
    }
    for (int i = 1; i <= MAX_EN; i++) {
        tank_t *t = &g.tk[i];
        if (t->alive) {
            continue;
        }
        memset(t, 0, sizeof *t);
        t->kind = g.queue[g.spawned];
        t->hp = t->kind == E_ARMOR ? 4 : 1;
        t->speed = en_speed[t->kind];
        t->x = (int16_t)x;
        t->y = 0;
        t->dir = DOWN;
        t->alive = 1;
        t->timer = 48;
        t->bonus = g.spawned == 3 || g.spawned == 10 || g.spawned == 17;
        if (t->bonus) {
            g.bonus_t = 0;              // a new bonus tank takes the old bonus away
        }
        g.spawned++;
        g.spawn_point = (uint8_t)((g.spawn_point + 1) % 3);
        int wait = 190 - g.stage * 6;
        g.spawn_wait = (uint16_t)(g.spawned < 3 ? 30 : wait < 70 ? 70 : wait);
        return;
    }
}

// ---------------------------------------------------------------------------
// Modes

static void new_game(int stage)
{
    uint32_t seed = g.rng ^ (uint32_t)g.t * 2654435761u;
    memset(&g, 0, sizeof g);
    g.magic = GAME_MAGIC;
    g.rng = seed ? seed : 1;
    g.lives = 2;                        // and the one on the field
    g.next_life = 20000;
    g.stage = (uint8_t)stage;
    g.pick = (uint8_t)stage;
}

static void to_intro(void)
{
    g.mode = TK_INTRO;
    g.t = 0;
    build_stage();
}

static void play_update(uint8_t keys, uint8_t press)
{
    if (g.paused) {
        g.paused = !(press & TK_START);
        return;
    }
    if ((press & TK_START) && !g.over_t) {
        g.paused = 1;
        return;
    }
    g.shield -= g.shield > 0;
    g.freeze -= g.freeze > 0;
    g.bonus_t -= g.bonus_t > 0;
    if (g.shovel && !--g.shovel) {
        base_wall(C_BRICK);
    }

    // the player
    tank_t *p = &g.tk[0];
    if (!p->alive && g.respawn && !g.over_t && !--g.respawn) {
        if (spot_taken(PLAYER_X, BASE_Y)) {
            g.respawn = 1;              // wait for the start to clear
        } else {
            p->x = PLAYER_X;
            p->y = BASE_Y;
            p->dir = UP;
            p->alive = 1;
            p->timer = 48;
            p->speed = PLAYER_SPEED;
        }
    }
    if (p->alive == 1 && !--p->timer) {
        p->alive = 2;
        g.shield = 180;
    } else if (p->alive == 2 && !g.over_t) {
        static const uint8_t bit[4] = {TK_UP, TK_RIGHT, TK_DOWN, TK_LEFT};
        int d = -1;
        if (keys & bit[p->dir]) {
            d = p->dir;                 // keep going while the key is held
        } else {
            for (int k = 0; k < 4; k++) {
                if (keys & bit[k]) {
                    d = k;
                    break;
                }
            }
        }
        if (d >= 0) {
            turn(0, d);
            drive(0);
        }
        if (press & (TK_A | TK_B)) {
            fire(0);
        }
        if (g.bonus_t && overlap(p->x, p->y, g.bonus_x, g.bonus_y)) {
            take_bonus();
        }
    }

    // the enemies
    for (int i = 1; i <= MAX_EN; i++) {
        tank_t *t = &g.tk[i];
        if (t->alive == 1 && !--t->timer) {
            t->alive = 2;
            t->timer = 8;
        } else if (t->alive == 2 && !g.freeze) {
            enemy_ai(i);
        }
    }
    if (!g.over_t) {
        spawn_enemy();
    }
    for (int b = 0; b < MAX_BUL; b++) {
        if (g.bul[b].alive) {
            move_bullet(b);
        }
    }
    for (int i = 0; i < MAX_FX; i++) {
        fx_t *f = &g.fx[i];
        if (f->kind && ++f->t >= (f->kind == FX_SMALL ? 10 : f->kind == FX_BIG ? 28 : 50)) {
            f->kind = 0;
        }
    }

    if (g.over_t && ++g.over_t > 200) {
        g.mode = TK_OVER;
        g.t = 0;
    } else if (!g.over_t && g.spawned >= QUEUE && !enemies_on_field() && ++g.clear_t > 150) {
        g.mode = TK_CLEAR;
        g.t = 0;
    }
}

// ---------------------------------------------------------------------------
// Drawing

#define RGB(h) ((uint16_t)((((h) >> 8) & 0xF800) | (((h) >> 5) & 0x07E0) | (((h) >> 3) & 0x001F)))
#define BE(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
#define COL(h) BE(RGB(h))

static uint16_t *fb;

static void pset(int x, int y, uint16_t c)
{
    if ((unsigned)x < TK_W && (unsigned)y < TK_H) {
        fb[y * TK_W + x] = c;
    }
}

static void fill(int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            pset(i, j, c);
        }
    }
}

// 5 x 7 letters
static const uint8_t font[][7] = {
    {0, 0, 0, 0, 0, 0, 0},                                                  // space
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},   // A B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},   // C D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},   // E F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},   // G H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},   // I J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},   // K L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},   // M N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},   // O P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},   // Q R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},   // S T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},   // U V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},   // W X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},   // Y Z
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}, {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},   // : -
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},   // ! .
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}, {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},   // < >
    {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11}, {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},   // x =
};

static const uint8_t *glyph(char c)
{
    if (c >= '0' && c <= '9') {
        return font[1 + c - '0'];
    }
    if (c >= 'A' && c <= 'Z') {
        return font[11 + c - 'A'];
    }
    if (c >= 'a' && c <= 'z') {
        return c == 'x' ? font[43] : font[11 + c - 'a'];
    }
    switch (c) {
    case ':': return font[37];
    case '-': return font[38];
    case '!': return font[39];
    case '.': return font[40];
    case '<': return font[41];
    case '>': return font[42];
    case '=': return font[44];
    default: return font[0];
    }
}

static int text_w(const char *s, int scale) { return (int)strlen(s) * 6 * scale - scale; }

static void text(int x, int y, const char *s, uint16_t c, int scale)
{
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *gl = glyph(*s);
        for (int r = 0; r < 7; r++) {
            for (int b = 0; b < 5; b++) {
                if (gl[r] & (0x10 >> b)) {
                    fill(x + b * scale, y + r * scale, scale, scale, c);
                }
            }
        }
    }
}

static void text_c(int y, const char *s, uint16_t c, int scale) { text((TK_W - text_w(s, scale)) / 2, y, s, c, scale); }

static void number(char *out, uint32_t v, int digits)
{
    out[digits] = 0;
    for (int i = digits - 1; i >= 0; i--) {
        out[i] = (char)('0' + v % 10);
        v /= 10;
    }
}

// Tiles: 8 x 8, a letter per pixel, '.' clear.
static const char *const brick_tex[8] = {"hhhmhhhh", "rrrmrrrr", "rrrmrrrr", "mmmmmmmm",
                                         "hhhhhhhm", "rrrrrrrm", "rrrrrrrm", "mmmmmmmm"};
static const char *const steel_tex[8] = {"wwwdwwwd", "wlldwlld", "wlldwlld", "dddddddd",
                                         "wwwdwwwd", "wlldwlld", "wlldwlld", "dddddddd"};
static const char *const water_tex[8] = {"bbbbbbbb", "bllbbbbb", "lbblbbbb", "bbbbbbbb",
                                         "bbbbbllb", "bbbblbbl", "bbbbbbbb", "bbbbwbbb"};
static const char *const trees_tex[8] = {".gGg.gGg", "gGLGgGLG", "GLgGGLgG", "gGGg.gGG",
                                         ".gGgGgGg", "gGLGgGLG", "GLgGGLgG", "gGG.gGG."};

static uint16_t tex_col(char ch)
{
    switch (ch) {
    case 'h': return COL(0xE27A44);
    case 'r': return COL(0xB04A1C);
    case 'm': return COL(0x5A2008);
    case 'w': return COL(0xFFFFFF);
    case 'l': return COL(0xB8BCC8);
    case 'd': return COL(0x5E606E);
    case 'g': return COL(0x1C5E22);
    case 'G': return COL(0x2E9634);
    case 'L': return COL(0x7AD35A);
    default: return 0;
    }
}

static uint16_t water_col(char ch)
{
    return ch == 'b' ? COL(0x1E4CC8) : ch == 'l' ? COL(0x5A92FF) : COL(0xD0E8FF);
}

static void draw_cell(int cx, int cy, int as)
{
    uint8_t c = g.cell[cy][cx];
    int type = as >= 0 ? as : c >> 4, x0 = cx * 8, y0 = cy * 8 + FY;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint16_t col;
            int q = 1 << ((y >> 2) * 2 + (x >> 2));
            switch (type) {
            case C_BRICK:
                if (!(c & q)) {
                    continue;
                }
                col = tex_col(brick_tex[y][x]);
                break;
            case C_STEEL:
                if (!(c & q)) {
                    continue;
                }
                col = tex_col(steel_tex[y][x]);
                break;
            case C_WATER:
                col = water_col(water_tex[y][(x + (g.t >> 5) * 2 + cx * 3) & 7]);
                break;
            case C_TREES:
                if (trees_tex[y][x] == '.') {
                    continue;
                }
                col = tex_col(trees_tex[y][x]);
                break;
            default:
                continue;
            }
            fb[(y0 + y) * TK_W + x0 + x] = col;
        }
    }
}

static const char *const star_bm[11] = {".....#.....", ".....#.....", "....###....", "....###....",
                                        "###########", ".#########.", "..#######..", "..#######..",
                                        ".####.####.", ".###...###.", "##.......##"};

static void draw_base(void)
{
    int x0 = BASE_X, y0 = BASE_Y + FY;
    fill(x0, y0, 16, 16, g.base_dead ? COL(0x3A2A2A) : COL(0x202A40));
    fill(x0, y0, 16, 1, COL(0x56607A));
    fill(x0, y0, 1, 16, COL(0x56607A));
    for (int y = 0; y < 11; y++) {
        for (int x = 0; x < 11; x++) {
            if (star_bm[y][x] != '#') {
                continue;
            }
            uint16_t c;
            if (g.base_dead) {
                c = ((x * 7 + y * 3) % 5) ? COL(0x6A6A6A) : COL(0x1A1A1A);
            } else {
                c = y < 5 && x < 6 ? COL(0xFFF2A0) : COL(0xF2B820);
            }
            pset(x0 + 3 + x, y0 + 3 + y, c);
        }
    }
}

// Tanks: 16 x 16, barrel up, turned for the other ways. T is the track,
// drawn moving with the tank; a b c d are the hull's shades, dark to light.
static const char *const tank_bm[5][16] = {
    {".......dd.......", ".......cc.......", "TTTT...cc...TTTT", "TTTTbbbccbbbTTTT", "TTTTbccccccbTTTT",
     "TTTTbcaccacbTTTT", "TTTTbacddcabTTTT", "TTTTbacddcabTTTT", "TTTTbcaccacbTTTT", "TTTTbccaaccbTTTT",
     "TTTTbbbbbbbbTTTT", "TTTTbabbbbabTTTT", "TTTTbbbbbbbbTTTT", "TTTTbabbbbabTTTT", "TTTTaaaaaaaaTTTT",
     "TTTT........TTTT"},
    {"................", ".......cc.......", "TTTT...cc...TTTT", "TTTT...cc...TTTT", "TTTTbbbccbbbTTTT",
     "TTTTbbaccabbTTTT", "TTTTbacddcabTTTT", "TTTTbacddcabTTTT", "TTTTbbaccabbTTTT", "TTTTbbbaabbbTTTT",
     "TTTTbbbbbbbbTTTT", "TTTTcbbbbbbcTTTT", "TTTTbbbbbbbbTTTT", "TTTTaaaaaaaaTTTT", "TTTT........TTTT",
     "TTTT........TTTT"},
    {".......dd.......", ".......cc.......", "TTT....cc....TTT", "TTT....cc....TTT", "TTT..bbccbb..TTT",
     "TTT.bbaccabb.TTT", "TTT.bacddcab.TTT", "TTT.bacddcab.TTT", "TTT.bbaccabb.TTT", "TTT.bbbaabbb.TTT",
     "TTT.bbbbbbbb.TTT", "TTT..bbbbbb..TTT", "TTT..cbbbbc..TTT", "TTT...aaaa...TTT", "TTT..........TTT",
     "TTT..........TTT"},
    {"......d..d......", "......c..c......", "TTTT..c..c..TTTT", "TTTT.bcbbcb.TTTT", "TTTTbbcbbcbbTTTT",
     "TTTTbaaccaabTTTT", "TTTTbacddcabTTTT", "TTTTbacddcabTTTT", "TTTTbaaccaabTTTT", "TTTTbbaaaabbTTTT",
     "TTTTbbbbbbbbTTTT", "TTTTbcbbbbcbTTTT", "TTTTbbbbbbbbTTTT", "TTTTaaaaaaaaTTTT", "TTTT........TTTT",
     "TTTT........TTTT"},
    {"................", ".......dd.......", "TTTT...cc...TTTT", "TTTTaaaccaaaTTTT", "TTTTabbccbbaTTTT",
     "TTTabcaccacbaTTT", "TTTabacddcabaTTT", "TTTabacddcabaTTT", "TTTabcaccacbaTTT", "TTTabbcaacbbaTTT",
     "TTTTabbbbbbaTTTT", "TTTTacbccbcaTTTT", "TTTTabbbbbbaTTTT", "TTTTaaaaaaaaTTTT", "TTTT........TTTT",
     "TTTT........TTTT"},
};

// Track dark, track light, then a b c d.
static const uint32_t pal_player[6] = {0x3A2C08, 0x7A6428, 0x7A5200, 0xD29A10, 0xFFD040, 0xFFF6C8};
static const uint32_t pal_solana[6] = {0x15052E, 0x3A2A6A, 0x3A137A, 0x6B2FD9, 0xB58CFF, 0x14F195};
static int skin_solana;                     // a setting, not game state: snapshots don't carry it
static const uint32_t pal_silver[6] = {0x26262E, 0x5A5A66, 0x4A5060, 0x9098A8, 0xD0D6E0, 0xFFFFFF};
static const uint32_t pal_red[6] = {0x3A1010, 0x6A2A2A, 0x701010, 0xC02828, 0xFF6060, 0xFFD0D0};
static const uint32_t pal_armor[3][6] = {
    {0x26262E, 0x5A5A66, 0x6A3010, 0xB86030, 0xF0A070, 0xFFE8D0},   // 2 hits left
    {0x26262E, 0x5A5A66, 0x6A5010, 0xB89A30, 0xF0D870, 0xFFF8D0},   // 3
    {0x26262E, 0x5A5A66, 0x1E4A26, 0x3A8A46, 0x7CD07C, 0xE0FFE0},   // 4
};

static void draw_tank_bm(int x0, int y0, int bm, int dir, const uint32_t *pal, int phase)
{
    uint16_t c[6];
    for (int i = 0; i < 6; i++) {
        c[i] = COL(pal[i]);
    }
    for (int sy = 0; sy < 16; sy++) {
        for (int sx = 0; sx < 16; sx++) {
            char ch = tank_bm[bm][sy][sx];
            if (ch == '.') {
                continue;
            }
            uint16_t col = ch == 'T' ? (((sy + phase) & 3) == 0 || sx == 0 || sx == 15 ? c[0] : c[1]) : c[2 + ch - 'a'];
            int x, y;
            switch (dir) {
            case UP: x = sx; y = sy; break;
            case RIGHT: x = 15 - sy; y = sx; break;
            case DOWN: x = 15 - sx; y = 15 - sy; break;
            default: x = sy; y = 15 - sx; break;
            }
            pset(x0 + x, y0 + y, col);
        }
    }
}

static void draw_sparkle(int x, int y, int t)
{
    static const uint8_t size[4] = {2, 4, 7, 4};
    int s = size[(t >> 2) & 3], cx = x + 8, cy = y + 8 + FY;
    uint16_t w = COL(0xFFFFFF), b = COL(0x8CD8FF);
    for (int i = -s; i <= s; i++) {
        pset(cx + i, cy, (i > -2 && i < 2) ? w : b);
        pset(cx, cy + i, (i > -2 && i < 2) ? w : b);
    }
    for (int i = -s / 2; i <= s / 2; i++) {
        pset(cx + i, cy + i, b);
        pset(cx + i, cy - i, b);
    }
}

static void draw_tank(int i)
{
    const tank_t *t = &g.tk[i];
    if (t->alive == 1) {
        draw_sparkle(t->x, t->y, t->timer);
        return;
    }
    if (t->alive != 2) {
        return;
    }
    int phase = (t->anim >> 1) & 3;
    if (i == 0) {
        draw_tank_bm(t->x, t->y + FY, 0, t->dir, skin_solana ? pal_solana : pal_player, phase);
        if (g.shield) {
            uint16_t c = (g.t & 2) ? COL(0xFFFFFF) : COL(0x60C8FF);
            for (int k = 0; k < 72; k++) {
                if (((k + (g.t >> 1)) & 3) == 0) {
                    int x = k < 18 ? k : k < 36 ? 17 : k < 54 ? 53 - k : 0;
                    int y = k < 18 ? 0 : k < 36 ? k - 18 : k < 54 ? 17 : 71 - k;
                    pset(t->x - 1 + x, t->y - 1 + y + FY, c);
                }
            }
        }
        return;
    }
    const uint32_t *pal = pal_silver;
    if (t->bonus && (g.t & 8)) {
        pal = pal_red;
    } else if (t->kind == E_ARMOR && t->hp > 1) {
        pal = pal_armor[t->hp - 2];
    }
    draw_tank_bm(t->x, t->y + FY, 1 + t->kind, t->dir, pal, phase);
}

// Bonus icons, 12 x 12: # the colour, + light, o dark.
static const char *const bonus_bm[B_N][12] = {
    {"............", ".....##.....", ".....##.....", "....####....", "############", ".##########.",
     "..########..", "..########..", ".####..####.", ".###....###.", "##........##", "............"},
    {"............", "....####....", "..########..", ".####++####.", ".###++#####.", "############",
     "############", "############", "oooooooooooo", ".##......##.", ".##......##.", "............"},
    {"...######...", "..#++++++#..", ".#++++#+++#.", "#+++++#++++#", "#+++++#++++#", "#+++++###++#",
     "#++++++++++#", "#++++++++++#", ".#++++++++#.", "..#++++++#..", "...######...", "............"},
    {"..........#.", ".........###", "..........#.", ".........#..", "........#...", ".......#....",
     "..+++.#.....", ".+++++......", ".++++++.....", ".+++++......", "..+++.......", "............"},
    {"........#...", ".......#.#..", "......#.....", "....####....", "..########..", ".##+#######.",
     ".#++#######.", ".##########.", ".##########.", "..########..", "...######...", "............"},
    {".....##.....", ".....##.....", "##..####..##", "##.######.##", "##.##++##.##", "##.#+oo+#.##",
     "##.#+oo+#.##", "##.##++##.##", "##.######.##", "##........##", "............", "............"},
};
static const uint32_t bonus_col[B_N][3] = {
    {0xF2B820, 0xFFF2A0, 0x7A5200}, {0xB8BCC8, 0xFFFFFF, 0x5E606E}, {0xD0D6E0, 0x5A92FF, 0x26262E},
    {0x9A6A3A, 0xB8BCC8, 0x4A3010}, {0x2E7A34, 0x9AE08A, 0x103010}, {0xF2B820, 0xFFF2A0, 0x7A5200},
};

static void draw_bonus(void)
{
    if (!g.bonus_t || (g.bonus_t < 180 && (g.t & 8))) {
        return;
    }
    int x0 = g.bonus_x, y0 = g.bonus_y + FY;
    fill(x0, y0, 16, 16, COL(0x141430));
    uint16_t edge = (g.t & 16) ? COL(0xFFFFFF) : COL(0xFF5A5A);
    fill(x0, y0, 16, 1, edge);
    fill(x0, y0 + 15, 16, 1, edge);
    fill(x0, y0, 1, 16, edge);
    fill(x0 + 15, y0, 1, 16, edge);
    const uint32_t *c = bonus_col[g.bonus_kind];
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 12; x++) {
            char ch = bonus_bm[g.bonus_kind][y][x];
            if (ch != '.') {
                pset(x0 + 2 + x, y0 + 2 + y, COL(c[ch == '#' ? 0 : ch == '+' ? 1 : 2]));
            }
        }
    }
}

static void draw_bullets(void)
{
    uint16_t c = COL(0xFFFFFF), e = COL(0xC8C8C8);
    for (int i = 0; i < MAX_BUL; i++) {
        const bullet_t *b = &g.bul[i];
        if (!b->alive) {
            continue;
        }
        int x = b->x, y = b->y + FY;
        fill(x - 1, y - 1, 3, 3, e);
        pset(x, y, c);
        pset(x + dx4[b->dir], y + dy4[b->dir], c);
    }
}

// Tiny digits for the points: 3 x 5.
static const uint16_t digits3[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};

static void draw_fx(void)
{
    static const uint32_t ring[5] = {0xFFFFFF, 0xFFE040, 0xFF8A20, 0xC02810, 0x601008};
    for (int i = 0; i < MAX_FX; i++) {
        const fx_t *f = &g.fx[i];
        if (!f->kind) {
            continue;
        }
        int cx = f->x, cy = f->y + FY;
        if (f->kind == FX_POINTS) {
            if (f->t < 20) {
                continue;               // after the explosion
            }
            char s[6];
            number(s, f->pts, f->pts >= 1000 ? 4 : 3);
            int w = (int)strlen(s) * 4 - 1, x = cx - w / 2, y = cy - 3;
            for (const char *p = s; *p; p++, x += 4) {
                uint16_t bits = digits3[*p - '0'];
                for (int k = 0; k < 15; k++) {
                    if (bits & (0x4000 >> k)) {
                        pset(x + k % 3, y + k / 3, COL(0xFFFFFF));
                    }
                }
            }
            continue;
        }
        int life = f->kind == FX_SMALL ? 10 : 28, peak = f->kind == FX_SMALL ? 5 : 12;
        int half = life / 2, r = f->t < half ? 1 + f->t * peak / half : peak * (life - f->t) / half + 1;
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                int d2 = x * x + y * y;
                if (d2 > r * r) {
                    continue;
                }
                // ragged edge: skip some pixels near the rim
                if (d2 > (r - 2) * (r - 2) && ((x * 13 + y * 7 + f->t) & 3) == 0) {
                    continue;
                }
                int band = d2 * 5 / (r * r + 1) + f->t * 4 / life;
                pset(cx + x, cy + y, COL(ring[band > 4 ? 4 : band]));
            }
        }
    }
}

static void draw_hud(void)
{
    fill(0, 0, TK_W, FY, COL(0x1C1C28));
    char s[12];
    uint16_t w = COL(0xE8E8F0), y = COL(0xFFD040), dim = COL(0x8A8AA0);
    // lives: a little yellow tank
    fill(2, 2, 2, 5, y);
    fill(7, 2, 2, 5, y);
    fill(4, 3, 3, 4, COL(0xD29A10));
    fill(5, 1, 1, 2, y);
    s[0] = 'x';
    s[1] = (char)('0' + (g.lives > 9 ? 9 : g.lives));
    s[2] = 0;
    text(11, 1, s, w, 1);
    number(s, g.score, 6);
    text(34, 1, s, w, 1);
    // enemies still to come
    int left = QUEUE - g.killed;
    fill(84, 2, 7, 5, dim);
    fill(87, 1, 1, 2, dim);
    number(s, (uint32_t)(left < 0 ? 0 : left), 2);
    text(93, 1, s, w, 1);
    // the stage: a flag
    fill(118, 1, 1, 7, w);
    fill(119, 1, 5, 3, COL(0xFF5A5A));
    number(s, g.stage, g.stage >= 100 ? 3 : 2);
    text(126, 1, s, w, 1);
    if (g.freeze) {
        text(146, 1, "F", COL(0x8CD8FF), 1);
    }
}

static void draw_field(void)
{
    fill(0, FY, PW, PH, 0);
    for (int y = 0; y < FH; y++) {
        for (int x = 0; x < FW; x++) {
            int t = g.cell[y][x] >> 4;
            if (t == C_BRICK || t == C_STEEL || t == C_WATER) {
                int as = -1;
                // the shovel's steel wall blinks back to brick before it ends
                if (t == C_STEEL && g.shovel && g.shovel < 120 && (g.t & 8) && y >= 13 && x >= 8 && x <= 11) {
                    as = C_BRICK;
                }
                draw_cell(x, y, as);
            }
        }
    }
    draw_base();
    draw_bonus();
    for (int i = 0; i <= MAX_EN; i++) {
        draw_tank(i);
    }
    draw_bullets();
    for (int y = 0; y < FH; y++) {
        for (int x = 0; x < FW; x++) {
            if (g.cell[y][x] >> 4 == C_TREES) {
                draw_cell(x, y, -1);   // trees over the tanks
            }
        }
    }
    draw_fx();
    draw_hud();
}

// Big letters made of bricks (each pixel of a letter a 4 x 4 brick quarter).
static void brick_text(int y, const char *s)
{
    int x0 = (TK_W - text_w(s, 4)) / 2;
    for (int n = 0; s[n]; n++) {
        const uint8_t *gl = glyph(s[n]);
        for (int r = 0; r < 7; r++) {
            for (int b = 0; b < 5; b++) {
                if (!(gl[r] & (0x10 >> b))) {
                    continue;
                }
                int px = x0 + n * 24 + b * 4, py = y + r * 4;
                for (int j = 0; j < 4; j++) {
                    for (int i = 0; i < 4; i++) {
                        pset(px + i, py + j, tex_col(brick_tex[(r & 1) * 4 + j][(b & 1) * 4 + i]));
                    }
                }
            }
        }
    }
}

static void draw_title(void)
{
    fill(0, 0, TK_W, TK_H, 0);
    brick_text(8, "TANK");
    brick_text(40, "SIEGE");
    draw_tank_bm(30, 76, 0, RIGHT, pal_player, (g.t >> 2) & 3);
    draw_tank_bm(114, 76, 4, LEFT, pal_armor[2], (g.t >> 2) & 3);
    for (int x = 50; x < 110; x += 6) {
        pset(x + ((g.t >> 1) % 6), 84, COL(0x6A6A80));
    }
    char s[24];
    if (max_stage() > 1) {
        char n[4];
        number(n, g.pick, 2);
        s[0] = 0;
        strcat(s, "STAGE < ");
        strcat(s, n);
        strcat(s, " >");
        text_c(100, s, COL(0xE8E8F0), 1);
    }
    if ((g.t >> 5) & 1) {
        text_c(113, "PRESS A OR START", COL(0xFFD040), 1);
    }
    strcpy(s, "HI ");
    number(s + 3, hi_score(), 6);
    text_c(130, s, COL(0x8A8AA0), 1);
}

static void draw_curtain(int cover)
{
    if (cover <= 0) {
        return;
    }
    fill(0, 0, TK_W, cover, COL(0x6C6C78));
    fill(0, TK_H - cover, TK_W, cover, COL(0x6C6C78));
}

static void draw_intro(void)
{
    draw_field();
    int t = g.t, cover = t < 16 ? t * 72 / 16 : t < 84 ? 72 : (100 - t) * 72 / 16;
    draw_curtain(cover);
    if (t >= 16 && t < 84) {
        char s[12] = "STAGE ";
        number(s + 6, g.stage, g.stage >= 100 ? 3 : 2);
        text_c(60, s, COL(0x101018), 2);
        text_c(84, "SELECT: FIRE", COL(0x202030), 1);
        text_c(96, "TAP BOTH KEYS: PAUSE", COL(0x202030), 1);
    }
}

static void draw_clear(void)
{
    fill(0, 0, TK_W, TK_H, 0);
    char s[24], n[8];
    strcpy(s, "STAGE ");
    number(s + 6, g.stage, g.stage >= 100 ? 3 : 2);
    strcat(s, " CLEAR");
    text_c(8, s, COL(0xFFD040), 1);
    int total = 0;
    for (int k = 0; k < 4; k++) {
        int start = 20 + k * 40;
        if (g.t < start) {
            break;
        }
        int shown = (g.t - start) / 5;
        if (shown > g.kills[k]) {
            shown = g.kills[k];
        }
        total += shown;
        int y = 26 + k * 20;
        draw_tank_bm(18, y, 1 + k, UP, k == E_ARMOR ? pal_armor[2] : pal_silver, 0);
        strcpy(s, "x");
        number(n, (uint32_t)shown, 2);
        strcat(s, n);
        text(42, y + 5, s, COL(0xE8E8F0), 1);
        number(n, (uint32_t)(shown * points[k]), 4);
        strcpy(s, n);
        strcat(s, " PTS");
        text(150 - text_w(s, 1), y + 5, s, COL(0xE8E8F0), 1);
    }
    if (g.t >= 180) {
        fill(18, 108, 124, 1, COL(0x6A6A80));
        strcpy(s, "TOTAL ");
        number(n, (uint32_t)total, 2);
        strcat(s, n);
        text_c(113, s, COL(0xFFFFFF), 1);
        strcpy(s, "SCORE ");
        number(s + 6, g.score, 6);
        text_c(128, s, COL(0xFFD040), 1);
    }
}

static void draw_over(void)
{
    fill(0, 0, TK_W, TK_H, 0);
    brick_text(14, "GAME");
    brick_text(46, "OVER");
    char s[24];
    strcpy(s, "SCORE ");
    number(s + 6, g.score, 6);
    text_c(88, s, COL(0xE8E8F0), 1);
    if (g.new_hi) {
        if ((g.t >> 4) & 1) {
            text_c(102, "NEW HIGH SCORE!", COL(0xFFD040), 1);
        }
    } else {
        strcpy(s, "HI ");
        number(s + 3, hi_score(), 6);
        text_c(102, s, COL(0x8A8AA0), 1);
    }
    if (g.t > 60 && ((g.t >> 5) & 1)) {
        text_c(124, "PRESS A", COL(0xFFD040), 1);
    }
}

static void render(void)
{
    switch (g.mode) {
    case TK_TITLE: draw_title(); break;
    case TK_INTRO: draw_intro(); break;
    case TK_CLEAR: draw_clear(); break;
    case TK_OVER: draw_over(); break;
    default:
        draw_field();
        if (g.over_t) {
            int y = TK_H - (int)g.over_t * 2;
            text_c(y < 60 ? 60 : y, "GAME OVER", COL(0xFF4A30), 2);
        } else if (g.paused && ((g.t >> 4) & 1)) {
            text_c(68, "PAUSE", COL(0xFF4A30), 2);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// The machine

void tk_reset(void)
{
    uint32_t seed = g.rng;
    memset(&g, 0, sizeof g);
    g.magic = GAME_MAGIC;
    g.rng = seed ? seed : 0x2545F491u;
    g.mode = TK_TITLE;
    g.pick = 1;
}

void tk_frame(uint8_t keys, uint16_t *out)
{
    if (!ram_ok()) {
        memset(ram, 0, sizeof ram);
        ram[0] = 'T';
        ram[1] = 'S';
        ram[2] = 1;
        ram[3] = 1;
    }
    if (g.magic != GAME_MAGIC) {
        tk_reset();
    }
    uint8_t press = (uint8_t)(keys & ~g.prev);
    g.prev = keys;
    g.t++;
    rnd();
    switch (g.mode) {
    case TK_TITLE: {
        int most = max_stage() > 99 ? 99 : max_stage();
        if ((press & TK_LEFT) && g.pick > 1) {
            g.pick--;
        }
        if ((press & TK_RIGHT) && g.pick < most) {
            g.pick++;
        }
        if (g.pick < 1 || g.pick > most) {
            g.pick = 1;
        }
        if (press & (TK_A | TK_START)) {
            new_game(g.pick);
            to_intro();
        }
        break;
    }
    case TK_INTRO:
        if (g.t >= 100) {
            g.mode = TK_PLAY;
            g.t = 0;
        }
        break;
    case TK_PLAY:
        play_update(keys, press);
        break;
    case TK_CLEAR:
        if ((g.t >= 180 && (press & (TK_A | TK_START))) || g.t >= 330) {
            if (g.stage < 255) {
                g.stage++;
            }
            to_intro();
        } else if (g.t < 180 && (press & (TK_A | TK_START))) {
            g.t = 180;                  // skip the counting
        }
        break;
    case TK_OVER:
        if (g.t > 60 && (press & (TK_A | TK_START))) {
            uint8_t pick = g.pick;
            tk_reset();
            g.pick = pick;
        }
        break;
    }
    if (out) {
        fb = out;
        render();
        fb = NULL;
    }
}

const emu_native tk_machine = {
    tk_reset, tk_frame, ram, sizeof ram, &g, sizeof g, 0x54530100u ^ (uint32_t)sizeof(game_t)   // bump the 01 when game_t changes,
};

// ---------------------------------------------------------------------------
// For tools/tank_test.c

int tk_mode(void) { return g.mode; }
int tk_stage(void) { return g.stage; }
int tk_stages(void) { return N_STAGES; }
uint32_t tk_score(void) { return g.score; }
int tk_tank(int i, int *x, int *y, int *dir)
{
    if (i < 0 || i > MAX_EN || g.tk[i].alive != 2) {
        return 0;
    }
    *x = g.tk[i].x;
    *y = g.tk[i].y;
    *dir = g.tk[i].dir;
    return 1;
}

int tk_lives(void) { return g.base_dead ? -g.base_dead : g.lives; }
int tk_level(void) { return g.level; }
uint32_t tk_hi(void) { return ram_ok() ? hi_score() : 0; }
int tk_furthest(void) { return ram_ok() ? max_stage() : 1; }
void tk_set_skin(int solana) { skin_solana = solana != 0; }
int tk_cell(int x, int y) { return g.cell[y][x]; }

void tk_cheat(int what)
{
    if (what == 1) {
        grenade();
    } else if (what == 2) {
        g.god = 1;
    }
}

void tk_load_stage(int stage)
{
    new_game(stage);
    build_stage();
    g.mode = TK_PLAY;
    g.t = 0;
}

const char *tk_check(void)
{
    for (int s = 0; s < N_STAGES; s++) {
        for (int y = 0; y < FH; y++) {
            if (strlen(stages[s][y]) != FW / 2) {
                return "a stage row isn't 10 wide";
            }
        }
    }
    for (int b = 0; b < 5; b++) {
        for (int y = 0; y < 16; y++) {
            if (strlen(tank_bm[b][y]) != 16) {
                return "a tank row isn't 16 wide";
            }
        }
    }
    for (int b = 0; b < B_N; b++) {
        for (int y = 0; y < 12; y++) {
            if (strlen(bonus_bm[b][y]) != 12) {
                return "a bonus row isn't 12 wide";
            }
        }
    }
    if (g.mode != TK_PLAY) {
        return NULL;
    }
    for (int i = 0; i <= MAX_EN; i++) {
        const tank_t *t = &g.tk[i];
        if (!t->alive) {
            continue;
        }
        if (!ground_free(t->x, t->y)) {
            return "a tank is inside a wall";
        }
        for (int j = i + 1; j <= MAX_EN; j++) {
            if (g.tk[j].alive && overlap(t->x, t->y, g.tk[j].x, g.tk[j].y)) {
                return "two tanks overlap";
            }
        }
        if (t->dir > 3 || (i && t->kind > 3) || (i && (t->hp < 1 || t->hp > 4))) {
            return "a tank has a bad field";
        }
    }
    for (int b = 0; b < MAX_BUL; b++) {
        const bullet_t *u = &g.bul[b];
        if (u->alive && (u->x < 0 || u->y < 0 || u->x >= PW || u->y >= PH || (u->owner > MAX_EN && u->owner != ORPHAN))) {
            return "a shot is off the field";
        }
    }
    if (g.spawned > QUEUE || g.killed > g.spawned || g.level > 3 || g.lives > 9) {
        return "a count is out of range";
    }
    if (g.killed + enemies_on_field() != g.spawned) {
        return "enemies lost count";
    }
    return NULL;
}
