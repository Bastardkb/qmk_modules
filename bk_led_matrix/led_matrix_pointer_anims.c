// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_pointer_anims.h"
#include "led_matrix_motion.h"

#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
#    include "bk_pointing_modes.h"

static int32_t bklm_pa_x, bklm_pa_y; /* motion since the last frame, report coords */

void bklm_pointer_anim_feed(int16_t dx, int16_t dy) {
    bklm_pa_x += dx;
    bklm_pa_y += dy;
}

/* Cell (x, row) with rows counted from the top; nothing off the panel. */
static void bklm_pa_set(RGB *pixels, int16_t x, int16_t row, RGB c) {
    if (x >= 0 && x < BKLM_COLS && row >= 0 && row < BKLM_ROWS) *bklm_px(pixels, (uint8_t)x, (uint8_t)(BKLM_ROWS - 1 - row)) = c;
}

/* --- Drag-scroll: a scroll wheel seen from above ------------------------ */

#    define BKLM_WHEEL_X0     4
#    define BKLM_WHEEL_W      4
#    define BKLM_WHEEL_R      1792 /* Q8 rows, the wheel's radius as seen from above */
#    define BKLM_WHEEL_RIDGES 20   /* per turn */
#    define BKLM_WHEEL_GAP    (65536 / BKLM_WHEEL_RIDGES) /* between ridges, Q8 angle */

/* The tread: ridges on a cylinder, so they bunch up towards both ends and are
 * lit brightest where the wheel faces up. One scroll step turns the wheel one
 * ridge, like a notch. Sideways steps light a chevron on that side. */
static void bklm_anim_dragscroll(RGB *pixels, int32_t mx, int32_t my, uint32_t dt, bool fresh) {
    static int32_t  target, shown; /* Q8 angle the wheel has turned through */
    static int8_t   tilt;          /* last sideways step: -1 left, 1 right */
    static uint32_t tilt_ms;       /* time left to show it */
    if (fresh) {
        target = shown = 0;
        tilt_ms        = 0;
    }
    target += my * BKLM_WHEEL_GAP / BKLM_SCROLL_STEP;
    shown += (target - shown) * (int32_t)MIN(dt, 80) / 80;
    if (mx != 0) {
        tilt    = mx > 0 ? 1 : -1;
        tilt_ms = 400;
    }
    tilt_ms = tilt_ms > dt ? tilt_ms - dt : 0;

    for (int16_t row = 0; row < BKLM_ROWS; row++) {
        const int32_t dy = row * 256 + 128 - BKLM_ROWS * 128;
        if (dy <= -BKLM_WHEEL_R || dy >= BKLM_WHEEL_R) continue;
        const int32_t facing = (int32_t)bklm_isqrt((uint32_t)(BKLM_WHEEL_R * BKLM_WHEEL_R - dy * dy)); /* Q8, 0 at the ends */
        const int32_t angle  = (int32_t)(int8_t)bklm_atan2_8(dy, facing) * 256;
        int32_t       off    = (angle - shown) % BKLM_WHEEL_GAP;
        if (off < 0) off += BKLM_WHEEL_GAP;
        const int32_t near  = MIN(off, BKLM_WHEEL_GAP - off);
        const int32_t ridge = MAX(0, 255 - near * 255 / (BKLM_WHEEL_GAP / 3));    /* soft ridge edges */
        const int32_t light = 60 + 195 * facing / BKLM_WHEEL_R;
        const RGB     c     = {(uint8_t)((30 + 170 * ridge / 255) * light / 255), (uint8_t)((30 + 175 * ridge / 255) * light / 255),
                               (uint8_t)((40 + 180 * ridge / 255) * light / 255)};
        for (int16_t x = BKLM_WHEEL_X0; x < BKLM_WHEEL_X0 + BKLM_WHEEL_W; x++) bklm_pa_set(pixels, x, row, c);
    }

    if (tilt_ms) {
        const uint8_t lv = (uint8_t)(255 * tilt_ms / 400);
        const RGB     c  = {(uint8_t)(62 * lv / 255), (uint8_t)(207 * lv / 255), (uint8_t)(255 * lv / 255)};
        const int16_t tip = tilt > 0 ? BKLM_COLS - 1 : 0, back = tilt > 0 ? BKLM_COLS - 2 : 1;
        for (int16_t k = 0; k < 2; k++) {
            bklm_pa_set(pixels, tip, 7 + k, c);
            bklm_pa_set(pixels, back, 6 + 3 * k, c);
            bklm_pa_set(pixels, back, 7 + k, c);
        }
    }
}

/* --- Sniping: a scope tracking a target the ball moves -------------------- */

#    define BKLM_SCOPE_Q8_PER_COUNT 5   /* target movement; sniping runs at a low DPI */
#    define BKLM_SCOPE_RADIUS       900 /* Q8 cells, 3.5 */
#    define BKLM_SCOPE_MARGIN       640 /* keeps the target, and most of the scope, on the panel */

static void bklm_anim_scope(RGB *pixels, int32_t mx, int32_t my, uint32_t dt, bool fresh) {
    static int32_t tx, ty, sx, sy; /* Q8 cells from the top-left: target and scope centre */
    if (fresh) {
        tx = sx = BKLM_COLS * 128;
        ty = sy = BKLM_ROWS * 128;
    }
    tx = CONSTRAIN(tx + mx * BKLM_SCOPE_Q8_PER_COUNT, BKLM_SCOPE_MARGIN, BKLM_COLS * 256 - BKLM_SCOPE_MARGIN);
    ty = CONSTRAIN(ty + my * BKLM_SCOPE_Q8_PER_COUNT, BKLM_SCOPE_MARGIN, BKLM_ROWS * 256 - BKLM_SCOPE_MARGIN);
    sx += (tx - sx) * (int32_t)dt / 250;
    sy += (ty - sy) * (int32_t)dt / 250;

    /* Ring and crosshair, four samples a cell for a round ring. */
    for (int16_t row = 0; row < BKLM_ROWS; row++) {
        for (int16_t x = 0; x < BKLM_COLS; x++) {
            uint8_t ring = 0, cross = 0;
            for (uint8_t q = 0; q < 4; q++) {
                const int32_t dx = x * 256 + 64 + (q & 1) * 128 - sx;
                const int32_t dy = row * 256 + 64 + (q >> 1) * 128 - sy;
                const int32_t d  = (int32_t)bklm_isqrt((uint32_t)(dx * dx + dy * dy));
                ring += d > BKLM_SCOPE_RADIUS - 160 && d <= BKLM_SCOPE_RADIUS + 96;
                cross += d < BKLM_SCOPE_RADIUS - 160 && d > 240 && (abs(dx) < 128 || abs(dy) < 128);
            }
            if (ring) {
                bklm_pa_set(pixels, x, row, (RGB){62 * ring / 4, 207 * ring / 4, 255 * ring / 4});
            } else if (cross) {
                bklm_pa_set(pixels, x, row, (RGB){120 * cross / 4, 130 * cross / 4, 140 * cross / 4});
            }
        }
    }
    bklm_splat(pixels, tx, BKLM_ROWS * 256 - ty, (RGB){255, 40, 30}, 255, false);
}

bool bklm_pointer_anim_active(uint8_t mode) {
    return mode == MODE_DRAGSCROLL || mode == MODE_SNIPING;
}

bool bklm_pointer_anim_paint(RGB *pixels, uint8_t mode) {
    static uint32_t last;
    static uint8_t  last_mode;
    const uint32_t  dt    = MIN(timer_elapsed32(last), 200);
    const bool      fresh = mode != last_mode || timer_elapsed32(last) > 500;
    const int32_t   mx = fresh ? 0 : bklm_pa_x, my = fresh ? 0 : bklm_pa_y; /* drop motion from before the mode */
    bklm_pa_x = bklm_pa_y = 0;
    last      = timer_read32();
    last_mode = mode;

    switch (mode) {
        case MODE_DRAGSCROLL:
            bklm_anim_dragscroll(pixels, mx, my, dt, fresh);
            return true;
        case MODE_SNIPING:
            bklm_anim_scope(pixels, mx, my, dt, fresh);
            return true;
        default:
            return false;
    }
}
#else
void bklm_pointer_anim_feed(int16_t dx, int16_t dy) {}

bool bklm_pointer_anim_active(uint8_t mode) {
    return false;
}
#endif
