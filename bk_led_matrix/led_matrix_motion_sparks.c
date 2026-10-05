// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Sparks: particles sprayed from the centre along the motion, in a cone about
 * 30 degrees either side; faster rolling sprays more, further.
 */

#define BKLM_SPARKS 40

typedef struct {
    int32_t  x, y;   /* Q8 cells */
    int32_t  vx, vy; /* Q8 cells per second */
    uint16_t age, life;
    uint8_t  hue;
    bool     alive;
} bklm_spark_t;

bool bklm_motion_sparks(RGB *pixels, const bklm_motion_t *m) {
    static bklm_spark_t sparks[BKLM_SPARKS];
    static uint32_t     spawn_acc; /* spark-milliseconds owed */
    static uint8_t      base_hue;

    if (m->fresh) {
        memset(sparks, 0, sizeof(sparks));
        spawn_acc = 0;
    }
    base_hue += (uint8_t)(m->dt / 8);

    /* Spawn while the ball moves: 15 to 80 sparks a second; idle, a trickle of 6. */
    if (m->still_ms < 60) {
        spawn_acc += (15 + 65 * (uint32_t)m->norm / 255) * m->dt;
    } else if (m->idle) {
        spawn_acc += 6 * m->dt;
    }
    for (uint8_t i = 0; i < BKLM_SPARKS && spawn_acc >= 1000; i++) {
        if (sparks[i].alive) continue;
        spawn_acc -= 1000;
        /* 10 to 26 cells per second, plus a little random. */
        const int32_t speed  = (10 + 16 * (int32_t)m->norm / 255) * 256 + (int32_t)(bklm_rand() % 1024);
        const int32_t spread = (int32_t)(bklm_rand() % 129) - 64; /* -0.5..0.5 of the speed, sideways */
        bklm_spark_t *s      = &sparks[i];
        s->x                 = BKLM_COLS * 128 + (int32_t)(bklm_rand() % 256) - 128;
        s->y                 = BKLM_ROWS * 128 + (int32_t)(bklm_rand() % 256) - 128;
        s->vx                = (m->dir_x * speed + -m->dir_y * spread * speed / 128) / 127;
        s->vy                = (m->dir_y * speed + m->dir_x * spread * speed / 128) / 127;
        s->age               = 0;
        s->life              = (uint16_t)(300 + bklm_rand() % 300);
        s->hue               = (uint8_t)(base_hue + bklm_rand() % 128); /* half the colour wheel at once */
        s->alive             = true;
    }
    spawn_acc = MIN(spawn_acc, 1000); /* all slots full: don't bank a burst */

    bool any = false;
    for (uint8_t i = 0; i < BKLM_SPARKS; i++) {
        bklm_spark_t *s = &sparks[i];
        if (!s->alive) continue;
        s->age += (uint16_t)m->dt;
        s->x += s->vx * (int32_t)m->dt / 1000;
        s->y += s->vy * (int32_t)m->dt / 1000;
        if (s->age >= s->life || s->x < -256 || s->y < -256 || s->x > BKLM_COLS * 256 + 256 || s->y > BKLM_ROWS * 256 + 256) {
            s->alive = false;
            continue;
        }
        const uint32_t left  = (uint32_t)(s->life - s->age) * 255 / s->life;
        const uint8_t  level = (uint8_t)(left * m->fade / 255);
        const RGB c = hsv_to_rgb((HSV){.h = s->hue, .s = 255, .v = 255});
        bklm_splat(pixels, s->x, s->y, c, level, false);
        /* A short tail: where it was 40 ms ago, at half strength. */
        bklm_splat(pixels, s->x - s->vx * 40 / 1000, s->y - s->vy * 40 / 1000, c, level / 2, false);
        any = true;
    }
    /* Keep the frame (and the faster pacing) while sparks are still flying. */
    return any || m->still_ms < 60 || m->idle;
}
