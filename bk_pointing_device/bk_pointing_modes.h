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

 #include QMK_KEYBOARD_H
 
typedef struct {
    uint16_t keycode;
    uint8_t mode;
} mode_map_t;

#define BKPD_AMOUNT_CUSTOM_MODES 4

 enum {
    MODE_NORMAL = 0,
    MODE_SNIPING = 1,
    MODE_DRAGSCROLL = 2,
    MODE_CURSOR = 3,
    MODE_BRIGHTNESS = 4,
    MODE_ZOOM = 5,
    MODE_VOLUME = 6,
    MODE_TAB_SWITCH = 7,
    MODE_HISTORY = 8,
    MODE_CUSTOM1 = 9,
    MODE_CUSTOM2 = 10,
    MODE_CUSTOM3 = 11,
    MODE_CUSTOM4 = 12,
    MODE_CUSTOM5 = 13,
    MODE_LAST = 14
};

void bkpd_mode_set_active(uint8_t id);
report_mouse_t bkpd_process_active_mode(report_mouse_t mouse_report);
void bkpd_mode_current_cycle_dpi(bool forward);
void bkpd_mode_current_invert_axis(bool axis);
void bkpd_mode_legacy_sniping_cycle_dpi(bool forward);
report_mouse_t bkpd_mode_dragscroll_process(report_mouse_t mouse_report);
report_mouse_t bkpd_mode_cursor_process(report_mouse_t mouse_report);
void bkpd_modes_init(void);
void bkpd_mode_cycle_dpi(uint8_t mode_id, bool forward);
void bkpd_mode_apply_dpi(uint8_t mode_id);
uint8_t bkpd_mode_get_active_id(void);
void bkpd_mode_change_dpi(uint8_t mode_id, uint16_t new_dpi);
uint16_t bkpd_mode_get_dpi(uint8_t mode_id);
void bkpd_mode_set_invert(uint8_t mode_id, uint8_t axis_index, bool invert);
uint16_t bkpd_mode_get_minimum_dpi(uint8_t mode_id);
uint16_t bkpd_mode_get_max_dpi(uint8_t mode_id);
bool bkpd_mode_get_invert(uint8_t mode_id, uint8_t axis_index);
uint16_t bkpd_mode_get_dpi_per_step(uint8_t mode_id);
report_mouse_t bkpd_mode_brightness_process(report_mouse_t mouse_report);
report_mouse_t bkpd_mode_zoom_process(report_mouse_t mouse_report);
uint16_t bkpd_get_highest_mode_keycode(void);
uint16_t bkpd_get_lowest_mode_keycode(void);
uint8_t bkpd_mode_from_keycode(uint16_t keycode);
void bkpd_mode_toggle_active(uint8_t mode_id);
bool bkpd_mode_is_valid(uint8_t mode_id);
report_mouse_t bkpd_mode_volume_process(report_mouse_t mouse_report);  
report_mouse_t bkpd_mode_tab_switch_process(report_mouse_t mouse_report);
report_mouse_t bkpd_mode_history_process(report_mouse_t mouse_report);
void bkpd_modes_init_default_config(void);
bool bkpd_mode_is_sniping(void);
bool bkpd_mode_should_handle_rgb(void);
report_mouse_t bkpd_mode_custom_process(report_mouse_t mouse_report);
void bkpd_custom_mode_set_keys(uint8_t mode_id, uint8_t *mode_config);
void bkpd_custom_mode_set_keys(uint8_t mode_id, uint8_t *mode_config);
void bkpd_mode_set_activate_on_layer(uint8_t mode_id, uint8_t activate_on_layer);