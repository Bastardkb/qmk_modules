// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

#define BKLM_GLYPH_W 3
#define BKLM_GLYPH_H 5

/* A 3x5 glyph of A-Z or 0-9, row 0 at the top, '#' lit; NULL for others. */
typedef const char (*bklm_glyph_t)[BKLM_GLYPH_W + 1];
bklm_glyph_t bklm_font_glyph(char c);

/* Override in the keymap to name layers (up to three letters); NULL falls back. */
const char *bklm_layer_name_user(uint8_t layer);

bool bklm_draw_layer_stack(RGB *pixels);

RGB bklm_get_active_layer_color(void);
