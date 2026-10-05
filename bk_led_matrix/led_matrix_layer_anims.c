// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_layer_anims.h"
#include "led_matrix_layers.h"
#include "led_matrix_motion.h"

/*
 * Drawn in the layer's Argos colour, except the equaliser.
 * Char art below is row 0 = top, '#' = lit; bklm_top() converts to panel rows.
 */

__attribute__((weak)) uint8_t bklm_layer_anim_user(uint8_t layer) {
    return BKLM_LAYER_ANIM_NONE;
}

/* Cell (x, row) counted from the TOP, or NULL off the panel. */
static RGB *bklm_top(RGB *pixels, int16_t x, int16_t row) {
    if (x < 0 || x >= BKLM_COLS || row < 0 || row >= BKLM_ROWS) return NULL;
    return bklm_px(pixels, (uint8_t)x, (uint8_t)(BKLM_ROWS - 1 - row));
}

static void bklm_put(RGB *pixels, int16_t x, int16_t row, RGB c) {
    RGB *p = bklm_top(pixels, x, row);
    if (p) *p = c;
}

static RGB bklm_scale(RGB c, uint32_t level) {
    return (RGB){.r = (uint8_t)(c.r * level / 255), .g = (uint8_t)(c.g * level / 255), .b = (uint8_t)(c.b * level / 255)};
}

/* Whitish, with a hint of the layer colour. */
static RGB bklm_whitish(RGB c) {
    return (RGB){.r = (uint8_t)((255 + c.r) / 2), .g = (uint8_t)((255 + c.g) / 2), .b = (uint8_t)((255 + c.b) / 2)};
}

/* --- Gear (settings) ------------------------------------------------------ */

/* A six-toothed gear with a hub hole, turning a sixth of a turn a second;
 * four samples a cell for smooth teeth. */
static void bklm_anim_gear(RGB *pixels, RGB c, uint32_t t) {
    const uint8_t rot = (uint8_t)(t * 43 / 1000);
    for (uint8_t y = 0; y < BKLM_ROWS; y++) {
        for (uint8_t x = 0; x < BKLM_COLS; x++) {
            uint8_t hits = 0;
            for (uint8_t q = 0; q < 4; q++) {
                const int32_t dx = x * 256 + 64 + (q & 1) * 128 - BKLM_COLS * 128;
                const int32_t dy = y * 256 + 64 + (q >> 1) * 128 - BKLM_ROWS * 128;
                const int32_t r  = (int32_t)bklm_isqrt((uint32_t)(dx * dx + dy * dy)); /* Q8 cells */
                const uint8_t a  = (uint8_t)(bklm_atan2_8(dy, dx) - rot);
                const bool    tooth = ((uint8_t)(a * 6) & 255) < 128;
                const int32_t outer = tooth ? 1450 : 1000; /* 5.7 / 3.9 cells */
                hits += r <= outer && r >= 400;             /* hub hole 1.6 cells */
            }
            if (hits) *bklm_px(pixels, x, y) = bklm_scale(c, 64 * hits - 1);
        }
    }
}

/* --- Navigation arrow (navigation) ---------------------------------------- */

/* A sat-nav arrow that eases to a new heading every second or so, inside a
 * faint pulsing halo. Four samples a cell. */
static void bklm_anim_navarrow(RGB *pixels, RGB c, uint32_t t, uint32_t dt) {
    static int32_t  heading = 64 << 8, target = 64 << 8; /* 1/256 turn, Q8; starts pointing up */
    static uint32_t next_turn;
    if ((int32_t)(t - next_turn) >= 0) {
        static const int8_t turns[6] = {-64, -32, 32, 64, 96, -96};
        target += turns[bklm_rand() % 6] << 8;
        next_turn = t + 900 + bklm_rand() % 700;
    }
    heading += (target - heading) * (int32_t)dt / 220; /* ease in */
    const uint8_t a  = (uint8_t)(heading >> 8);
    const int32_t hx = bklm_cos8(a), hy = bklm_sin8(a);

    /* Halo: a thin ring breathing between 4.9 and 5.7 cells. */
    const int32_t halo = 1350 + (int32_t)(bklm_sin8((uint8_t)(t / 8)) * 110 / 127);
    for (uint8_t y = 0; y < BKLM_ROWS; y++) {
        for (uint8_t x = 0; x < BKLM_COLS; x++) {
            const int32_t dx0 = x * 256 + 128 - BKLM_COLS * 128, dy0 = y * 256 + 128 - BKLM_ROWS * 128;
            const int32_t r   = (int32_t)bklm_isqrt((uint32_t)(dx0 * dx0 + dy0 * dy0)) - halo;
            if (r > -160 && r < 160) *bklm_px(pixels, x, y) = bklm_scale(c, (uint32_t)(70 * (160 - (r < 0 ? -r : r)) / 160));

            uint8_t hits = 0;
            for (uint8_t q = 0; q < 4; q++) {
                const int32_t dx = x * 256 + 64 + (q & 1) * 128 - BKLM_COLS * 128;
                const int32_t dy = y * 256 + 64 + (q >> 1) * 128 - BKLM_ROWS * 128;
                const int32_t f  = (dx * hx + dy * hy) / 127, sd = (-dx * hy + dy * hx) / 127;
                const int32_t as = sd < 0 ? -sd : sd;
                /* Tip 4.5 cells ahead, back corners 3.25 cells behind and out,
                 * and a notch 1.75 cells deep in the back. */
                const bool in = f <= 1152 && as * (1152 + 832) <= (1152 - f) * 832 && f >= -832 + 448 - 448 * as / 832;
                hits += in;
            }
            if (hits) *bklm_px(pixels, x, y) = bklm_scale(c, 64 * hits - 1);
        }
    }
}

/* --- Equaliser (media) ---------------------------------------------------- */

/* Four bars chasing random levels (mostly low, with spikes; bass louder), with
 * falling peak dots. */
static void bklm_anim_equalizer(RGB *pixels, uint32_t dt) {
    static int32_t  level[4], target[4], peak[4]; /* Q8 rows */
    static uint32_t retarget;
    retarget += dt;
    if (retarget >= 110) {
        retarget = 0;
        for (uint8_t b = 0; b < 4; b++) {
            const uint32_t r = bklm_rand() % 256;
            target[b]        = (int32_t)(r * r / 256 * (16 - b) / 256) * 256 + 256 * (2 + (bklm_rand() & 1)); /* r^2: spiky */
        }
    }
    for (uint8_t b = 0; b < 4; b++) {
        level[b] += (target[b] - level[b]) * (int32_t)dt / 120;
        level[b] = CONSTRAIN(level[b], 0, BKLM_ROWS * 256);
        peak[b]  = MAX(peak[b] - (int32_t)dt * 6 * 256 / 1000, level[b]); /* peaks fall 6 rows/s */
        const int16_t h  = (int16_t)(level[b] >> 8);
        const int16_t pk = (int16_t)MIN(peak[b] >> 8, BKLM_ROWS - 1);
        for (int16_t y = 0; y < BKLM_ROWS; y++) {
            RGB c = y < 8 ? (RGB){.r = 20, .g = 220, .b = 40} : (y < 12 ? (RGB){.r = 230, .g = 200, .b = 0} : (RGB){.r = 255, .g = 30, .b = 0});
            if (y < h || y == pk) {
                if (y == pk && y >= h) c = bklm_scale(c, 170);
                *bklm_px(pixels, (uint8_t)(b * 3), (uint8_t)y)     = c;
                *bklm_px(pixels, (uint8_t)(b * 3 + 1), (uint8_t)y) = c;
            }
        }
    }
}

/* --- Pointing hand (pointer) --------------------------------------------- */

/* The "pointer" hand cursor: index finger up, three knuckles, thumb out.
 * '#' is the hand; its outline is worked out from the shape. */
static const char bklm_hand_art[10][8] = {
    "..##...", "..##...", "..##...", "..#####", "..#####", "#.#####", "#######", ".######", "..#####", "..####.",
};

static bool bklm_hand_at(int16_t c, int16_t r) {
    return r >= 0 && r < 10 && c >= 0 && c < 7 && bklm_hand_art[r][c] == '#';
}

/* White hand outlined in the layer colour, wandering; clicks every 1.3 s. */
static void bklm_anim_hand(RGB *pixels, RGB c, uint32_t t) {
    const uint32_t since = t % 1300;
    const int16_t  ox    = (int16_t)(2 + (bklm_sin8((uint8_t)(t / 11)) * 2 + 127) / 127 - 1);      /* 1..3 */
    const int16_t  oy    = (int16_t)(3 + (bklm_sin8((uint8_t)(t / 7 + 40)) * 2 + 127) / 127 - 1 + (since < 140)); /* 2..4, +1 pressed */

    /* The hand: outline first (cells next to the shape), then the fill. */
    for (int16_t r = -1; r <= 10; r++) {
        for (int16_t k = -1; k <= 7; k++) {
            if (bklm_hand_at(k, r)) {
                bklm_put(pixels, ox + k, oy + r, (RGB){.r = 255, .g = 255, .b = 255});
            } else if (bklm_hand_at(k - 1, r) || bklm_hand_at(k + 1, r) || bklm_hand_at(k, r - 1) || bklm_hand_at(k, r + 1)) {
                bklm_put(pixels, ox + k, oy + r, c);
            }
        }
    }
    /* Click burst: four sparks flying out from just above the fingertip. */
    if (since < 300) {
        const int16_t d = (int16_t)(1 + since / 100), tx = ox + 2, ty = oy - 2;
        const RGB     b = bklm_scale(c, 255 - since * 200 / 300);
        bklm_put(pixels, tx - d, ty, b), bklm_put(pixels, tx + 1 + d, ty, b);
        bklm_put(pixels, tx, ty - d, b), bklm_put(pixels, tx + 1, ty - d, b);
    }
}

/* --- Floating digits (numbers) ------------------------------------------- */

#define BKLM_FLOAT_N 6

typedef struct {
    int32_t x, y;   /* Q8, top-left of the digit, rows from the top */
    int16_t vx, vy; /* Q8 cells per second */
    uint8_t digit;
    bool    near, entered, alive;
} bklm_float_t;

static bool bklm_float_off(const bklm_float_t *f) {
    return f->x <= -BKLM_GLYPH_W * 256 || f->x >= BKLM_COLS * 256 || f->y <= -BKLM_GLYPH_H * 256 || f->y >= BKLM_ROWS * 256;
}

/* A digit heading in a random direction through a random spot on the panel;
 * unless on_panel, moved back along its path to just outside the edge. */
static void bklm_float_spawn(bklm_float_t *f, bool on_panel) {
    const uint8_t a = (uint8_t)bklm_rand();
    f->near         = bklm_rand() & 1;
    const int32_t v = (f->near ? 4 * 256 : 2 * 256) + (int32_t)(bklm_rand() % (2 * 256)); /* near 4-6, far 2-4 cells/s */
    f->vx           = (int16_t)(bklm_cos8(a) * v / 127);
    f->vy           = (int16_t)(bklm_sin8(a) * v / 127);
    f->x            = (int32_t)(bklm_rand() % ((BKLM_COLS - BKLM_GLYPH_W) * 256));
    f->y            = (int32_t)(bklm_rand() % ((BKLM_ROWS - BKLM_GLYPH_H) * 256));
    f->digit        = (uint8_t)(bklm_rand() % 10);
    f->entered      = on_panel;
    f->alive        = true;
    while (!on_panel && !bklm_float_off(f)) {
        f->x -= f->vx / 4;
        f->y -= f->vy / 4;
    }
}

/* Digits drift across the panel in every direction at two depths, near ones
 * bright and fast over far ones dim and slow, like the "calculating" meme. */
static void bklm_anim_digits(RGB *pixels, RGB c, uint32_t dt) {
    static bklm_float_t f[BKLM_FLOAT_N];
    static uint32_t     spawn_in, last;

    /* Shown again after a while: start with digits already on the panel. */
    if (timer_elapsed32(last) > 200) {
        for (uint8_t i = 0; i < BKLM_FLOAT_N; i++) {
            if (i < 4) {
                bklm_float_spawn(&f[i], true);
            } else {
                f[i].alive = false;
            }
        }
        spawn_in = 0;
    }
    last = timer_read32();

    for (uint8_t i = 0; i < BKLM_FLOAT_N; i++) {
        if (!f[i].alive) continue;
        f[i].x += f[i].vx * (int32_t)dt / 1000;
        f[i].y += f[i].vy * (int32_t)dt / 1000;
        const bool off = bklm_float_off(&f[i]);
        f[i].entered |= !off;
        f[i].alive = !(off && f[i].entered);
    }

    if (spawn_in > dt) {
        spawn_in -= dt;
    } else {
        spawn_in = 300 + bklm_rand() % 500;
        for (uint8_t i = 0; i < BKLM_FLOAT_N; i++) {
            if (!f[i].alive) {
                bklm_float_spawn(&f[i], false);
                break;
            }
        }
    }

    const RGB w = bklm_whitish(c);
    for (uint8_t near = 0; near < 2; near++) { /* far ones first, so near ones cover them */
        for (uint8_t i = 0; i < BKLM_FLOAT_N; i++) {
            if (!f[i].alive || f[i].near != near) continue;
            const bklm_glyph_t g = bklm_font_glyph((char)('0' + f[i].digit));
            for (int16_t r = 0; r < BKLM_GLYPH_H; r++)
                for (int16_t k = 0; k < BKLM_GLYPH_W; k++)
                    if (g[r][k] == '#') bklm_put(pixels, (int16_t)((f[i].x >> 8) + k), (int16_t)((f[i].y >> 8) + r), near ? w : bklm_scale(w, 70));
        }
    }
}

/* --- Floating maths (symbols) --------------------------------------------- */

/* Little 3x5 maths glyphs: + - x / = pi sqrt % ? ^ ( ) < > +-. No digits. */
static const char bklm_math_art[15][5][4] = {
    {"   ", " # ", "###", " # ", "   "}, /* + */
    {"   ", "   ", "###", "   ", "   "}, /* - */
    {"   ", "# #", " # ", "# #", "   "}, /* x */
    {"  #", "  #", " # ", "#  ", "#  "}, /* / */
    {"   ", "###", "   ", "###", "   "}, /* = */
    {"   ", "###", "# #", "# #", "# #"}, /* pi */
    {"  #", "  #", "  #", "# #", " # "}, /* sqrt */
    {"# #", "  #", " # ", "#  ", "# #"}, /* % */
    {"## ", "  #", " # ", "   ", " # "}, /* ? */
    {" # ", "# #", "   ", "   ", "   "}, /* ^ */
    {" # ", "#  ", "#  ", "#  ", " # "}, /* ( */
    {" # ", "  #", "  #", "  #", " # "}, /* ) */
    {"  #", " # ", "#  ", " # ", "  #"}, /* < */
    {"#  ", " # ", "  #", " # ", "#  "}, /* > */
    {" # ", "###", " # ", "   ", "###"}, /* +- */
};

#define BKLM_MATH_N 4

typedef struct {
    int32_t  x, y;   /* Q8, top-left of the glyph, rows from the top */
    int16_t  vx, vy; /* Q8 cells per second */
    uint16_t age, life;
    uint8_t  glyph;
} bklm_math_t;

/* Glyphs drift and fade in and out, one per quarter of the panel and bouncing
 * within it, so they never overlap and stay readable. */
static void bklm_anim_math(RGB *pixels, RGB c, uint32_t dt) {
    static bklm_math_t g[BKLM_MATH_N];
    for (uint8_t i = 0; i < BKLM_MATH_N; i++) {
        bklm_math_t  *m  = &g[i];
        const int32_t x0 = (i & 1) * 6 * 256, y0 = (i >> 1) * 8 * 256; /* this glyph's quarter */
        const int32_t x1 = x0 + 3 * 256, y1 = y0 + 3 * 256;           /* furthest top-left in it */
        if (m->age >= m->life) { /* come back elsewhere in the quarter, as something else */
            const uint8_t a = (uint8_t)bklm_rand();
            const int32_t v = 90 + (int32_t)(bklm_rand() % 150); /* 0.35-0.9 cells/s */
            *m = (bklm_math_t){
                .x = x0 + (int32_t)(bklm_rand() % (3 * 256)), .y = y0 + (int32_t)(bklm_rand() % (3 * 256)),
                .vx = (int16_t)(bklm_cos8(a) * v / 127), .vy = (int16_t)(bklm_sin8(a) * v / 127),
                .life = (uint16_t)(1600 + bklm_rand() % 1200), .age = (uint16_t)(bklm_rand() % 500), /* staggered */
                .glyph = (uint8_t)(bklm_rand() % ARRAY_SIZE(bklm_math_art)),
            };
        }
        m->age += (uint16_t)dt;
        m->x += m->vx * (int32_t)dt / 1000;
        m->y += m->vy * (int32_t)dt / 1000;
        if (m->x < x0 || m->x > x1) m->vx = (int16_t)-m->vx; /* bounce inside the quarter */
        if (m->y < y0 || m->y > y1) m->vy = (int16_t)-m->vy;
        m->x = CONSTRAIN(m->x, x0, x1);
        m->y = CONSTRAIN(m->y, y0, y1);

        /* Fade in over the first quarter of its life, out over the last. */
        const uint32_t q   = m->life / 4;
        const uint32_t age = MIN(m->age, m->life);
        const uint32_t lv  = age < q ? age * 255 / q : (age > m->life - q ? (m->life - age) * 255 / q : 255);
        const RGB      w  = bklm_whitish(c);
        for (int16_t r = 0; r < 5; r++)
            for (int16_t k = 0; k < 3; k++)
                if (bklm_math_art[m->glyph][r][k] == '#') {
                    RGB *p = bklm_top(pixels, (int16_t)((m->x >> 8) + k), (int16_t)((m->y >> 8) + r));
                    if (p) bklm_add(p, w, (uint8_t)lv);
                }
    }
}

bool bklm_draw_layer_anim(RGB *pixels) {
    static uint32_t last;
    const uint32_t  now = timer_read32();
    const uint32_t  dt  = MIN(now - last, 200);
    last                = now;

    const uint8_t layer = get_highest_layer(layer_state);
    if (layer == 0) return false;
    const RGB c = bklm_get_active_layer_color();

    switch (bklm_layer_anim_user(layer)) {
        case BKLM_LAYER_ANIM_GEAR:
            bklm_anim_gear(pixels, c, now);
            return true;
        case BKLM_LAYER_ANIM_NAVARROW:
            bklm_anim_navarrow(pixels, c, now, dt);
            return true;
        case BKLM_LAYER_ANIM_EQUALIZER:
            bklm_anim_equalizer(pixels, dt);
            return true;
        case BKLM_LAYER_ANIM_HAND:
            bklm_anim_hand(pixels, c, now);
            return true;
        case BKLM_LAYER_ANIM_DIGIT:
            bklm_anim_digits(pixels, c, dt);
            return true;
        case BKLM_LAYER_ANIM_MATH:
            bklm_anim_math(pixels, c, dt);
            return true;
        default:
            return false;
    }
}
