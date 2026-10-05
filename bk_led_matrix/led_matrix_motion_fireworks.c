// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Fireworks: rockets launch from behind the centre along the motion and burst
 * into rings of embers that fall under gravity. Bursts carry on after the ball
 * stops (bklm_motion_linger) until the last ember dies.
 */

#define BKLM_FW_PARTICLES 72
#define BKLM_FW_EMBERS 16

enum { BKLM_FW_FREE, BKLM_FW_ROCKET, BKLM_FW_EMBER };

typedef struct {
    int32_t  x, y;   /* Q8 cells */
    int32_t  vx, vy; /* Q8 cells per second */
    uint16_t age, life;
    uint8_t  hue;
    uint8_t  kind;
} bklm_fw_t;

/* cos(2*pi*k/16) * 127; sin is the same table shifted by 4. */
static const int8_t bklm_fw_cos[16] = {127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49, 0, 49, 90, 117};

static bklm_fw_t *bklm_fw_alloc(bklm_fw_t *p) {
    for (uint8_t i = 0; i < BKLM_FW_PARTICLES; i++) {
        if (p[i].kind == BKLM_FW_FREE) return &p[i];
    }
    return NULL;
}

static void bklm_fw_burst(bklm_fw_t *p, const bklm_fw_t *rocket) {
    const uint8_t turn = (uint8_t)(bklm_rand() % 16); /* rotate the ring a little each time */
    for (uint8_t k = 0; k < BKLM_FW_EMBERS; k++) {
        bklm_fw_t *e = bklm_fw_alloc(p);
        if (e == NULL) return;
        const uint8_t a     = (uint8_t)((k * 16 / BKLM_FW_EMBERS + turn) & 15);
        const int32_t speed = 7 * 256 + (int32_t)(bklm_rand() % (4 * 256)); /* 7-11 cells/s */
        e->x                = rocket->x;
        e->y                = rocket->y;
        e->vx               = rocket->vx / 4 + bklm_fw_cos[a] * speed / 127;
        e->vy               = rocket->vy / 4 + bklm_fw_cos[(a + 12) & 15] * speed / 127;
        e->age              = 0;
        e->life             = (uint16_t)(600 + bklm_rand() % 400);
        e->hue              = (uint8_t)(rocket->hue + bklm_rand() % 24);
        e->kind             = BKLM_FW_EMBER;
    }
}

bool bklm_motion_fireworks(RGB *pixels, const bklm_motion_t *m) {
    static bklm_fw_t p[BKLM_FW_PARTICLES];
    static uint32_t  launch_acc;

    if (m->fresh) {
        launch_acc = 1000; /* first rocket straight away */
    }

    /* Launch while the ball moves: 2 to 6 rockets a second. */
    const bool moving = m->still_ms < 60;
    if (moving) {
        launch_acc += (2 + 4 * (uint32_t)m->norm / 255) * m->dt;
    } else if (m->idle) {
        launch_acc += m->dt * 11 / 10; /* idle: about one rocket a second */
    }
    while (launch_acc >= 1000) {
        launch_acc -= 1000;
        bklm_fw_t *r = bklm_fw_alloc(p);
        if (r == NULL) break;
        const int32_t speed  = (10 + 8 * (int32_t)m->norm / 255) * 256;
        const int32_t spread = (int32_t)(bklm_rand() % 53) - 26; /* +-0.2 of the speed, sideways */
        r->x                 = BKLM_COLS * 128 - m->dir_x * 5 * 256 / 127 + (int32_t)(bklm_rand() % 512) - 256;
        r->y                 = BKLM_ROWS * 128 - m->dir_y * 5 * 256 / 127 + (int32_t)(bklm_rand() % 512) - 256;
        r->vx                = (m->dir_x * speed + -m->dir_y * spread * speed / 128) / 127;
        r->vy                = (m->dir_y * speed + m->dir_x * spread * speed / 128) / 127;
        r->age               = 0;
        r->life              = (uint16_t)(350 + bklm_rand() % 250);
        r->hue               = (uint8_t)bklm_rand();
        r->kind              = BKLM_FW_ROCKET;
    }

    const int32_t dt   = (int32_t)m->dt;
    bool          live = false;
    for (uint8_t i = 0; i < BKLM_FW_PARTICLES; i++) {
        bklm_fw_t *q = &p[i];
        if (q->kind == BKLM_FW_FREE) continue;
        q->age += (uint16_t)dt;
        if (q->kind == BKLM_FW_EMBER) {
            q->vy -= 9 * 256 * dt / 1000;        /* gravity, 9 cells/s^2, down */
            q->vx -= q->vx * 3 * dt / 2000;      /* air drag */
            q->vy -= q->vy * 3 * dt / 2000;
        }
        q->x += q->vx * dt / 1000;
        q->y += q->vy * dt / 1000;

        const bool off = q->x < -512 || q->x > BKLM_COLS * 256 + 512 || q->y < -512 || q->y > BKLM_ROWS * 256 + 512;
        if (q->age >= q->life || off) {
            if (q->kind == BKLM_FW_ROCKET && !off) {
                const bklm_fw_t rocket = *q;
                q->kind                = BKLM_FW_FREE; /* free its slot for an ember first */
                bklm_fw_burst(p, &rocket);
            } else {
                q->kind = BKLM_FW_FREE;
            }
            continue;
        }
        live = true;

        if (q->kind == BKLM_FW_ROCKET) {
            bklm_splat(pixels, q->x, q->y, (RGB){.r = 255, .g = 230, .b = 180}, 255, false);
            bklm_splat(pixels, q->x - q->vx * 50 / 1000, q->y - q->vy * 50 / 1000, (RGB){.r = 255, .g = 120, .b = 30}, 110, false);
        } else {
            uint32_t left = (uint32_t)(q->life - q->age) * 255 / q->life;
            left          = 255 - (255 - left) * (255 - left) / 255; /* stay bright, then fade */
            if (left < 90 && (bklm_rand() & 3) == 0) left = 0;        /* twinkle out */
            bklm_splat(pixels, q->x, q->y, hsv_to_rgb((HSV){.h = q->hue, .s = 230, .v = 255}), (uint8_t)left, false);
        }
    }
    if (live) {
        bklm_motion_linger(200); /* keep drawing until the last ember is gone */
    }
    return live || moving || m->idle;
}
