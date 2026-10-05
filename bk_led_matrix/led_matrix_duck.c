// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_display.h"
#include "led_matrix_duck.h"
#include "led_matrix_motion.h"

#define BKLM_DUCK_W 7
#define BKLM_DUCK_H 5

/* Water fills the bottom rows; the duck's lowest row sits in the surface row. */
#define BKLM_DUCK_WATER_ROWS 4
#define BKLM_DUCK_SURFACE    (BKLM_ROWS - BKLM_DUCK_WATER_ROWS)
#define BKLM_DUCK_TOP_ROW    (BKLM_DUCK_SURFACE - BKLM_DUCK_H + 1)

#define BKLM_DUCK_SPEED 384 /* Q8 columns per second */

static const RGB bklm_duck_outline = {0, 0, 0};
static const RGB bklm_duck_body    = {156, 131, 4};  /* #9c8304 */
static const RGB bklm_duck_beak    = {255, 138, 61}; /* #ff8a3d */
static const RGB bklm_duck_water   = {62, 207, 255}; /* #3ecfff */
static const RGB bklm_duck_deep    = {20, 90, 160};
static const RGB bklm_duck_shimmer = {130, 225, 255};
static const RGB bklm_duck_wake    = {200, 240, 255};

/* Facing left; mirrored to face right. Row 0 is the top.
 *   '.' transparent, '#' the eye, 'O' body, '*' beak (against the body, so it
 *   does not float off the head) */
static const char bklm_duck_sprite[BKLM_DUCK_H][BKLM_DUCK_W + 1] = {
    ".OOO...",
    "*O#O...",
    "..OO..O",
    ".OOOOOO",
    "..OOOO.",
};

/* Row 0 is the top of the panel; framebuffer y = 0 is the bottom LED. */
static RGB *bklm_duck_pixel_at(RGB *pixels, uint8_t x, uint8_t row) {
    const uint8_t y = (BKLM_ROWS - 1) - row;
    return &pixels[(uint16_t)y * BKLM_COLS + x];
}

/* A small duck paddling back and forth on the water, now and then stopping or
 * turning round, with a wake behind it while it swims. It is the resting state,
 * so it paints unconditionally: the composer calls it only once every
 * indicator has declined the frame. No-op when pixels is NULL. */
void bklm_draw_swimming_duck(RGB *pixels) {
    static int32_t  x = 2 * 256; /* Q8 column of the sprite's left edge */
    static int8_t   dir = -1;    /* -1 left, +1 right */
    static uint32_t last, pause_ms, next_ms;
    if (pixels == NULL) {
        return;
    }

    const uint32_t now = timer_read32();
    const uint32_t dt  = MIN(now - last, 200);
    last               = now;

    /* Every few seconds: stop for a moment, or turn round. */
    if (next_ms > dt) {
        next_ms -= dt;
    } else {
        next_ms = 2000 + bklm_rand() % 3000;
        if (bklm_rand() & 1) {
            pause_ms = 800 + bklm_rand() % 1200;
        } else {
            dir = (int8_t)-dir;
        }
    }
    const bool swimming = pause_ms == 0;
    if (!swimming) {
        pause_ms = pause_ms > dt ? pause_ms - dt : 0;
    } else {
        x += dir * (int32_t)(BKLM_DUCK_SPEED * dt / 1000);
        if (x <= 0 || x >= (BKLM_COLS - BKLM_DUCK_W) * 256) {
            x   = CONSTRAIN(x, 0, (BKLM_COLS - BKLM_DUCK_W) * 256);
            dir = (int8_t)-dir;
        }
    }
    const uint8_t col0 = (uint8_t)((x + 128) >> 8);

    /* Water: the surface shimmers, deeper rows darken. */
    for (uint8_t row = BKLM_DUCK_SURFACE; row < BKLM_ROWS; row++) {
        for (uint8_t c = 0; c < BKLM_COLS; c++) {
            const bool light = row == BKLM_DUCK_SURFACE && ((c + now / 400) % 4) == 0;
            *bklm_duck_pixel_at(pixels, c, row) = light ? bklm_duck_shimmer : (row > BKLM_DUCK_SURFACE + 1 ? bklm_duck_deep : bklm_duck_water);
        }
    }

    /* The wake: two cells behind the tail on the surface, while swimming. */
    if (swimming) {
        for (uint8_t k = 1; k <= 2; k++) {
            const int16_t c = dir < 0 ? col0 + BKLM_DUCK_W - 1 + k : col0 - k;
            if (c >= 0 && c < BKLM_COLS) *bklm_duck_pixel_at(pixels, (uint8_t)c, BKLM_DUCK_SURFACE) = bklm_duck_wake;
        }
    }

    /* Paddling bobs quickly; resting, slowly. */
    const uint8_t top = BKLM_DUCK_TOP_ROW - (uint8_t)((now / (swimming ? 300 : 1000)) & 1);
    for (uint8_t row = 0; row < BKLM_DUCK_H; row++) {
        for (uint8_t col = 0; col < BKLM_DUCK_W; col++) {
            const RGB *color;
            switch (bklm_duck_sprite[row][dir < 0 ? col : BKLM_DUCK_W - 1 - col]) {
                case '#':
                    color = &bklm_duck_outline;
                    break;
                case 'O':
                    color = &bklm_duck_body;
                    break;
                case '*':
                    color = &bklm_duck_beak;
                    break;
                default:
                    continue;
            }
            *bklm_duck_pixel_at(pixels, col0 + col, top + row) = *color;
        }
    }
}
