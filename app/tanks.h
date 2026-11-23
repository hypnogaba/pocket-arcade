// tanks.h: Tank Siege, an original tank game made for Pocket Arcade.
// No badge API here: the app runs it through emu.c (tk_machine), and
// tools/tank_test.c plays it on a computer.
#pragma once
#include <stdint.h>

#include "emu.h"

#define TK_W 160
#define TK_H 144

// Keys, active high (the joypad's bits).
enum { TK_A = 1, TK_B = 2, TK_SELECT = 4, TK_START = 8, TK_RIGHT = 16, TK_LEFT = 32, TK_UP = 64, TK_DOWN = 128 };

extern const emu_native tk_machine;

void tk_reset(void);                          // the title screen; the high score stays
void tk_frame(uint8_t keys, uint16_t *fb);   // one 60th of a second; fb: 160 x 144 big-endian RGB565, or NULL

// For tools/tank_test.c.
enum { TK_TITLE, TK_INTRO, TK_PLAY, TK_CLEAR, TK_OVER };
int tk_mode(void);
int tk_stage(void);
int tk_stages(void);                          // stages drawn by hand; after them they repeat, harder
uint32_t tk_score(void);
int tk_lives(void);
int tk_tank(int i, int *x, int *y, int *dir);  // tank i (0 the player): 1 on the field, then where; else 0                           // spare lives; once the base has fallen -1 (the player shot it) or below (an enemy)
const char *tk_check(void);                   // NULL, or what is wrong with the state
void tk_cheat(int what);                      // 1: destroy every enemy on the field, 2: the player and the base can't be hit
int tk_cell(int x, int y);                    // the field: type << 4 | brick quarters
void tk_load_stage(int stage);                // a stage, straight into play
