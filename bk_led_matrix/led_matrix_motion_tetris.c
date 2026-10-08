// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_motion.h"

/*
 * Tetris doing T-spin doubles, over and over (not reactive), in a 10x16 well.
 * It plays a ten-piece cycle found by a search over this exact rule set
 * (pieces, rotations and kicks below) that ends with the well empty again.
 * Each piece follows a BFS path from the spawn point, so the T is seen
 * dropping in, then twisting under the overhang.
 *
 * Board rows count from the TOP here; drawing flips them.
 */

#define BKLM_TW 10
#define BKLM_TH 16

/* Each piece's four rotations as 4x4 bitmasks, row-major from the top-left
 * (0x8000 is the top-left cell). The T's centre is cell (1, 1) in all four. */
static const uint16_t bklm_tet_shape[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, /* I */
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, /* J */
    {0x4460, 0x0E80, 0xC440, 0x2E00}, /* L */
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, /* O */
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, /* S */
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, /* T */
    {0x0C60, 0x4C80, 0xC600, 0x2640}, /* Z */
};
#define BKLM_TET_T 5
static const RGB bklm_tet_color[7] = {
    {.r = 0, .g = 220, .b = 230},  /* I cyan */
    {.r = 20, .g = 60, .b = 255},  /* J blue */
    {.r = 255, .g = 120, .b = 0},  /* L orange */
    {.r = 240, .g = 220, .b = 0},  /* O yellow */
    {.r = 30, .g = 220, .b = 40},  /* S green */
    {.r = 170, .g = 30, .b = 230}, /* T purple */
    {.r = 240, .g = 20, .b = 30},  /* Z red */
};

static uint8_t bklm_tet_board[BKLM_TH][BKLM_TW]; /* 0 empty, else colour index + 1 */

static bool bklm_tet_cell(uint16_t shape, int8_t r, int8_t c) {
    return (shape >> (15 - (r * 4 + c))) & 1;
}

static bool bklm_tet_solid(int8_t x, int8_t y) { /* walls and floor count as solid */
    return x < 0 || x >= BKLM_TW || y >= BKLM_TH || (y >= 0 && bklm_tet_board[y][x]);
}

static bool bklm_tet_fits(uint8_t piece, uint8_t rot, int8_t x, int8_t y) {
    const uint16_t s = bklm_tet_shape[piece][rot & 3];
    for (int8_t r = 0; r < 4; r++)
        for (int8_t c = 0; c < 4; c++)
            if (bklm_tet_cell(s, r, c) && bklm_tet_solid(x + c, y + r)) return false;
    return true;
}

static void bklm_tet_stamp(uint8_t piece, uint8_t rot, int8_t x, int8_t y, uint8_t v) {
    const uint16_t s = bklm_tet_shape[piece][rot & 3];
    for (int8_t r = 0; r < 4; r++)
        for (int8_t c = 0; c < 4; c++)
            if (bklm_tet_cell(s, r, c) && y + r >= 0) bklm_tet_board[y + r][x + c] = v;
}

static bool bklm_tet_full(int8_t row) {
    for (int8_t c = 0; c < BKLM_TW; c++)
        if (!bklm_tet_board[row][c]) return false;
    return true;
}

static uint8_t bklm_tet_lines(void) {
    uint8_t n = 0;
    for (int8_t r = 0; r < BKLM_TH; r++) n += bklm_tet_full(r);
    return n;
}

/* Three-corner rule, for a T at (x, y) whose last move was a rotation. */
static bool bklm_tet_tspin_corners(int8_t x, int8_t y) {
    return bklm_tet_solid(x, y) + bklm_tet_solid(x + 2, y) + bklm_tet_solid(x, y + 2) + bklm_tet_solid(x + 2, y + 2) >= 3;
}

/* --- Search: every state reachable from the spawn point ------------------- */

enum { BKLM_MV_LEFT, BKLM_MV_RIGHT, BKLM_MV_DOWN, BKLM_MV_CW, BKLM_MV_CCW, BKLM_MV_NONE };

#define BKLM_SX 12 /* x from -2 to 9 */
#define BKLM_SY 18 /* y from -2 to 15 */
#define BKLM_STATES (4 * BKLM_SY * BKLM_SX)
#define BKLM_PATH_MAX 96

static uint16_t bklm_st(int8_t x, int8_t y, uint8_t rot) {
    return (uint16_t)(((rot & 3) * BKLM_SY + (y + 2)) * BKLM_SX + (x + 2));
}

/* Rotation kicks, tried in order: in place, sideways, down, down-sideways,
 * and one row up. */
static const int8_t bklm_kicks[7][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, 1}, {-1, 1}, {1, 1}, {0, -1}};

static uint16_t bklm_parent[BKLM_STATES];
static uint8_t  bklm_move[BKLM_STATES]; /* move that reached the state; BKLM_MV_NONE = unvisited */
static uint16_t bklm_queue[BKLM_STATES];

/* Tries one move from (x, y, rot); writes the result and returns whether it fits. */
static bool bklm_tet_try(uint8_t piece, uint8_t mv, int8_t *x, int8_t *y, uint8_t *rot) {
    if (mv <= BKLM_MV_DOWN) {
        const int8_t nx = *x + (mv == BKLM_MV_LEFT ? -1 : mv == BKLM_MV_RIGHT ? 1 : 0), ny = *y + (mv == BKLM_MV_DOWN);
        if (!bklm_tet_fits(piece, *rot, nx, ny)) return false;
        *x = nx, *y = ny;
        return true;
    }
    const uint8_t nr = (uint8_t)((*rot + (mv == BKLM_MV_CW ? 1 : 3)) & 3);
    for (uint8_t k = 0; k < 7; k++) {
        const int8_t nx = *x + bklm_kicks[k][0], ny = *y + bklm_kicks[k][1];
        if (nx >= -2 && nx < BKLM_TW && ny >= -2 && ny < BKLM_TH && bklm_tet_fits(piece, nr, nx, ny)) {
            *x = nx, *y = ny, *rot = nr;
            return true;
        }
    }
    return false;
}

/* Breadth-first search from the spawn point to the target state. Fills path[]
 * with the moves; returns their count, or -1 if the target can't be reached. */
static int16_t bklm_tet_plan(uint8_t piece, int8_t sx, int8_t sy, int8_t tx, int8_t ty, uint8_t trot, uint8_t *path) {
    memset(bklm_move, BKLM_MV_NONE, sizeof(bklm_move));
    uint16_t       head = 0, tail = 0;
    const uint16_t start = bklm_st(sx, sy, 0), target = bklm_st(tx, ty, trot);
    bklm_move[start]     = BKLM_MV_DOWN; /* any value but NONE: visited */
    bklm_queue[tail++]   = start;
    while (head < tail && bklm_move[target] == BKLM_MV_NONE) {
        const uint16_t s   = bklm_queue[head++];
        const uint8_t  rot = (uint8_t)(s / (BKLM_SY * BKLM_SX));
        const int8_t   y = (int8_t)((s / BKLM_SX) % BKLM_SY) - 2, x = (int8_t)(s % BKLM_SX) - 2;
        for (uint8_t mv = BKLM_MV_LEFT; mv <= BKLM_MV_CCW; mv++) {
            int8_t  nx = x, ny = y;
            uint8_t nr = rot;
            if (!bklm_tet_try(piece, mv, &nx, &ny, &nr)) continue;
            const uint16_t n = bklm_st(nx, ny, nr);
            if (bklm_move[n] != BKLM_MV_NONE) continue;
            bklm_move[n] = mv, bklm_parent[n] = s;
            bklm_queue[tail++] = n;
        }
    }
    if (bklm_move[target] == BKLM_MV_NONE) return -1;

    int16_t n = 0;
    for (uint16_t s = target; s != start && n < BKLM_PATH_MAX; s = bklm_parent[s]) path[n++] = bklm_move[s];
    for (int16_t i = 0; i < n / 2; i++) {
        const uint8_t t = path[i];
        path[i] = path[n - 1 - i], path[n - 1 - i] = t;
    }
    return n;
}

/* --- Game ----------------------------------------------------------------- */

/* The cycle: piece, rotation and where its 4x4 box comes to rest (x, y from
 * the top-left of the well). Found by a search; see the comment at the top. */
static const struct {
    uint8_t piece, rot;
    int8_t  x, y;
} bklm_tet_cycle[10] = {
    {2, 0, -1, 13}, /* L */
    {0, 0, 3, 14},  /* I */
    {1, 1, 7, 14},  /* J */
    {5, 0, 3, 12},  /* T, a builder */
    {4, 0, 5, 12},  /* S */
    {3, 0, 8, 13},  /* O: the slot is ready */
    {5, 0, 1, 13},  /* T: the T-spin double */
    {6, 0, 0, 13},  /* Z */
    {0, 0, 2, 14},  /* I (the Z just cleared a row, so one lower) */
    {0, 0, 6, 14},  /* I: empty well again */
};

enum { BKLM_TET_PLAY, BKLM_TET_CLEAR, BKLM_TET_OVER, BKLM_TET_SPAWN };

/* 3x5 letters for the T-spin banner. */
static const char bklm_tet_font[3][5][4] = {
    {"###", " # ", " # ", " # ", " # "}, /* T */
    {"###", "#  ", "###", "  #", "###"}, /* S */
    {"## ", "# #", "# #", "# #", "## "}, /* D */
};

bool bklm_motion_tetris(RGB *pixels, const bklm_motion_t *m) {
    static uint8_t  state = BKLM_TET_OVER, piece, rot, path[BKLM_PATH_MAX], last_mv, next;
    static int8_t   x, y;
    static int16_t  steps, step;
    static uint32_t move_ms, state_ms = 900, banner_ms;
    static uint8_t  banner_lines; /* 1..3 while the T-spin banner shows */

    switch (state) {
        case BKLM_TET_PLAY:
            /* Sideways moves and turns slower than drops, so they can be seen. */
            move_ms += m->dt;
            while (state == BKLM_TET_PLAY && step < steps && move_ms >= (path[step] == BKLM_MV_DOWN ? 55u : 120u)) {
                move_ms -= path[step] == BKLM_MV_DOWN ? 55u : 120u;
                last_mv = path[step];
                if (!bklm_tet_try(piece, path[step], &x, &y, &rot)) step = steps; /* shouldn't happen */
                step++;
            }
            if (step >= steps && move_ms >= 150) { /* settle for a moment, then lock */
                const bool spun = piece == BKLM_TET_T && last_mv >= BKLM_MV_CW && bklm_tet_tspin_corners(x, y);
                bklm_tet_stamp(piece, rot, x, y, piece + 1);
                const uint8_t lines = bklm_tet_lines();
                if (spun && lines) banner_lines = lines, banner_ms = 0;
                state    = lines ? BKLM_TET_CLEAR : BKLM_TET_SPAWN;
                state_ms = 0;
            }
            break;
        case BKLM_TET_CLEAR:
            if ((state_ms += m->dt) >= 300) {
                for (int8_t r = 0; r < BKLM_TH; r++) {
                    if (!bklm_tet_full(r)) continue;
                    for (int8_t k = r; k > 0; k--) memcpy(bklm_tet_board[k], bklm_tet_board[k - 1], BKLM_TW);
                    memset(bklm_tet_board[0], 0, BKLM_TW);
                }
                state = BKLM_TET_SPAWN;
            }
            break;
        case BKLM_TET_OVER:
            /* Start, or something went wrong: wipe, then restart the cycle. */
            if ((state_ms += m->dt) >= 900) {
                memset(bklm_tet_board, 0, sizeof(bklm_tet_board));
                next  = 0;
                state = BKLM_TET_SPAWN;
            }
            break;
    }
    if (state == BKLM_TET_SPAWN) {
        piece = bklm_tet_cycle[next].piece, rot = 0, x = 3, y = -1, move_ms = 0, step = 0, last_mv = BKLM_MV_NONE;
        steps = bklm_tet_fits(piece, rot, x, y) ? bklm_tet_plan(piece, x, y, bklm_tet_cycle[next].x, bklm_tet_cycle[next].y, bklm_tet_cycle[next].rot, path) : -1;
        next  = (uint8_t)((next + 1) % ARRAY_SIZE(bklm_tet_cycle));
        if (steps < 0) {
            state = BKLM_TET_OVER, state_ms = 0;
        } else {
            state = BKLM_TET_PLAY;
        }
    }

    for (uint8_t py = 0; py < BKLM_ROWS; py++) {
        *bklm_px(pixels, 0, py)             = (RGB){.r = 35, .g = 35, .b = 45};
        *bklm_px(pixels, BKLM_COLS - 1, py) = (RGB){.r = 35, .g = 35, .b = 45};
    }
    const bool flash = state == BKLM_TET_CLEAR && (state_ms / 75) % 2 == 0;
    for (int8_t r = 0; r < BKLM_TH; r++) {
        const bool lit = flash && bklm_tet_full(r);
        for (int8_t c = 0; c < BKLM_TW; c++) {
            const uint8_t v = bklm_tet_board[r][c];
            if (!v) continue;
            *bklm_px(pixels, (uint8_t)(c + 1), (uint8_t)(BKLM_ROWS - 1 - r)) = lit ? (RGB){.r = 255, .g = 255, .b = 255} : bklm_tet_color[v - 1];
        }
    }
    if (state == BKLM_TET_PLAY) {
        const uint16_t s = bklm_tet_shape[piece][rot];
        for (int8_t r = 0; r < 4; r++)
            for (int8_t c = 0; c < 4; c++)
                if (bklm_tet_cell(s, r, c) && y + r >= 0) *bklm_px(pixels, (uint8_t)(x + c + 1), (uint8_t)(BKLM_ROWS - 1 - (y + r))) = bklm_tet_color[piece];
    }
    if (state == BKLM_TET_OVER) {
        const int8_t up = (int8_t)(state_ms * BKLM_TH / 700); /* rows wiped so far, from the bottom */
        for (int8_t r = BKLM_TH - 1; r >= BKLM_TH - up && r >= 0; r--)
            for (int8_t c = 0; c < BKLM_TW; c++) *bklm_px(pixels, (uint8_t)(c + 1), (uint8_t)(BKLM_ROWS - 1 - r)) = (RGB){.r = 70, .g = 70, .b = 80};
    }
    if (banner_lines && (banner_ms += m->dt) < 1400) {
        /* "TS" + S, D or T (single, double, triple), blinking, across the top. */
        const uint8_t letters[3] = {0, 1, banner_lines == 1 ? 1 : (banner_lines == 2 ? 2 : 0)};
        const RGB     c          = (banner_ms / 140) % 2 ? (RGB){.r = 255, .g = 255, .b = 255} : bklm_tet_color[BKLM_TET_T];
        for (uint8_t l = 0; l < 3; l++)
            for (int8_t r = 0; r < 5; r++)
                for (int8_t k = 0; k < 3; k++)
                    if (bklm_tet_font[letters[l]][r][k] == '#') *bklm_px(pixels, (uint8_t)(l * 4 + k), (uint8_t)(BKLM_ROWS - 2 - r)) = c;
    } else {
        banner_lines = 0;
    }
    bklm_dim(pixels, m->fade);
    return true;
}
