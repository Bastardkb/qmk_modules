// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Ocean from above: a main swell along the motion plus a shorter one crossing
 * at 25 degrees, so the surface does not read as stripes. Height maps to
 * colour, from navy troughs to white foam.
 */

/* Trochoid (Gerstner) profile, not a sine: real crests are sharp and troughs
 * broad. One wavelength in 32 steps: 255 at the crest, 0 in the trough. */
static const uint8_t bklm_ocean_profile[32] = {
    255, 229, 192, 160, 132, 108, 87, 70, 54, 41, 30, 20, 13, 7, 3, 1,
    0,   1,   3,   7,   13,  20,  30, 41, 54, 70, 87, 108, 132, 160, 192, 229,
};

/* Height (0..255) to colour. */
static const struct {
    uint8_t h;
    RGB     c;
} bklm_ocean_ramp[] = {
    {0, {.r = 0, .g = 6, .b = 36}},      {90, {.r = 0, .g = 34, .b = 110}},    {165, {.r = 0, .g = 95, .b = 185}},
    {215, {.r = 30, .g = 165, .b = 225}}, {245, {.r = 170, .g = 225, .b = 250}}, {255, {.r = 235, .g = 248, .b = 255}},
};

static RGB bklm_ocean_color(uint8_t h) {
    uint8_t i = 1;
    while (i < ARRAY_SIZE(bklm_ocean_ramp) - 1 && h > bklm_ocean_ramp[i].h) i++;
    const uint8_t  h0 = bklm_ocean_ramp[i - 1].h, h1 = bklm_ocean_ramp[i].h;
    const RGB      a = bklm_ocean_ramp[i - 1].c, b = bklm_ocean_ramp[i].c;
    const uint32_t f = h1 > h0 ? (uint32_t)(MAX(h, h0) - h0) * 256 / (h1 - h0) : 256;
    return (RGB){
        .r = (uint8_t)(a.r + ((int32_t)b.r - a.r) * (int32_t)f / 256),
        .g = (uint8_t)(a.g + ((int32_t)b.g - a.g) * (int32_t)f / 256),
        .b = (uint8_t)(a.b + ((int32_t)b.b - a.b) * (int32_t)f / 256),
    };
}

/* Profile value at a phase (1/65536 of a wavelength), interpolated. */
static uint32_t bklm_ocean_wave(uint16_t phase) {
    const uint8_t  i = (uint8_t)(phase >> 11), j = (uint8_t)((i + 1) & 31);
    const uint32_t f = (phase >> 3) & 255;
    return (bklm_ocean_profile[i] * (256 - f) + bklm_ocean_profile[j] * f) >> 8;
}

bool bklm_motion_ocean(RGB *pixels, const bklm_motion_t *m) {
    static uint16_t ph1, ph2;

    /* Long waves travel faster than short ones, as on real water. Waves per
     * second: main 0.7 to 2.3, cross 1.0 to 3.2, with speed. */
    const uint32_t mw1 = 700 + 1600 * (uint32_t)m->norm / 255;
    const uint32_t mw2 = 1000 + 2200 * (uint32_t)m->norm / 255;
    ph1 += (uint16_t)(mw1 * m->dt * 1024 / 15625); /* * 65536 / 1000000, without overflowing */
    ph2 += (uint16_t)(mw2 * m->dt * 1024 / 15625);

    const int32_t c25 = 115, s25 = 54; /* cos, sin * 127 */
    const int32_t d2x = (m->dir_x * c25 - m->dir_y * s25) / 127;
    const int32_t d2y = (m->dir_x * s25 + m->dir_y * c25) / 127;

    /* Wavelengths in half-cells * 127 (unit vectors are * 127). */
    const int32_t wl1 = 2 * LED_MATRIX_MODULE_OCEAN_LENGTH * 127;
    const int32_t wl2 = wl1 * 5 / 9;

    for (uint8_t y = 0; y < BKLM_ROWS; y++) {
        for (uint8_t x = 0; x < BKLM_COLS; x++) {
            const int32_t cx = 2 * x - (BKLM_COLS - 1), cy = 2 * y - (BKLM_ROWS - 1);
            /* Crests move towards +direction as the phase grows. */
            const uint16_t t1 = (uint16_t)((cx * m->dir_x + cy * m->dir_y) * 65536 / wl1 - ph1);
            const uint16_t t2 = (uint16_t)((cx * d2x + cy * d2y) * 65536 / wl2 - ph2);
            uint32_t       h  = MIN((bklm_ocean_wave(t1) * 200 + bklm_ocean_wave(t2) * 100) / 255, 255);

            RGB c = bklm_ocean_color((uint8_t)h);
            if (h > 200 && (bklm_rand() & 15) == 0) {
                c = (RGB){.r = 255, .g = 255, .b = 255};
            }
            *bklm_px(pixels, x, y) = (RGB){
                .r = (uint8_t)(c.r * m->fade / 255),
                .g = (uint8_t)(c.g * m->fade / 255),
                .b = (uint8_t)(c.b * m->fade / 255),
            };
        }
    }
    return true;
}
