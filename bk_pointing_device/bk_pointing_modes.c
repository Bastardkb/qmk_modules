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

#include QMK_KEYBOARD_H
#include "bk_pointing_modes.h"
#include "bk_pointing_device.h"
#include "math.h"

#define BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE 30
// #ifdef POINTING_DEVICE_DRIVER_digitizer
// #define BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_X 250
// #define BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_Y 400
// #else // trackball
#define BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_X 40
#define BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_Y 80
// #endif
#define BK_POINTING_DEVICE_BRIGHTNESS_BUFFER_SIZE 30
#define BK_POINTING_DEVICE_ZOOM_BUFFER_SIZE 30
#define BK_POINTING_DEVICE_VOLUME_BUFFER_SIZE 30
#define BK_POINTING_DEVICE_TAB_SWITCH_BUFFER_SIZE 30
#define BK_POINTING_DEVICE_HISTORY_BUFFER_SIZE 50

#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
#    include "argos.h"
#endif

#define BK_POINTING_DEVICE_MINIMUM_DEFAULT_DPI 100
#define BK_POINTING_DEVICE_DEFAULT_DPI_CONFIG_STEP 100
#define BK_POINTING_DEVICE_MINIMUM_SNIPING_DPI 100
#define BK_POINTING_DEVICE_SNIPING_DPI_CONFIG_STEP 100
// etc etc

#define BK_POINTING_DEVICE_NORMAL_DEFAULT_STEP 3
// etc etc

#define BK_POINTING_DEVICE_DPI_STEPS 30
#define BK_POINTING_DEVICE_SNIPING_DPI_STEPS 20

extern bkpd_config_t g_bkpd_config;
extern int8_t changing_dpi_settings_for_mode;

// pointers to different functions for each mode
typedef struct {
    uint8_t id;
    report_mouse_t (*process)(report_mouse_t mouse_report);
    void (*set_active)(bool);
    void (*invert_axis)(bool axis);
    uint16_t keycode;
} pointing_mode_t;

static pointing_mode_t modes[] = {
    {.id = MODE_NORMAL, .keycode = 0x0000, .process = NULL, .set_active = NULL, .invert_axis = NULL}, 
    {.id = MODE_SNIPING, .keycode = 0x7E04, .process = NULL, .set_active = NULL, .invert_axis = NULL}, 
    {.id = MODE_DRAGSCROLL, .keycode = 0x7E06, .process = bkpd_mode_dragscroll_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_CURSOR, .keycode = 0x7E08, .process = bkpd_mode_cursor_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_BRIGHTNESS, .keycode = 0x7E0A, .process = bkpd_mode_brightness_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_ZOOM, .keycode = 0x7E0C, .process = bkpd_mode_zoom_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_VOLUME, .keycode = 0x7E0E, .process = bkpd_mode_volume_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_TAB_SWITCH, .keycode = 0x7E10, .process = bkpd_mode_tab_switch_process, .set_active = NULL, .invert_axis = NULL},
     {.id = MODE_HISTORY, .keycode = 0x7E12, .process = bkpd_mode_history_process, .set_active = NULL, .invert_axis = NULL},
    {.id = MODE_CUSTOM1, .keycode = 0x7E14, .process = bkpd_mode_custom_process, .set_active = NULL, .invert_axis = NULL},
    {.id = MODE_CUSTOM2, .keycode = 0x7E16, .process = bkpd_mode_custom_process, .set_active = NULL, .invert_axis = NULL},
    {.id = MODE_CUSTOM3, .keycode = 0x7E18, .process = bkpd_mode_custom_process, .set_active = NULL, .invert_axis = NULL},
    {.id = MODE_CUSTOM4, .keycode = 0x7E1A, .process = bkpd_mode_custom_process, .set_active = NULL, .invert_axis = NULL},
    {.id = MODE_CUSTOM5, .keycode = 0x7E1C, .process = bkpd_mode_custom_process, .set_active = NULL, .invert_axis = NULL},
};

static pointing_mode_t *active_mode = &modes[0]; // reset on keyboard connection
bool sniping_modifier_active = false;

/* -----------------------------------------------------------------------------
            Helper functions for mode management */

void bkpd_modes_init(void) {

    // Init to default values
    for(int i = 0; i < MODE_LAST; i++) {
        g_bkpd_config.modes_config[i].activate_on_layer = 0;
        g_bkpd_config.modes_config[i].max_dpi_steps     = BK_POINTING_DEVICE_SNIPING_DPI_STEPS;
        g_bkpd_config.modes_config[i].dpi_per_step     = BK_POINTING_DEVICE_SNIPING_DPI_CONFIG_STEP;
        g_bkpd_config.modes_config[i].minimum_dpi     = BK_POINTING_DEVICE_MINIMUM_SNIPING_DPI;
        
        // set default step to a big higher than minimum
        g_bkpd_config.modes_config[i].current_dpi_step = 4;
    }

    // edit specific values
    g_bkpd_config.modes_config[MODE_NORMAL].max_dpi_steps     = BK_POINTING_DEVICE_DPI_STEPS;
    g_bkpd_config.modes_config[MODE_NORMAL].dpi_per_step     = BK_POINTING_DEVICE_DEFAULT_DPI_CONFIG_STEP;
    g_bkpd_config.modes_config[MODE_NORMAL].minimum_dpi     = BK_POINTING_DEVICE_MINIMUM_DEFAULT_DPI;
    g_bkpd_config.modes_config[MODE_NORMAL].current_dpi_step = 6;
}

void bkpd_mode_set_activate_on_layer(uint8_t mode_id, uint8_t activate_on_layer) {
    if(!bkpd_mode_is_valid(mode_id)) return;
    printf("set activate on layer, mode_id: %d, activate_on_layer: %d\n", mode_id, activate_on_layer);
    g_bkpd_config.modes_config[mode_id].activate_on_layer = activate_on_layer;
    write_bkpd_config_to_eeprom();
}

uint16_t bkpd_get_highest_mode_keycode(void) {
    uint16_t highest_keycode = 0;
    for(int i = 0; i < sizeof(modes) / sizeof(pointing_mode_t); i++) {
        if(modes[i].keycode > highest_keycode) {
            highest_keycode = modes[i].keycode;
        }
    }
    return highest_keycode;
}

uint16_t bkpd_get_lowest_mode_keycode(void) {
    uint16_t lowest_keycode = bkpd_get_highest_mode_keycode();
    for(int i = 0; i < sizeof(modes) / sizeof(pointing_mode_t); i++) {
        if(modes[i].keycode < lowest_keycode) {
            lowest_keycode = modes[i].keycode;
        }
    }
    return lowest_keycode;
}

uint8_t bkpd_mode_from_keycode(uint16_t keycode) {
    for(int i = 0; i < sizeof(modes) / sizeof(pointing_mode_t); i++) {
        if(modes[i].keycode == keycode || // regular or toggle
            modes[i].keycode + 1 == keycode) {
            return modes[i].id;
        }
    }
    return -1;
}

uint8_t bkpd_mode_get_active_id(void) {
    return active_mode->id;
}

bool bkpd_mode_is_valid(uint8_t mode_id) {
    // manually test for mode 0
    if(mode_id == MODE_NORMAL) {
        return true;
    }
    // otherwise, test in map
    for(int i = 0; i < sizeof(modes) / sizeof(pointing_mode_t); i++) {
        if(modes[i].id == mode_id) {
            return true;
        }
    }
    return false;
}

void bkpd_deactivate_old_mode_and_activate_new(uint8_t new_mode_id) {
    if (active_mode->set_active != NULL) {
        active_mode->set_active(false); // disable current mode nonetheless, only one mode active at a time
    }
    active_mode = &modes[new_mode_id];
    if (active_mode->set_active != NULL) {
        active_mode->set_active(true);
    }
}

void bkpd_mode_release(uint8_t mode_id) {
    // if we are releasing sniping, check if we are in a mode combo
    // eg. pressing snipe+drag, release snipe -> keep drag
    if(mode_id == MODE_SNIPING) {
        if(sniping_modifier_active) {
            // nothing to do with primary mode, we kept it active
            // simply disable secondary sniping mode
            sniping_modifier_active = false;
        }
        // no secondary active, just go back to normal mode
        else{
            bkpd_deactivate_old_mode_and_activate_new(MODE_NORMAL);
        }
    }
    // if we are releasing another mode, we need to check if sniping was enabled.
    // if so, we want to move sniping back into the active mode
    else{
        // are we in a mode combo?
        if(sniping_modifier_active) {
            // activate primary mode as sniping
            bkpd_deactivate_old_mode_and_activate_new(MODE_SNIPING);
            // deactivate secondary sniping mode
            sniping_modifier_active = false;
        }
        // no mode combo - just go back to normal mode
        else{
            bkpd_deactivate_old_mode_and_activate_new(MODE_NORMAL);
        }
    }

    // in all cases, affect DPI
    bkpd_mode_apply_dpi(active_mode->id);
}

void bkpd_mode_set_active(uint8_t id) {
    // test if the mode is valid: search in map
    if (bkpd_mode_is_valid(id)) {
        if (active_mode->id != id) {
            if(id!=MODE_SNIPING){
                // if not currently sniping, replace mode
                if(active_mode->id != MODE_SNIPING) {
                    bkpd_deactivate_old_mode_and_activate_new(id);
                    sniping_modifier_active = false;
                }
                // if currently sniping, activate mode and mark sniping as modifier
                else if((active_mode->id == MODE_SNIPING || sniping_modifier_active)) {
                    bkpd_deactivate_old_mode_and_activate_new(id);
                    sniping_modifier_active = true;
                }
            }
            // we want to switch to sniping mode
            else{
                // if previous mode was normal mode, just activate sniping as a mode, not as modifier
                if(active_mode->id == MODE_NORMAL) {
                    bkpd_deactivate_old_mode_and_activate_new(MODE_SNIPING);
                    sniping_modifier_active = false;
                }
                // if previous mode was other than normal mode, activate mode and mark sniping as modifier
                else {
                    // stay in that mode, nothing to impact on active_mode
                    sniping_modifier_active = true;
                }
            }
        }
        bkpd_mode_apply_dpi(id);
    }
}

// activate the mode, or go back to normal mode.
void bkpd_mode_toggle_active(uint8_t mode_id) {
    if (bkpd_mode_is_valid(mode_id)) {
        // are we already in this mode?
        if (active_mode == &modes[mode_id]) {
            // go back to normal mode
            bkpd_mode_set_active(MODE_NORMAL);
            return;
        }
        // else, we were in another mode - we can then switch up the mode.
        else {
            bkpd_mode_set_active(mode_id);
        }
    }
}

report_mouse_t bkpd_process_active_mode(report_mouse_t mouse_report) {
    // invert x/y if needed, regardless of following processing
    if (bkpd_mode_get_invert(active_mode->id, 0)) {
        mouse_report.x = -mouse_report.x;
    }
    if (bkpd_mode_get_invert(active_mode->id, 1)) {
        mouse_report.y = -mouse_report.y;
    }
    if (active_mode->process != NULL) {
        return active_mode->process(mouse_report);
    }
    return mouse_report;
}

// used for RGB indicators
bool bkpd_mode_is_sniping(void) {
    return (active_mode->id == MODE_SNIPING) || sniping_modifier_active;
}

// used to override Argos color manager
bool bkpd_mode_should_handle_rgb(void) {
    if(changing_dpi_settings_for_mode >= 0) {
        return true;
    }
    return false;
}

// used by local shortcuts
void bkpd_mode_cycle_dpi(uint8_t mode_id, bool forward) {
    uint16_t current_dpi = bkpd_mode_get_dpi(mode_id);
    int16_t new_dpi = current_dpi;

    if(forward) {
        new_dpi += bkpd_mode_get_dpi_per_step(mode_id);
    } else {
        new_dpi -= bkpd_mode_get_dpi_per_step(mode_id);
    }

    // cycle on ends
    if(new_dpi > bkpd_mode_get_max_dpi(mode_id)) 
        new_dpi = bkpd_mode_get_minimum_dpi(mode_id);
    if(new_dpi < bkpd_mode_get_minimum_dpi(mode_id) || new_dpi < 0) 
        new_dpi = bkpd_mode_get_max_dpi(mode_id);

    const uint16_t new_dpi_uint16 = (uint16_t)new_dpi;
    bkpd_mode_change_dpi(mode_id, new_dpi_uint16);
}

void bkpd_mode_set_invert(uint8_t mode_id, uint8_t axis_index, bool invert) {
    if(!bkpd_mode_is_valid(mode_id)) return;
    if (axis_index > 1) return;
    printf("setting invert for mode_id: %d, axis_index: %d, invert: %d\n", mode_id, axis_index, invert);
    if (axis_index == 0) {
        g_bkpd_config.modes_config[mode_id].invert_x = invert;
    } else {
        g_bkpd_config.modes_config[mode_id].invert_y = invert;
    }
    write_bkpd_config_to_eeprom();
}

uint16_t bkpd_mode_get_minimum_dpi(uint8_t mode_id) {
    if(!bkpd_mode_is_valid(mode_id)) return 0;
    return g_bkpd_config.modes_config[mode_id].minimum_dpi;
}

uint16_t bkpd_mode_get_max_dpi(uint8_t mode_id) {
    if(!bkpd_mode_is_valid(mode_id)) return 0;
    return g_bkpd_config.modes_config[mode_id].max_dpi_steps * bkpd_mode_get_dpi_per_step(mode_id) + bkpd_mode_get_minimum_dpi(mode_id);
}

bool bkpd_mode_get_invert(uint8_t mode_id, uint8_t axis_index) {
    if(!bkpd_mode_is_valid(mode_id)) return false;
    if (axis_index > 1) return false;
    if (axis_index == 0) {
        return g_bkpd_config.modes_config[mode_id].invert_x;
    } else {
        return g_bkpd_config.modes_config[mode_id].invert_y;
    }
}

uint16_t bkpd_mode_get_dpi_per_step(uint8_t mode_id) {
    if(!bkpd_mode_is_valid(mode_id)) return 0;
    return g_bkpd_config.modes_config[mode_id].dpi_per_step;
}

/* -----------------------------------------------------------------------------
            Helper functions for current active mode */

void bkpd_mode_current_cycle_dpi(bool forward) {
    printf("cycle dpi for mode_id: %d, forward: %d\n", active_mode->id, forward);
    bkpd_mode_cycle_dpi(active_mode->id, forward);
}

void bkpd_mode_current_invert_axis(bool axis) {
    if (active_mode->invert_axis != NULL) {
        active_mode->invert_axis(axis);
    }
}

uint16_t bkpd_mode_get_dpi(uint8_t mode_id) {
    if(!bkpd_mode_is_valid(mode_id)) return 0;
    return g_bkpd_config.modes_config[mode_id].current_dpi_step * bkpd_mode_get_dpi_per_step(mode_id) + bkpd_mode_get_minimum_dpi(mode_id);
}

void bkpd_mode_apply_dpi(uint8_t mode_id) {
    if(!bkpd_mode_is_valid(mode_id)) return;

    uint16_t new_dpi = bkpd_mode_get_dpi(mode_id);
    
    // clamp
    if (new_dpi > bkpd_mode_get_max_dpi(mode_id)) new_dpi = bkpd_mode_get_max_dpi(mode_id);
    if (new_dpi < bkpd_mode_get_minimum_dpi(mode_id)) new_dpi = bkpd_mode_get_minimum_dpi(mode_id);

    if(sniping_modifier_active) {
        new_dpi = new_dpi/2;
    }

    printf("new_dpi: %d\n", new_dpi);

    if (new_dpi == pointing_device_get_cpi()) return;
    pointing_device_set_cpi(new_dpi);
}

void bkpd_mode_change_dpi(uint8_t mode_id, uint16_t new_dpi) {
    if(!bkpd_mode_is_valid(mode_id)) return;
    // calculate step from new DPI
    // essentially the inverse of the formula in bkpd_mode_get_dpi()
    uint8_t new_dpi_step = (new_dpi - bkpd_mode_get_minimum_dpi(mode_id)) / bkpd_mode_get_dpi_per_step(mode_id);

    g_bkpd_config.modes_config[mode_id].current_dpi_step = new_dpi_step;
    printf("applied dpi from bytes: mode_id: %d, new_dpi_step: %d\n", mode_id, new_dpi_step);
    write_bkpd_config_to_eeprom();

    // if active mode (eg. Normal), then apply immediately
    if (active_mode->id == mode_id) {
        bkpd_mode_apply_dpi(mode_id);
    }
}

static void accumulate_buffer(int16_t *buffer_x, int16_t *buffer_y, int8_t dx, int8_t dy) {
    *buffer_x += dx;
    *buffer_y += dy;
}

/* -----------------------------------------------------------------------------
            Legacy helpers for backwards compatibility */

void bkpd_mode_legacy_sniping_cycle_dpi(bool forward) {
    bkpd_mode_cycle_dpi(MODE_SNIPING, forward);
}

/* -----------------------------------------------------------------------------
            Normal mode */

/* -----------------------------------------------------------------------------
            Custom mode stuff */

void bkpd_custom_mode_set_keys(uint8_t mode_id, uint8_t *mode_config) {
        // only allow setting custom mode if we are in custom mode
        // in one of the 5
        if(mode_id < MODE_CUSTOM1 || mode_id > MODE_CUSTOM5) {
            return;
        }
        uint8_t custom_mode_index = mode_id - MODE_CUSTOM1;
        
        // extract the data
        uint16_t keycode_left = mode_config[0] | mode_config[1] << 8;
        uint16_t keycode_right = mode_config[2] | mode_config[3] << 8;
        uint16_t keycode_up = mode_config[4] | mode_config[5] << 8;
        uint16_t keycode_down = mode_config[6] | mode_config[7] << 8;
        printf("set custom mode, id=%d, keycode_left=%d, keycode_right=%d, keycode_up=%d, keycode_down=%d\n", mode_id, keycode_left, keycode_right, keycode_up, keycode_down);
    
        g_bkpd_config.custom_modes_config[custom_mode_index].keycode_left = keycode_left;
        g_bkpd_config.custom_modes_config[custom_mode_index].keycode_right = keycode_right;
        g_bkpd_config.custom_modes_config[custom_mode_index].keycode_up = keycode_up;
        g_bkpd_config.custom_modes_config[custom_mode_index].keycode_down = keycode_down;
        write_bkpd_config_to_eeprom();
}

// essentially same as cursor mode but with custom keys
// TODO extract duplicate code in function
report_mouse_t bkpd_mode_custom_process(report_mouse_t mouse_report) {
    printf("custom mode process, id=%d\n", active_mode->id);
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;

    uint8_t custom_mode_index = active_mode->id - MODE_CUSTOM1;

    uint16_t keycode_left = g_bkpd_config.custom_modes_config[custom_mode_index].keycode_left;
    uint16_t keycode_right = g_bkpd_config.custom_modes_config[custom_mode_index].keycode_right;
    uint16_t keycode_up = g_bkpd_config.custom_modes_config[custom_mode_index].keycode_up;
    uint16_t keycode_down = g_bkpd_config.custom_modes_config[custom_mode_index].keycode_down;

    if (abs(buffer_x) > BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_X) {
        printf("tap keycode_right: %d, keycode_left: %d\n", keycode_right, keycode_left);
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_tap(buffer_x > 0 ? keycode_right : keycode_left);
#else
        tap_code(buffer_x > 0 ? keycode_right : keycode_left);
#endif
        buffer_x = 0;
        // only allow to go horizontal or vertical at a time
        buffer_y = 0;
    }
    if (abs(buffer_y) > BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_Y) {
        printf("tap keycode_down: %d, keycode_up: %d\n", keycode_down, keycode_up);
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_tap(buffer_y > 0 ? keycode_down : keycode_up);
#else
        tap_code(buffer_y > 0 ? keycode_down : keycode_up);
#endif
        buffer_y = 0;
        // only allow to go horizontal or vertical at a time
        buffer_x = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            Sniping mode */

/* -----------------------------------------------------------------------------
            Dragscroll mode */

report_mouse_t bkpd_mode_dragscroll_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    if (abs(buffer_x) > BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE) {
        mouse_report.h = buffer_x > 0 ? 1 : -1;
        buffer_x       = 0;
    }
    if (abs(buffer_y) > BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE) {
        mouse_report.v = buffer_y > 0 ? 1 : -1;
        buffer_y       = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            Cursor mode */

report_mouse_t bkpd_mode_cursor_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    if (abs(buffer_x) > BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_X) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_tap(buffer_x > 0 ? KC_RIGHT : KC_LEFT);
        printf("buffer x: %d\n", buffer_x);
#else
        tap_code(buffer_x > 0 ? KC_RIGHT : KC_LEFT);
#endif
        buffer_x = 0;
        // only allow to go horizontal or vertical at a time
        buffer_y = 0;
    }
    if (abs(buffer_y) > BK_POINTING_DEVICE_CURSOR_BUFFER_SIZE_Y) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_tap(buffer_y > 0 ? KC_DOWN : KC_UP);
#else
        tap_code(buffer_y > 0 ? KC_DOWN : KC_UP);
#endif
        buffer_y = 0;
        // only allow to go horizontal or vertical at a time
        buffer_x = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            Brightness mode */

report_mouse_t bkpd_mode_brightness_process(report_mouse_t mouse_report) {
#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0; 
    mouse_report.y = 0;
    if (abs(buffer_y) > BK_POINTING_DEVICE_BRIGHTNESS_BUFFER_SIZE) {
        (buffer_y > 0)? rgb_matrix_decrease_val() : rgb_matrix_increase_val();
        buffer_y = 0;
    }
    return mouse_report;
#endif
}

/* -----------------------------------------------------------------------------
            Zoom mode */

report_mouse_t bkpd_mode_zoom_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    // tap Control + plus or minus
    if (abs(buffer_y) > BK_POINTING_DEVICE_ZOOM_BUFFER_SIZE) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_down(KC_LCTL);
        argos_keycode_tap(buffer_y < 0 ? KC_PLUS : KC_MINUS);
        argos_keycode_up(KC_LCTL);
#else
        register_code(KC_LCTL);
        tap_code(buffer_y < 0 ? KC_PLUS : KC_MINUS);
        unregister_code(KC_LCTL);
#endif
        buffer_y = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            Volume mode */

report_mouse_t bkpd_mode_volume_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    // tap volume keys
    if (abs(buffer_y) > BK_POINTING_DEVICE_VOLUME_BUFFER_SIZE) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_tap(buffer_y < 0 ? KC_VOLU : KC_VOLD);
#else
        tap_code(buffer_y < 0 ? KC_VOLU : KC_VOLD);
#endif
        buffer_y = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            Tab switch mode */

report_mouse_t bkpd_mode_tab_switch_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    // tap Control + plus or minus
    if (abs(buffer_x) > BK_POINTING_DEVICE_TAB_SWITCH_BUFFER_SIZE) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_down(KC_LCTL);
        if (buffer_x < 0) {
            argos_keycode_down(KC_LSFT);
        }
        argos_keycode_tap(KC_TAB);
        if (buffer_x < 0) {
            argos_keycode_up(KC_LSFT);
        }
        argos_keycode_up(KC_LCTL);
#else
        register_code(KC_LCTL);
        if (buffer_x < 0) {
            register_code(KC_LSFT);
        }
        tap_code(KC_TAB);
        if (buffer_x < 0) {
            unregister_code(KC_LSFT);
        }
        unregister_code(KC_LCTL);
#endif
        buffer_x = 0;
    }
    return mouse_report;
}

/* -----------------------------------------------------------------------------
            History mode */

report_mouse_t bkpd_mode_history_process(report_mouse_t mouse_report) {
    static int16_t buffer_x = 0;
    static int16_t buffer_y = 0;
    accumulate_buffer(&buffer_x, &buffer_y, mouse_report.x, mouse_report.y);
    mouse_report.x = 0;
    mouse_report.y = 0;
    // tap Control + plus or minus
    if (abs(buffer_x) > BK_POINTING_DEVICE_TAB_SWITCH_BUFFER_SIZE) {
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
        argos_keycode_down(KC_LCTL);
        if (buffer_x > 0) {
            argos_keycode_down(KC_LSFT);
        }
        argos_keycode_tap(KC_Z);
        if (buffer_x > 0) {
            argos_keycode_up(KC_LSFT);
        }
        argos_keycode_up(KC_LCTL);
#else
        register_code(KC_LCTL);
        if (buffer_x > 0) {
            register_code(KC_LSFT);
        }
        tap_code(KC_Z);
        if (buffer_x > 0) {
            unregister_code(KC_LSFT);
        }
        unregister_code(KC_LCTL);
#endif
        buffer_x = 0;
    }
    return mouse_report;
}