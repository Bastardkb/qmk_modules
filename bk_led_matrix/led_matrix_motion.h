// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"
#include "led_matrix_display.h"

/* Trackball animations. The core (led_matrix_motion.c) turns pointer
 * reports into one bklm_motion_t per frame for the selected style, each in its
 * own led_matrix_motion_*.c.
 *
 * Panel coordinates throughout: x right, y UP (y = 0 is the bottom row).
 * Pointer reports are x right, y DOWN; the core flips y once, here. */

typedef enum {
    BKLM_MOTION_SPARKS,    /* particles sprayed from the centre in the direction of motion */
    BKLM_MOTION_FIREWORKS, /* rockets fly the way you roll and burst into falling sparks */
    BKLM_MOTION_OCEAN,     /* the sea from above: blue swells rolling the way you move */
    BKLM_MOTION_ASTEROIDS, /* the arcade game: a fixed ship you rotate */
    BKLM_MOTION_MATRIX,    /* Matrix rain (not reactive) */
    BKLM_MOTION_TETRIS,    /* Tetris playing itself (not reactive) */
    /* New styles go at the end: the saved choice is this enum's value. */
    BKLM_MOTION_STYLE_COUNT,
    /* The keymap's own styles, LED_MATRIX_MODULE_MOTION_USER_COUNT of them,
     * drawn by bklm_motion_user(). */
    BKLM_MOTION_USER = 128
} bklm_motion_style_t;

/* Selected style; starts at LED_MATRIX_MODULE_MOTION_STYLE, or the saved one. */
extern uint8_t bklm_motion_style;
const char    *bklm_motion_style_name(uint8_t style);

/* The styles LED_MATRIX_ANIMATION_NEXT steps through, in order
 * (LED_MATRIX_MODULE_MOTION_CYCLE; by default the built-in styles, then the
 * keymap's). Styles left out still work if selected. */
uint8_t bklm_motion_cycle_next(uint8_t style, bool backwards);

/* Called by a style that still has something to show after the ball stops
 * (bursts, drifting, settling): keeps it drawn for at least ms more, fading out
 * over the last LED_MATRIX_MODULE_MOTION_FADE_MS. */
void bklm_motion_linger(uint32_t ms);

/* Loads the saved style from Argos storage (stored +1 so a cleared EEPROM
 * reads as "not set"). */
void bklm_motion_style_load(void);

/* Switches style, saves it to EEPROM if it changed, and with preview plays it
 * for LED_MATRIX_MODULE_MOTION_PREVIEW_MS so it shows without touching the ball. */
void bklm_motion_style_set(uint8_t style, bool preview);

typedef struct {
    int32_t  vx, vy;       /* smoothed velocity, counts per second, panel coords (y up) */
    int16_t  dir_x, dir_y; /* last good direction, unit vector * 127, panel coords */
    uint32_t speed;        /* |velocity|, counts per second */
    uint8_t  norm;         /* speed as 0..255 of LED_MATRIX_MODULE_MOTION_FULL_SPEED */
    uint8_t  fade;         /* 255 while moving (or lingering), down to 0 as it fades out */
    uint32_t still_ms;     /* time since the last motion report */
    uint32_t dt;           /* ms since the previous frame, 1..200 */
    uint32_t now;          /* timer_read32() for this frame */
    bool     fresh;        /* first frame after the animation was idle: reset state */
    bool     idle;         /* ball still, animation kept running (LED_MATRIX_MODULE_MOTION_IDLE):
                            * vx, vy are a slow drift; styles should be calm */
} bklm_motion_t;

/* Adds pointer motion (report units: x right, y down) on the panel half. */
void bklm_motion_feed(int16_t dx, int16_t dy);

/* True while an animation is showing or fading, so frames can be paced faster.
 * Always true with LED_MATRIX_MODULE_MOTION_IDLE. */
bool bklm_motion_active(void);

/* True while the ball moves, or a style is still playing out its motion
 * (lingering, previewing): the animation then takes the panel over the layer
 * stack. When false, an idle animation only fills in behind it. */
bool bklm_motion_moving(void);

/* True for LED_MATRIX_MODULE_MOTION_PREVIEW_MS after a style is picked: the
 * preview then takes the panel over everything else, even held modifiers. */
bool bklm_motion_previewing(void);

/* Paints one frame of the selected style. False once the motion has faded out. */
bool bklm_draw_motion(RGB *pixels);

/* --- Helpers shared by the styles ---------------------------------------- */

uint32_t bklm_isqrt(uint32_t n);
uint32_t bklm_rand(void); /* xorshift32, deterministic */

/* Angles: 0..255 for a full turn, 0 = right, 64 = up (panel coordinates).
 * sin and cos return -127..127. */
int32_t bklm_sin8(uint8_t angle);
int32_t bklm_cos8(uint8_t angle);
uint8_t bklm_atan2_8(int32_t y, int32_t x);

/* Motion over dt in Q8 cells (256 = one cell), at counts_per_cell. */
int32_t bklm_motion_step(int32_t velocity, uint32_t dt, uint32_t counts_per_cell);

static inline RGB *bklm_px(RGB *pixels, uint8_t x, uint8_t y) {
    return &pixels[(uint16_t)y * BKLM_COLS + x];
}

static inline uint8_t bklm_sat8(uint32_t v) {
    return v > 255 ? 255 : (uint8_t)v;
}

/* Adds c at level/255 to a cell, saturating per channel. */
static inline void bklm_add(RGB *dst, RGB c, uint8_t level) {
    dst->r = bklm_sat8(dst->r + c.r * level / 255);
    dst->g = bklm_sat8(dst->g + c.g * level / 255);
    dst->b = bklm_sat8(dst->b + c.b * level / 255);
}

/* Scales the whole panel to level/255, e.g. by m->fade for a style that draws
 * at full brightness. */
void bklm_dim(RGB *pixels, uint8_t level);

/* Spreads c at a fractional Q8 position over the four nearest cells, so a dot
 * moving by less than a cell still moves visibly. Wraps when wrap is set,
 * otherwise drops what falls off the panel. */
void bklm_splat(RGB *pixels, int32_t x_q8, int32_t y_q8, RGB c, uint8_t level, bool wrap);

/* Style painters, one per led_matrix_motion_*.c. Each paints one frame onto
 * a cleared panel and returns false once it has nothing left to show. */
bool bklm_motion_sparks(RGB *pixels, const bklm_motion_t *m);
bool bklm_motion_fireworks(RGB *pixels, const bklm_motion_t *m);
bool bklm_motion_ocean(RGB *pixels, const bklm_motion_t *m);
bool bklm_motion_asteroids(RGB *pixels, const bklm_motion_t *m);
bool bklm_motion_matrix(RGB *pixels, const bklm_motion_t *m);
bool bklm_motion_tetris(RGB *pixels, const bklm_motion_t *m);

/* The keymap's styles: paints user style index (0..USER_COUNT-1), like the
 * painters above. */
bool bklm_motion_user(uint8_t index, RGB *pixels, const bklm_motion_t *m);
