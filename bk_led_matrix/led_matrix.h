// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "led_matrix_display.h"

bool process_record_bk_led_matrix(uint16_t keycode, keyrecord_t *record);
void process_records_bk_led_matrix_note_last_input(uint16_t keycode, keyrecord_t *record);

/* The bk_pointing_device mode as seen by this half: local on the USB half,
 * synced from it on the other. */
uint8_t bklm_pointer_mode(void);
