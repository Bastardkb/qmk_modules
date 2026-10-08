// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Asteroids, after the 1979 arcade game: a fixed ship that turns towards the
 * motion and fires at random; rocks split large -> medium -> small -> debris.
 * Everything lives on the wrapped panel, in Q8 cells; angles are 0..255,
 * 0 = right, 64 = up.
 */

#define BKLM_AST_ROCKS 10
#define BKLM_AST_SHOTS 6
#define BKLM_AST_DEBRIS 28
#define BKLM_AST_W (BKLM_COLS * 256)
#define BKLM_AST_H (BKLM_ROWS * 256)

/* Rock radius by size: none, small, medium, large (0.7, 1.2, 1.75 cells). */
static const int32_t bklm_ast_radius[4] = {0, 180, 300, 448};

typedef struct {
    int32_t x, y, vx, vy; /* Q8 cells, Q8 cells per second */
    uint8_t size;         /* 0 = slot free */
    uint8_t seed, rot;    /* outline wobble, and its rotation */
    int8_t  spin;         /* rotation per second, in 1/256 turns */
} bklm_rock_t;

typedef struct {
    int32_t  x, y, vx, vy;
    uint16_t age, life; /* life 0 = slot free */
    RGB      color;
} bklm_bit_t; /* a shot or a piece of debris */

static bklm_rock_t bklm_rocks[BKLM_AST_ROCKS];
static bklm_bit_t  bklm_shots[BKLM_AST_SHOTS];
static bklm_bit_t  bklm_debris[BKLM_AST_DEBRIS];

static int32_t bklm_ast_wrap(int32_t v, int32_t size) {
    v %= size;
    return v < 0 ? v + size : v;
}

/* Shortest signed distance on the wrapped panel. */
static int32_t bklm_ast_d(int32_t d, int32_t size) {
    d = bklm_ast_wrap(d, size);
    return d > size / 2 ? d - size : d;
}

static int32_t bklm_ast_rand(int32_t span) {
    return (int32_t)(bklm_rand() % (uint32_t)(2 * span + 1)) - span;
}

static void bklm_ast_rock(int32_t x, int32_t y, int32_t vx, int32_t vy, uint8_t size) {
    for (uint8_t i = 0; i < BKLM_AST_ROCKS; i++) {
        if (bklm_rocks[i].size) continue;
        bklm_rocks[i] = (bklm_rock_t){.x = x, .y = y, .vx = vx, .vy = vy, .size = size, .seed = (uint8_t)bklm_rand(), .spin = (int8_t)bklm_ast_rand(60)};
        return;
    }
}

/* A large rock drifting in from a random edge, roughly across the panel. */
static void bklm_ast_rock_from_edge(void) {
    const bool    side  = bklm_rand() & 1;
    const int32_t x     = side ? 0 : (int32_t)(bklm_rand() % BKLM_AST_W);
    const int32_t y     = side ? (int32_t)(bklm_rand() % BKLM_AST_H) : 0;
    const uint8_t a     = (uint8_t)bklm_rand();
    const int32_t speed = 128 + (int32_t)(bklm_rand() % 230); /* 0.5-1.4 cells/s */
    bklm_ast_rock(x, y, bklm_cos8(a) * speed / 127, bklm_sin8(a) * speed / 127, 3);
}

static void bklm_ast_bits(bklm_bit_t *pool, uint8_t n, int32_t x, int32_t y, uint8_t count, RGB color, int32_t speed) {
    for (uint8_t i = 0; i < n && count; i++) {
        if (pool[i].life) continue;
        const uint8_t a = (uint8_t)bklm_rand();
        const int32_t v = speed / 2 + (int32_t)(bklm_rand() % (uint32_t)speed);
        pool[i]         = (bklm_bit_t){.x = x, .y = y, .vx = bklm_cos8(a) * v / 127, .vy = bklm_sin8(a) * v / 127, .life = (uint16_t)(300 + bklm_rand() % 400), .color = color};
        count--;
    }
}

/* Shot or collision: split the rock in two smaller ones, or burst a small one. */
static void bklm_ast_break(bklm_rock_t *r) {
    bklm_ast_bits(bklm_debris, BKLM_AST_DEBRIS, r->x, r->y, (uint8_t)(2 + r->size * 2), (RGB){.r = 210, .g = 210, .b = 210}, 6 * 256);
    if (r->size > 1) {
        const uint8_t a = (uint8_t)bklm_rand();
        const int32_t kick = 2 * 256 + (int32_t)(bklm_rand() % 512); /* children fly apart at 2-4 cells/s */
        const int32_t kx = bklm_cos8(a) * kick / 127, ky = bklm_sin8(a) * kick / 127;
        const uint8_t s = r->size - 1;
        const int32_t x = r->x, y = r->y, vx = r->vx, vy = r->vy;
        r->size = 0;
        bklm_ast_rock(x, y, vx + kx, vy + ky, s);
        bklm_ast_rock(x, y, vx - kx, vy - ky, s);
    } else {
        r->size = 0;
    }
}

/* Is (f, s), in the ship's own coordinates, on the ship? An arrowhead 3.25
 * cells long and 2.5 wide, with the classic notch in its back. */
static bool bklm_ast_ship(int32_t f, int32_t s) {
    const int32_t as = s < 0 ? -s : s;
    if (f > 512 || f < -320) return false;
    if (as * 832 > (512 - f) * 320) return false;
    return !(f < -128 && as < 96);
}

static void bklm_ast_move(bklm_bit_t *pool, uint8_t n, uint32_t dt) {
    for (uint8_t i = 0; i < n; i++) {
        bklm_bit_t *b = &pool[i];
        if (!b->life) continue;
        b->age += (uint16_t)dt;
        if (b->age >= b->life) {
            b->life = 0;
            continue;
        }
        b->x = bklm_ast_wrap(b->x + b->vx * (int32_t)dt / 1000, BKLM_AST_W);
        b->y = bklm_ast_wrap(b->y + b->vy * (int32_t)dt / 1000, BKLM_AST_H);
    }
}

bool bklm_motion_asteroids(RGB *pixels, const bklm_motion_t *m) {
    /* The ship never moves. It pivots on a cell centre, half a cell off the
     * panel's true middle (a corner where four cells meet), so it draws sharp. */
    const int32_t   sx = BKLM_AST_W / 2 - 128, sy = BKLM_AST_H / 2 - 128;
    static uint8_t  ang;         /* ship heading */
    static bool     alive;
    static int32_t  respawn_ms;  /* until the ship comes back */
    static int32_t  shield_ms;   /* blinking and untouchable after a respawn */
    static int32_t  fire_ms;     /* until the next shot */
    const int32_t   dt = (int32_t)m->dt;
    const uint8_t   target = bklm_atan2_8(m->dir_y, m->dir_x);

    if (m->fresh) {
        memset(bklm_rocks, 0, sizeof(bklm_rocks));
        memset(bklm_shots, 0, sizeof(bklm_shots));
        memset(bklm_debris, 0, sizeof(bklm_debris));
        ang   = target;
        alive = true, respawn_ms = 0, shield_ms = 1000, fire_ms = 300;
        for (uint8_t i = 0; i < 2; i++) bklm_ast_rock_from_edge();
    }

    if (alive) {
        const int8_t  diff = (int8_t)(uint8_t)(target - ang);
        const int32_t most = MAX(LED_MATRIX_MODULE_ASTEROIDS_TURN_RATE * dt / 1000, 1);
        ang                = (uint8_t)(ang + CONSTRAIN(diff, -most, most));
        shield_ms          = MAX(shield_ms - dt, 0);
    } else if ((respawn_ms -= dt) <= 0) {
        ang   = target;
        alive = true, shield_ms = 2000;
    }
    const int32_t hx = bklm_cos8(ang), hy = bklm_sin8(ang);

    if (alive && (fire_ms -= dt) <= 0) {
        fire_ms = LED_MATRIX_MODULE_ASTEROIDS_FIRE_MS / 2 + (int32_t)(bklm_rand() % LED_MATRIX_MODULE_ASTEROIDS_FIRE_MS);
        for (uint8_t i = 0; i < BKLM_AST_SHOTS; i++) {
            if (bklm_shots[i].life) continue;
            bklm_shots[i] = (bklm_bit_t){
                .x = bklm_ast_wrap(sx + hx * 512 / 127, BKLM_AST_W), .y = bklm_ast_wrap(sy + hy * 512 / 127, BKLM_AST_H),
                .vx = hx * 22 * 256 / 127, .vy = hy * 22 * 256 / 127, .life = 650, .color = {.r = 255, .g = 255, .b = 255},
            };
            break;
        }
    }

    uint8_t mass = 0; /* large = 4, medium = 2, small = 1 */
    for (uint8_t i = 0; i < BKLM_AST_ROCKS; i++) {
        bklm_rock_t *r = &bklm_rocks[i];
        if (!r->size) continue;
        r->x = bklm_ast_wrap(r->x + r->vx * dt / 1000, BKLM_AST_W);
        r->y = bklm_ast_wrap(r->y + r->vy * dt / 1000, BKLM_AST_H);
        r->rot += (uint8_t)(r->spin * dt / 1000);
        mass += (uint8_t)(1 << (r->size - 1));
    }
    if (mass < 3) bklm_ast_rock_from_edge();

    bklm_ast_move(bklm_shots, BKLM_AST_SHOTS, m->dt);
    bklm_ast_move(bklm_debris, BKLM_AST_DEBRIS, m->dt);

    for (uint8_t i = 0; i < BKLM_AST_ROCKS; i++) {
        bklm_rock_t *r = &bklm_rocks[i];
        if (!r->size) continue;
        const int32_t rr = bklm_ast_radius[r->size];
        for (uint8_t j = 0; j < BKLM_AST_SHOTS && r->size; j++) {
            bklm_bit_t *b = &bklm_shots[j];
            if (!b->life) continue;
            const int32_t dx = bklm_ast_d(b->x - r->x, BKLM_AST_W), dy = bklm_ast_d(b->y - r->y, BKLM_AST_H);
            if (dx * dx + dy * dy < rr * rr) {
                b->life = 0;
                bklm_ast_break(r);
            }
        }
        if (r->size && alive && !shield_ms) {
            const int32_t dx = bklm_ast_d(sx - r->x, BKLM_AST_W), dy = bklm_ast_d(sy - r->y, BKLM_AST_H);
            const int32_t reach = rr - 40; /* the rock must really overlap the ship, not graze it */
            if (dx * dx + dy * dy < reach * reach) {
                bklm_ast_bits(bklm_debris, BKLM_AST_DEBRIS, sx, sy, 12, (RGB){.r = 255, .g = 255, .b = 255}, 7 * 256);
                alive      = false;
                respawn_ms = 1000;
                bklm_ast_break(r);
            }
        }
    }

    for (uint8_t py = 0; py < BKLM_ROWS; py++) {
        for (uint8_t px = 0; px < BKLM_COLS; px++) {
            RGB *dst = bklm_px(pixels, px, py);
            for (uint8_t i = 0; i < BKLM_AST_ROCKS; i++) {
                const bklm_rock_t *r = &bklm_rocks[i];
                if (!r->size) continue;
                const int32_t dx = bklm_ast_d(px * 256 + 128 - r->x, BKLM_AST_W), dy = bklm_ast_d(py * 256 + 128 - r->y, BKLM_AST_H);
                const int32_t rr = bklm_ast_radius[r->size];
                if (dx > rr + 128 || dx < -rr - 128 || dy > rr + 128 || dy < -rr - 128) continue;
                const int32_t dist = (int32_t)bklm_isqrt((uint32_t)(dx * dx + dy * dy));
                /* A lumpy edge: the radius wobbles three times round the rock. */
                const uint8_t a    = (uint8_t)(bklm_atan2_8(dy, dx) + r->rot);
                const int32_t edge = rr + rr * bklm_sin8((uint8_t)(a * 3 + r->seed)) * 18 / (127 * 100);
                if (dist <= edge && dist > edge - 200) {
                    bklm_add(dst, (RGB){.r = 170, .g = 170, .b = 170}, 255); /* outline */
                } else if (dist <= edge - 200) {
                    bklm_add(dst, (RGB){.r = 14, .g = 14, .b = 14}, 255); /* faint fill, so a rock reads as solid */
                }
            }
        }
    }
    for (uint8_t i = 0; i < BKLM_AST_SHOTS; i++) {
        const bklm_bit_t *b = &bklm_shots[i];
        if (!b->life) continue;
        bklm_splat(pixels, b->x, b->y, b->color, 255, true);
        bklm_splat(pixels, b->x - b->vx * 30 / 1000, b->y - b->vy * 30 / 1000, b->color, 110, true); /* short tail */
    }
    for (uint8_t i = 0; i < BKLM_AST_DEBRIS; i++) {
        const bklm_bit_t *d = &bklm_debris[i];
        if (d->life) bklm_splat(pixels, d->x, d->y, d->color, (uint8_t)(255 * (d->life - d->age) / d->life), true);
    }

    if (alive && (shield_ms == 0 || (m->now / 120) & 1)) {
        for (uint8_t py = 0; py < BKLM_ROWS; py++) {
            for (uint8_t px = 0; px < BKLM_COLS; px++) {
                uint8_t hits = 0;
                for (uint8_t q = 0; q < 4; q++) {
                    const int32_t dx = bklm_ast_d(px * 256 + 64 + (q & 1) * 128 - sx, BKLM_AST_W);
                    const int32_t dy = bklm_ast_d(py * 256 + 64 + (q >> 1) * 128 - sy, BKLM_AST_H);
                    hits += bklm_ast_ship((dx * hx + dy * hy) / 127, (-dx * hy + dy * hx) / 127);
                }
                /* Solid where the ship covers most of the cell, a faint glow
                 * where it only clips it, so it stays crisp. */
                if (hits >= 2) {
                    *bklm_px(pixels, px, py) = (RGB){.r = 255, .g = 255, .b = 255};
                } else if (hits == 1) {
                    bklm_add(bklm_px(pixels, px, py), (RGB){.r = 255, .g = 255, .b = 255}, 70);
                }
            }
        }
    }

    for (uint16_t i = 0; i < BKLM_COLS * BKLM_ROWS; i++) {
        pixels[i].r = (uint8_t)(pixels[i].r * m->fade / 255);
        pixels[i].g = (uint8_t)(pixels[i].g * m->fade / 255);
        pixels[i].b = (uint8_t)(pixels[i].b * m->fade / 255);
    }
    return true;
}
