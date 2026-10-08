// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_display.h"
#include "led_matrix_layers.h"
#include "led_matrix_mods.h"

#define BKLM_MOD_NAME_W 11
#define BKLM_MOD_NAME_H 4

/* A blank row between stacked names, none after the last one. */
#define BKLM_MOD_NAME_ADVANCE (BKLM_MOD_NAME_H + 1)

/* A fourth held modifier is not drawn: four names need 4 * 5 - 1 = 19 rows.
 * Letters one row shorter than the layer font are what buy the third name - at
 * five rows tall, three names and their gaps already overflow the panel. */
#define BKLM_MOD_NAME_SLOTS 3

_Static_assert(BKLM_MOD_NAME_W <= BKLM_COLS, "modifier name runs off the right of the panel");
_Static_assert(BKLM_MOD_NAME_SLOTS * BKLM_MOD_NAME_ADVANCE - 1 <= BKLM_ROWS, "stacked modifier names run off the bottom of the panel");

enum {
    BKLM_MOD_CTRL = 0,
    BKLM_MOD_SHIFT,
    BKLM_MOD_ALT,
    BKLM_MOD_GUI,
    BKLM_MOD_COUNT
};

/*
 * Char art of the whole word rather than a 3x4 alphabet and a lookup: there are
 * only four labels, so a font would carry glyphs nothing draws and one more
 * table to keep in sync. Same trade the duck sprite makes - the bitmap is its
 * own comment.
 *
 * Three 3-wide letters with a blank column between them. '#' is ink, anything
 * else stays dark. Row 0 is the top of the word.
 */
static const struct {
    uint8_t mask;
    char    name[BKLM_MOD_NAME_H][BKLM_MOD_NAME_W + 1];
} bklm_mod[BKLM_MOD_COUNT] = {
    [BKLM_MOD_CTRL] = {
        .mask = MOD_MASK_CTRL, /* KC_LCTL | KC_RCTL */
        .name = {
            "###.###.#..",
            "#....#..#..",
            "#....#..#..",
            "###..#..###",
        },
    },
    [BKLM_MOD_SHIFT] = {
        .mask = MOD_MASK_SHIFT, /* KC_LSFT | KC_RSFT */
        .name = {
            "###.###.###",
            "#...#....#.",
            "..#.###..#.",
            "###.#....#.",
        },
    },
    [BKLM_MOD_ALT] = {
        .mask = MOD_MASK_ALT, /* KC_LALT | KC_RALT */
        .name = {
            "###.#...###",
            "#.#.#....#.",
            "###.#....#.",
            "#.#.###..#.",
        },
    },
    [BKLM_MOD_GUI] = {
        .mask = MOD_MASK_GUI, /* KC_LGUI | KC_RGUI */
        .name = {
            "###.#.#.###",
            "#...#.#..#.",
            "#.#.#.#..#.",
            "###.###.###",
        },
    },
};

/* Indices into bklm_mod, oldest press first. */
static uint8_t bklm_mod_held[BKLM_MOD_COUNT];
static uint8_t bklm_mod_held_count;

/* Held modifiers in the order their keys went down: released ones drop out of
 * the list, newly held ones append to it.
 *
 * Rebuilt from get_mods() once per frame rather than recorded in
 * process_record, because the composer skips this indicator entirely while a
 * pointer mode owns the panel, and an event-driven list would go stale there.
 * Sampling is self-healing, at the cost of two modifiers pressed inside one
 * refresh period falling back to the order of bklm_mod. */
static void bklm_mods_track_press_order(uint8_t mods) {
    uint8_t kept = 0;
    for (uint8_t i = 0; i < bklm_mod_held_count; i++) {
        if (mods & bklm_mod[bklm_mod_held[i]].mask) {
            bklm_mod_held[kept++] = bklm_mod_held[i];
        }
    }
    bklm_mod_held_count = kept;

    for (uint8_t i = 0; i < BKLM_MOD_COUNT; i++) {
        if ((mods & bklm_mod[i].mask) == 0) {
            continue;
        }

        bool held = false;
        for (uint8_t j = 0; j < bklm_mod_held_count; j++) {
            held = held || bklm_mod_held[j] == i;
        }
        if (!held) {
            bklm_mod_held[bklm_mod_held_count++] = i;
        }
    }
}

/* Left-aligned like the layer name, so the spare column stays dark on the
 * right. top_row is the visual top of the word; framebuffer y = 0 is the
 * bottom LED. */
static void bklm_mods_draw_name(RGB *pixels, uint8_t top_row, const char name[BKLM_MOD_NAME_H][BKLM_MOD_NAME_W + 1], RGB color) {
    for (uint8_t row = 0; row < BKLM_MOD_NAME_H; row++) {
        for (uint8_t col = 0; col < BKLM_MOD_NAME_W; col++) {
            if (name[row][col] != '#') {
                continue;
            }
            const uint8_t y                       = (BKLM_ROWS - 1) - (top_row + row);
            pixels[(uint16_t)y * BKLM_COLS + col] = color;
        }
    }
}

/* CTL / SFT / ALT / GUI for the held modifiers, stacked in the order they were
 * pressed and painted in the active layer's color, so the panel still says
 * which layer you are on while a modifier hides the layer stack. Left and right
 * keys of the same modifier share one name.
 *
 * Returns false without painting when pixels is NULL or nothing is held, which
 * is how the composer knows to fall through to the layer stack. */
bool bklm_draw_active_modifier_names(RGB *pixels) {
    if (pixels == NULL) {
        return false;
    }

    bklm_mods_track_press_order(get_mods());
    if (bklm_mod_held_count == 0) {
        return false;
    }

    const uint8_t shown = bklm_mod_held_count < BKLM_MOD_NAME_SLOTS ? bklm_mod_held_count : BKLM_MOD_NAME_SLOTS;

    /* Centered as a block, so a single name sits mid-panel and three fill it. */
    const uint8_t top   = (BKLM_ROWS - (shown * BKLM_MOD_NAME_ADVANCE - 1)) / 2;
    const RGB     color = bklm_get_active_layer_color();

    for (uint8_t i = 0; i < shown; i++) {
        bklm_mods_draw_name(pixels, top + i * BKLM_MOD_NAME_ADVANCE, bklm_mod[bklm_mod_held[i]].name, color);
    }

    return true;
}
