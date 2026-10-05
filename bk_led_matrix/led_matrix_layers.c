// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_display.h"
#include "led_matrix_layers.h"

#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
#    include "argos_rgb.h"
#endif

#define BKLM_LAYER_NAME_LEN 3

/* One blank column beyond the glyph, so the three letters stay separate. */
#define BKLM_GLYPH_ADVANCE (BKLM_GLYPH_W + 1)

/* The last glyph has no blank column after it, hence the -1. */
_Static_assert(BKLM_LAYER_NAME_LEN * BKLM_GLYPH_ADVANCE - 1 <= BKLM_COLS, "layer name runs off the right of the panel");

/* One slot per layer, layer BKLM_LAYER_SLOTS-1 at the top down to layer 0 at
 * the bottom. An inactive slot is a single bar; the active one grows into a
 * block of line, blank row, name, blank row, line - so the name sits
 * BKLM_LAYER_BLOCK_NAME_ROW rows into the block, with the same gap below.
 *
 * Seven bars plus one block is exactly BKLM_ROWS, which is what lets the stack
 * fill the panel whichever layer is active with no padding to place. Widen the
 * gaps or the font and the assert below stops the build rather than letting the
 * stack quietly overflow the panel. */
#define BKLM_LAYER_SLOTS          8
#define BKLM_LAYER_BLOCK_NAME_ROW 2
#define BKLM_LAYER_BLOCK_H        (2 * BKLM_LAYER_BLOCK_NAME_ROW + BKLM_GLYPH_H)

_Static_assert((BKLM_LAYER_SLOTS - 1) + BKLM_LAYER_BLOCK_H == BKLM_ROWS, "layer stack does not fill the panel exactly");

/* Inactive bars are scaled down so the active layer's lines and name carry the
 * frame. Same num/den idiom as the brightness trim in led_matrix_display.c. */
#define BKLM_LAYER_DIM_NUM 1
#define BKLM_LAYER_DIM_DEN 2

/* Inactive bars also stop short of the right edge, so the active layer's two
 * full-width lines stand out by shape and not by brightness alone. */
#define BKLM_LAYER_BAR_W 9

_Static_assert(BKLM_LAYER_BAR_W <= BKLM_COLS, "inactive layer bar is wider than the panel");

/*
 * Char art rather than packed bits, the same trade the duck sprite makes: the
 * bitmap is its own comment, so there is nothing to keep in sync. '#' is ink,
 * anything else is transparent. Row 0 is the top of the letter.
 *
 * M, N and W are the honest limit of three columns - they carry a shoulder
 * instead of a diagonal. They still read in context, which is all a
 * three-letter layer name needs.
 */
static const char bklm_font_upper[26][BKLM_GLYPH_H][BKLM_GLYPH_W + 1] = {
    {"###", "#.#", "###", "#.#", "#.#"}, /* A */
    {"##.", "#.#", "##.", "#.#", "##."}, /* B */
    {"###", "#..", "#..", "#..", "###"}, /* C */
    {"##.", "#.#", "#.#", "#.#", "##."}, /* D */
    {"###", "#..", "###", "#..", "###"}, /* E */
    {"###", "#..", "###", "#..", "#.."}, /* F */
    {"###", "#..", "#.#", "#.#", "###"}, /* G */
    {"#.#", "#.#", "###", "#.#", "#.#"}, /* H */
    {"###", ".#.", ".#.", ".#.", "###"}, /* I */
    {"..#", "..#", "..#", "#.#", "###"}, /* J */
    {"#.#", "#.#", "##.", "#.#", "#.#"}, /* K */
    {"#..", "#..", "#..", "#..", "###"}, /* L */
    {"#.#", "###", "###", "#.#", "#.#"}, /* M */
    {"##.", "#.#", "#.#", "#.#", "#.#"}, /* N */
    {"###", "#.#", "#.#", "#.#", "###"}, /* O */
    {"###", "#.#", "###", "#..", "#.."}, /* P */
    {"###", "#.#", "#.#", "###", "..#"}, /* Q */
    {"###", "#.#", "##.", "#.#", "#.#"}, /* R */
    {"###", "#..", "###", "..#", "###"}, /* S */
    {"###", ".#.", ".#.", ".#.", ".#."}, /* T */
    {"#.#", "#.#", "#.#", "#.#", "###"}, /* U */
    {"#.#", "#.#", "#.#", "#.#", ".#."}, /* V */
    {"#.#", "#.#", "###", "###", "#.#"}, /* W */
    {"#.#", "#.#", ".#.", "#.#", "#.#"}, /* X */
    {"#.#", "#.#", ".#.", ".#.", ".#."}, /* Y */
    {"###", "..#", ".#.", "#..", "###"}, /* Z */
};

/* Only ever shown after an "L", so 0 sharing its shape with O and 5 with S
 * costs nothing: the position says which one it is. */
static const char bklm_font_digit[10][BKLM_GLYPH_H][BKLM_GLYPH_W + 1] = {
    {"###", "#.#", "#.#", "#.#", "###"}, /* 0 */
    {".#.", "##.", ".#.", ".#.", "###"}, /* 1 */
    {"###", "..#", "###", "#..", "###"}, /* 2 */
    {"###", "..#", "###", "..#", "###"}, /* 3 */
    {"#.#", "#.#", "###", "..#", "..#"}, /* 4 */
    {"###", "#..", "###", "..#", "###"}, /* 5 */
    {"###", "#..", "###", "#.#", "###"}, /* 6 */
    {"###", "..#", ".#.", ".#.", ".#."}, /* 7 */
    {"###", "#.#", "###", "#.#", "###"}, /* 8 */
    {"###", "#.#", "###", "..#", "..#"}, /* 9 */
};

/* NULL for anything the font does not carry, which is how a blank column in a
 * name draws as nothing rather than as a wrong letter. */
bklm_glyph_t bklm_font_glyph(char c) {
    if (c >= 'A' && c <= 'Z') {
        return bklm_font_upper[c - 'A'];
    }
    if (c >= '0' && c <= '9') {
        return bklm_font_digit[c - '0'];
    }
    return NULL;
}

/*
 * TODO: these are the Dilemma 4x6 layers. The 3x5 keymaps use a different enum
 * (base, function, navigation, media, pointer, numeral, symbols, lcd) and need
 * their own table once the module can tell the two boards apart.
 *
 * Leave a layer this board does not define empty rather than inventing a word
 * for it: bklm_layer_name_of falls back to "L" and the number.
 */
static const char bklm_layer_name[BKLM_LAYER_SLOTS][BKLM_LAYER_NAME_LEN + 1] = {
    "BSE", /* LAYER_BASE */
    "LOW", /* LAYER_LOWER */
    "RSE", /* LAYER_RAISE */
    "PTR", /* LAYER_POINTER */
    "",
    "",
    "",
    "",
};

/* "L" and the layer number for a layer the table does not name, so an
 * unexpected layer still identifies itself. The blank between them is a column
 * the font has no glyph for, which draws as nothing.
 *
 * The buffer is static because the caller only reads it before the next call,
 * and layer is already clamped below BKLM_LAYER_SLOTS, so the digit is one. */
__attribute__((weak)) const char *bklm_layer_name_user(uint8_t layer) {
    return NULL;
}

static const char *bklm_layer_name_of(uint8_t layer) {
    static char numbered[BKLM_LAYER_NAME_LEN + 1] = "L 0";

    const char *user = bklm_layer_name_user(layer);
    if (user != NULL) {
        return user;
    }
    if (bklm_layer_name[layer][0] != '\0') {
        return bklm_layer_name[layer];
    }
    numbered[2] = (char)('0' + layer);
    return numbered;
}

/* Layer 0 is white by definition. The others come from argos, which also
 * falls back to white when it has no color stored for the layer. */
static RGB bklm_layer_color(uint8_t layer) {
    RGB color = {255, 255, 255};
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
    if (layer != 0) {
        argos_rgb_get_layer_color(layer, &color);
        if (color.r == 0 && color.g == 0 && color.b == 0) {
            color.r = 255;
            color.g = 255;
            color.b = 255;
        }
    }
#endif
    return color;
}

/* The color this file paints the active layer with, for indicators that take
 * the panel from the layer stack and still have to say which layer is on. */
RGB bklm_get_active_layer_color(void) {
    return bklm_layer_color(get_highest_layer(layer_state));
}

static RGB bklm_layer_dim(RGB color) {
    color.r = (uint8_t)((color.r * BKLM_LAYER_DIM_NUM) / BKLM_LAYER_DIM_DEN);
    color.g = (uint8_t)((color.g * BKLM_LAYER_DIM_NUM) / BKLM_LAYER_DIM_DEN);
    color.b = (uint8_t)((color.b * BKLM_LAYER_DIM_NUM) / BKLM_LAYER_DIM_DEN);
    return color;
}

/* Runs from the left edge for width columns. row is the visual row, 0 at the
 * top; framebuffer y = 0 is the bottom LED. */
static void bklm_layers_fill_row(RGB *pixels, uint8_t row, uint8_t width, RGB color) {
    RGB *dst = &pixels[(uint16_t)((BKLM_ROWS - 1) - row) * BKLM_COLS];

    for (uint8_t x = 0; x < width; x++) {
        dst[x] = color;
    }
}

/* Three glyphs left-aligned like the bars, so the last ink column is 10 and the
 * spare column stays dark on the right. top_row is the visual top of the text. */
static void bklm_layers_draw_name(RGB *pixels, uint8_t top_row, const char *name, RGB color) {
    for (uint8_t i = 0; i < BKLM_LAYER_NAME_LEN && name[i] != '\0'; i++) {
        const bklm_glyph_t glyph = bklm_font_glyph(name[i]);
        if (glyph == NULL) {
            continue;
        }

        for (uint8_t row = 0; row < BKLM_GLYPH_H; row++) {
            for (uint8_t col = 0; col < BKLM_GLYPH_W; col++) {
                if (glyph[row][col] != '#') {
                    continue;
                }
                const uint8_t x = i * BKLM_GLYPH_ADVANCE + col;
                const uint8_t y = (BKLM_ROWS - 1) - (top_row + row);
                pixels[(uint16_t)y * BKLM_COLS + x] = color;
            }
        }
    }
}

/* One bar per layer, layer 7 at the top down to layer 0 at the bottom, each in
 * its own color. Inactive bars are dimmed and stop short of the right edge; the
 * active layer instead gets a nine-row block at full brightness and full width
 * - line, blank row, its name over five rows, blank row, line - so it reads as
 * the one thing on the panel.
 *
 * Returns false without painting when pixels is NULL or the base layer is
 * active: layer 0 is the resting state, and the composer shows the duck there. */
bool bklm_draw_layer_stack(RGB *pixels) {
    if (pixels == NULL) {
        return false;
    }

    uint8_t active = get_highest_layer(layer_state);
    if (active == 0) {
        return false;
    }
    if (active >= BKLM_LAYER_SLOTS) {
        active = BKLM_LAYER_SLOTS - 1;
    }

    uint8_t row = 0;
    for (int8_t layer = BKLM_LAYER_SLOTS - 1; layer >= 0; layer--) {
        const RGB color = bklm_layer_color((uint8_t)layer);

        if (layer != active) {
            bklm_layers_fill_row(pixels, row, BKLM_LAYER_BAR_W, bklm_layer_dim(color));
            row++;
            continue;
        }

        bklm_layers_fill_row(pixels, row, BKLM_COLS, color);
        bklm_layers_draw_name(pixels, row + BKLM_LAYER_BLOCK_NAME_ROW, bklm_layer_name_of((uint8_t)layer), color);
        bklm_layers_fill_row(pixels, row + BKLM_LAYER_BLOCK_H - 1, BKLM_COLS, color);
        row += BKLM_LAYER_BLOCK_H;
    }

    return true;
}
