// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_pointer.h"
#include "led_matrix.h"
#include "led_matrix_pointer_anims.h"

#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
#    include "bk_pointing_device.h"
#    include "led_matrix_display.h"

#    define LED_MATRIX_ICONS_DEFINE
#    include "led_matrix_data.h"

/*
 * Named modes are a table so a new picture is one line, not a new branch.
 * Unspecified slots stay NULL: Normal has no pictogram and falls through.
 */
static const led_matrix_icon_t *const bklm_icon_by_mode[] = {
    [MODE_CURSOR]     = &led_matrix_icon_cursor,
    [MODE_BRIGHTNESS] = &led_matrix_icon_brightness,
    [MODE_ZOOM]       = &led_matrix_icon_zoom,
    [MODE_VOLUME]     = &led_matrix_icon_volume,
    [MODE_TAB_SWITCH] = &led_matrix_icon_tab_switch,
    [MODE_HISTORY]    = &led_matrix_icon_history,
};

/* One well for Custom 1–5. Rebuilt each time: the digit is the mode. */
static led_matrix_icon_t bklm_icon_custom;


/* Stacks the shared C on a 0-based digit (slot 0 → "1"). Never returns NULL. */
static const led_matrix_icon_t *bklm_icon_compose_custom(uint8_t slot) {
    memset(&bklm_icon_custom, 0, sizeof(bklm_icon_custom));
    memcpy(bklm_icon_custom.pixel, led_matrix_icon_custom_letter, sizeof(led_matrix_icon_custom_letter));
    /* Blank row between C and digit so they read as two glyphs. */
    memcpy(&bklm_icon_custom.pixel[LED_MATRIX_ICON_CUSTOM_LETTER_H + 1], led_matrix_icon_custom_digit[slot], sizeof(led_matrix_icon_custom_digit[slot]));
    return &bklm_icon_custom;
}

/* Table for named modes; custom is letter+digit, not five full wells.
 * NULL means "no picture" — Normal, or a mode this file does not know. */
static const led_matrix_icon_t *bklm_pointer_resolve_icon(uint8_t mode) {
    if (mode >= MODE_CUSTOM1 && mode <= MODE_CUSTOM5) {
        return bklm_icon_compose_custom((uint8_t)(mode - MODE_CUSTOM1));
    }
    /* mode comes from the pointing module, not from this table. */
    if (mode < ARRAY_SIZE(bklm_icon_by_mode)) {
        return bklm_icon_by_mode[mode];
    }
    return NULL;
}

/* Places the well in the centered gutter. Caller passes a real icon.
 * Lives here so the palette is not copied into a second .c. */
static void bklm_draw_icon(RGB *pixels, const led_matrix_icon_t *icon) {
    const uint8_t origin_x = (BKLM_COLS - LED_MATRIX_ICON_W) / 2;
    const uint8_t origin_y = (BKLM_ROWS - LED_MATRIX_ICON_H) / 2;

    for (uint8_t row = 0; row < LED_MATRIX_ICON_H; row++) {
        for (uint8_t col = 0; col < LED_MATRIX_ICON_W; col++) {
            led_matrix_icon_rgb_t rgb = led_matrix_icon_palette[icon->pixel[row][col]];
            /* Icon row 0 is visual top; the framebuffer has y = 0 at the bottom LED. */
            uint8_t y   = (BKLM_ROWS - 1) - (origin_y + row);
            RGB    *dst = &pixels[(uint16_t)y * BKLM_COLS + (origin_x + col)];
            dst->r      = rgb.r;
            dst->g      = rgb.g;
            dst->b      = rgb.b;
        }
    }
}
#endif

/* The active pointing mode: an animation for drag-scroll and sniping, otherwise
 * its pictogram centered in the well.
 * Returns false when pixels is NULL, pointing is not built in, or the mode has no picture. */
bool bklm_pointer_paint(RGB *pixels) {
    if (pixels == NULL) {
        return false;
    }
#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
    if (bklm_pointer_anim_paint(pixels, bklm_pointer_mode())) {
        return true;
    }
    const led_matrix_icon_t *icon = bklm_pointer_resolve_icon(bklm_pointer_mode());
    if (icon == NULL) {
        return false;
    }
    bklm_draw_icon(pixels, icon);
    return true;
#else
    return false;
#endif
}