// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Matrix rain (not reactive): one falling glyph per column with a fading green
 * trail, each column at its own speed and trail length.
 */

typedef struct {
    int32_t  head;  /* Q8 rows from the top; negative while waiting to start */
    uint16_t speed; /* Q8 rows per second */
    uint8_t  trail; /* rows */
} bklm_drop_t;

static void bklm_drop_start(bklm_drop_t *d, bool anywhere) {
    d->speed = (uint16_t)((5 + bklm_rand() % 9) * 256);  /* 5-13 rows a second */
    d->trail = (uint8_t)(4 + bklm_rand() % 8);           /* 4-11 rows */
    d->head  = anywhere ? (int32_t)(bklm_rand() % (BKLM_ROWS * 256)) : -(int32_t)(bklm_rand() % (8 * 256));
}

bool bklm_motion_matrix(RGB *pixels, const bklm_motion_t *m) {
    static bklm_drop_t drops[BKLM_COLS];
    static uint8_t     glyph[BKLM_ROWS][BKLM_COLS]; /* per-cell brightness jitter: the "glyphs" */
    static uint32_t    flicker;

    if (m->fresh) {
        for (uint8_t x = 0; x < BKLM_COLS; x++) bklm_drop_start(&drops[x], true);
        for (uint8_t y = 0; y < BKLM_ROWS; y++)
            for (uint8_t x = 0; x < BKLM_COLS; x++) glyph[y][x] = (uint8_t)(150 + bklm_rand() % 106);
    }
    flicker += m->dt;
    while (flicker >= 60) {
        flicker -= 60;
        for (uint8_t k = 0; k < 10; k++) glyph[bklm_rand() % BKLM_ROWS][bklm_rand() % BKLM_COLS] = (uint8_t)(110 + bklm_rand() % 146);
    }

    for (uint8_t x = 0; x < BKLM_COLS; x++) {
        bklm_drop_t *d = &drops[x];
        d->head += (int32_t)(d->speed * m->dt / 1000);
        if ((d->head >> 8) - d->trail > BKLM_ROWS) bklm_drop_start(d, false);
        const int32_t head = d->head >> 8;
        for (int32_t row = head - d->trail; row <= head; row++) {
            if (row < 0 || row >= BKLM_ROWS) continue;
            RGB *p = bklm_px(pixels, x, (uint8_t)(BKLM_ROWS - 1 - row));
            if (row == head) {
                *p = (RGB){.r = 190, .g = 255, .b = 190}; /* the glyph being written */
            } else {
                const uint32_t age = (uint32_t)(head - row);                     /* 1 .. trail */
                const uint32_t lv  = 255 * (d->trail + 1 - age) / (d->trail + 1); /* fades along the trail */
                *p                 = (RGB){.r = 0, .g = (uint8_t)(lv * glyph[row][x] / 255), .b = (uint8_t)(lv * glyph[row][x] / 255 / 6)};
            }
        }
    }
    bklm_dim(pixels, m->fade);
    return true;
}
