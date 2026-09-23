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

#ifdef POINTING_DEVICE_DRIVER_digitizer
#include "digitizer.h"
#endif

#include "bk_pointing_modes.h"

#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
enum argos_pointer_command_id {
    argos_id_pointer_command_id_get_mode_info = 0x01,
    argos_id_pointer_command_id_set_dpi = 0x02,
    argos_id_pointer_command_id_set_invert = 0x03,
    argos_id_pointer_command_id_get_device_info = 0x04,
    argos_id_pointer_command_id_set_auto_mouse_layer_enabled = 0x05,
    argos_id_pointer_command_id_set_auto_precision_on_mouse_layer_enabled = 0x06,
    argos_id_pointer_command_id_set_custom_mode = 0x07,
    argos_id_pointer_command_id_set_activate_on_layer = 0x08,
};
#endif

typedef union {
    uint8_t raw;
    struct {
        bool invert_x : 1;
        bool invert_y : 1;
        uint8_t current_dpi_step; // up to 2^8 = 256 steps
        uint8_t dpi_per_step; // up to 256 config step at a time. should be more than enough
        uint8_t max_dpi_steps: 8; // we can optionally limit the max DPI to a certain amount of bytes
        uint16_t minimum_dpi;
        uint8_t activate_on_layer; 
    } __attribute__((packed));
} bkpd_mode_t;

typedef struct {
    uint16_t keycode_up;
    uint16_t keycode_down;
    uint16_t keycode_left;
    uint16_t keycode_right;
} bkpd_pointing_mode_custom_t;

typedef union {
    uint8_t raw;
    struct {
        bool    auto_mouse_layer_enabled : 1;
        bool    auto_precision_on_mouse_layer_enabled : 1;
        bool    has_copied_qmk_config : 1;
        bkpd_mode_t modes_config[MODE_LAST];
        bkpd_pointing_mode_custom_t custom_modes_config[BKPD_AMOUNT_CUSTOM_MODES];
        uint8_t active_mode;
    } __attribute__((packed));
} bkpd_config_t;

// TODO get rid of unused functions here
bool bkpd_dispatch_command(uint8_t *command_id, uint8_t *command_data);
void bkpd_build_pointing_device_info_command_data(uint8_t *command_data);
void keyboard_post_init_bk_pointing_device(void);
void bkpd_set_auto_mouse_layer_enabled(bool enabled);
void bkpd_set_auto_precision_on_mouse_layer_enabled(bool enabled);
bool bkpd_get_auto_mouse_layer_enabled(void);
bool bkpd_get_auto_precision_on_mouse_layer_enabled(void);
void write_bkpd_config_to_eeprom(void);
bool bkpd_is_changing_dpi_settings(void);

// TODO gate this behing community_module_argos_enabled
void bkpd_build_mode_config_command_data(uint8_t mode_id, uint8_t *command_data);
void bkpd_init_default_config(void);
void bkpd_deactivate_old_mode_and_activate_new(uint8_t new_mode_id);
void bkpd_deactivate_old_secondary_mode_and_activate_new(uint8_t new_mode_id);
void bkpd_mode_release(uint8_t mode_id);

#ifdef POINTING_DEVICE_DRIVER_digitizer
bool digitizer_task_kb(digitizer_t *const digitizer_state);
void accumulate_delta_x_y(uint8_t contact, digitizer_t *const digitizer_state, digitizer_t *const last_report, uint16_t *delta_x, uint16_t *delta_y);
#endif

// NOTE: made to work on branch bkb-pointing-device
