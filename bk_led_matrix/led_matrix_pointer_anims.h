// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

/* Counts one drag-scroll step stands for: bk_pointing_device turns the motion
 * into scroll steps before this module sees it. */
#define BKLM_SCROLL_STEP 32

/* Adds pointer motion (report units: x right, y down) on the panel half. */
void bklm_pointer_anim_feed(int16_t dx, int16_t dy);

/* True in a pointing mode that animates, so frames can be paced faster. */
bool bklm_pointer_anim_active(uint8_t mode);

/* Paints the mode's animation: a scroll wheel turning with the scroll for drag-scroll,
 * a scope tracking a target for sniping. False for other modes. */
bool bklm_pointer_anim_paint(RGB *pixels, uint8_t mode);
