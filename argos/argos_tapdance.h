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

typedef enum {
    TD_NONE,
    TD_UNKNOWN,
    TD_SINGLE_TAP,
    TD_SINGLE_HOLD,
    TD_DOUBLE_TAP,
    TD_DOUBLE_HOLD,
    TD_DOUBLE_SINGLE_TAP, // Send two single taps
    TD_MORE_TAPS
} argos_td_state_t;

void argos_reload_tap_dances(void);
bool argos_tap_dance_read_eeprom(uint8_t index, argos_td_entry_t *entry);
bool argos_tap_dance_write_eeprom(uint8_t index, const argos_td_entry_t *entry);
void argos_tap_dance_listen_for_key(uint8_t *data);
void argos_tap_dance_set_keycode(uint8_t tap_dance_index, uint16_t keycode, uint8_t key_index);
void argos_tap_dance_reset_capturing_tap_dance_key_index(uint8_t index);
void argos_tap_dances_load_from_eeprom(void);
void argos_tap_dance_load_from_eeprom(uint8_t index);
void argos_tap_dance_set(uint8_t index, argos_td_entry_t entry);
argos_td_entry_t* argos_tap_dance_get(uint8_t index);
void argos_tap_dance_set_timing(uint16_t timing, uint8_t td_index);