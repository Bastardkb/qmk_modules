/*
 * Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Publicw License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"
#include "process_combo.h"
#include "argos.h"

// TODO move non-public functions into the c file.

void argos_combos_load_from_eeprom(void);
void argos_combos_copy_from_QMK(void);
bool argos_combo_read_eeprom(uint8_t index, argos_combo_t* combo);
void argos_combo_write_eeprom(uint8_t index, argos_combo_t* combo);
combo_t argos_combo_get(uint8_t index);
void argos_combo_load_from_eeprom(uint8_t index);
void argos_combo_set_keycode(uint8_t combo_index, uint16_t keycode, uint8_t key_index);