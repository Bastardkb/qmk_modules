// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* Fixed keycodes in the keyboard range, after bk_pointing_device's
 * (0x7E00-0x7E20), so Argos can name them. 0x7E30-0x7E3F are this module's. */
#define LED_MATRIX_ANIMATION_NEXT 0x7E30 // next trackball animation; with Shift, the previous one
#define LM_ANIM LED_MATRIX_ANIMATION_NEXT
