/*
 * Copyright 2020 Christopher Courtney <drashna@live.com> (@drashna)
 * Copyright 2021 Quentin LEBASTARD <qlebastard@gmail.com>
 * Copyright 2021 Charly Delay <charly@codesink.dev> (@0xcharly)
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

 /*
    Generic pointing device configuration and features.
 */

#include QMK_KEYBOARD_H
#include "bk_pointing_device.h"
#include "transactions.h"
#include <string.h>
#include "math.h"
#include "introspection.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif // CONSOLE_ENABLE

#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
#include "argos.h"
#include "argos_rgb.h"
#endif

#ifdef POINTING_DEVICE_DRIVER_digitizer
#include "digitizer.h"
#endif

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 0, 0);

// TODO store those in config?
#define BK_POINTING_DEVICE_MINIMUM_DEFAULT_DPI 400
#define BK_POINTING_DEVICE_DEFAULT_DPI_CONFIG_STEP 200
#define BK_POINTING_DEVICE_MINIMUM_SNIPING_DPI 200
#define BK_POINTING_DEVICE_SNIPING_DPI_CONFIG_STEP 100
#define BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_DPI 100
#define BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE 6

#define BK_POINTING_DEVICE_MAX_DPI_BYTES 4
#define BK_POINTING_DEVICE_MAX_SNIPING_DPI_BYTES 2

#ifdef POINTING_DEVICE_DRIVER_digitizer
#define BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE_DIGITIZER 600
#endif

typedef union {
    uint8_t raw;
    struct {
        uint8_t pointer_default_dpi : BK_POINTING_DEVICE_MAX_DPI_BYTES; // 16 steps available.
        uint8_t pointer_sniping_dpi : BK_POINTING_DEVICE_MAX_SNIPING_DPI_BYTES; // 4 steps available.
        bool    is_dragscroll_enabled : 1;
        bool    is_sniping_enabled : 1;
        bool auto_mouse_layer_enabled : 1;
        bool auto_precision_on_mouse_layer_enabled : 1;
        bool dragscroll_axis_invert_x : 1;
        bool dragscroll_axis_invert_y : 1;
        bool has_copied_qmk_config : 1;
        // TODO for dpi: init at #define value
    } __attribute__((packed));
} bkpd_config_t;

static bkpd_config_t g_bkpd_config = {0};
static bool changing_dpi_settings = false;
static bool changing_sniping_dpi_settings = false;

/**
* \brief Set the value of `config` from EEPROM.
*
* Note that `is_dragscroll_enabled` and `is_sniping_enabled` are purposefully
* ignored since we do not want to persist this state to memory.  In practice,
* this state is always written to maximize write-performances.  Therefore, we
* explicitly set them to `false` in this function.
*/
static void read_bkpd_config_from_eeprom(void) {
// TODO: replace with per-module memory management
#if defined(BK_HAS_POINTING_DEVICE) && defined(COMMUNITY_MODULE_ARGOS_ENABLE)
argos_read_eeprom(ARGOS_OFFSET_POINTER_CONFIG, &g_bkpd_config, sizeof(bkpd_config_t));
#else
    g_bkpd_config.raw                   = eeconfig_read_kb() & 0xff;
#endif
    g_bkpd_config.is_dragscroll_enabled = false;
    g_bkpd_config.is_sniping_enabled    = false;
}

/**
* \brief Save the value of `config` to eeprom.
*
* Note that all values are written verbatim, including whether drag-scroll
* and/or sniper mode are enabled.  `read_bkpd_config_from_eeprom(…)`
* resets these 2 values to `false` since it does not make sense to persist
* these across reboots of the board.
*/
static void write_bkpd_config_to_eeprom(void) {
// TODO: replace with per-module memory management
#if defined(BK_HAS_POINTING_DEVICE) && defined(COMMUNITY_MODULE_ARGOS_ENABLE)
    argos_write_eeprom(ARGOS_OFFSET_POINTER_CONFIG, &g_bkpd_config, sizeof(bkpd_config_t));
#else
    eeconfig_update_kb(g_bkpd_config.raw);
#endif
}

/** \brief Return the current value of the pointer's default DPI. */
uint16_t bkpd_get_pointer_default_dpi(void) {
    return (uint16_t)g_bkpd_config.pointer_default_dpi * bkpd_get_default_dpi_config_step() + bkpd_get_minimum_default_dpi();
}

/** \brief Return the current value of the pointer's sniper-mode DPI. */
uint16_t bkpd_get_pointer_sniping_dpi(void) {
    return (uint16_t)g_bkpd_config.pointer_sniping_dpi * bkpd_get_sniping_dpi_config_step() + bkpd_get_minimum_sniping_dpi();
}

/** \brief Set the appropriate DPI for the input config. */
static void bkpd_maybe_update_cpi(void) {
    if (g_bkpd_config.is_dragscroll_enabled) {
        pointing_device_set_cpi(BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_DPI);
    } else if (g_bkpd_config.is_sniping_enabled) {
        pointing_device_set_cpi(bkpd_get_pointer_sniping_dpi());
    } else {
        pointing_device_set_cpi(bkpd_get_pointer_default_dpi());
    }
}

/**
* \brief Update the pointer's default DPI to the next or previous step.
*
* Increases the DPI value if `forward` is `true`, decreases it otherwise.
* The increment/decrement steps are equal to BK_BK_POINTING_DEVICE_DEFAULT_DPI_CONFIG_STEP.
*/
void bkpd_cycle_pointer_default_dpi_noeeprom(bool forward) {
    g_bkpd_config.pointer_default_dpi += forward ? 1 : -1;
    bkpd_maybe_update_cpi();
}

void bkpd_cycle_pointer_default_dpi(bool forward) {
    bkpd_cycle_pointer_default_dpi_noeeprom(forward);
    write_bkpd_config_to_eeprom();
}

/**
* \brief Update the pointer's sniper-mode DPI to the next or previous step.
*
* Increases the DPI value if `forward` is `true`, decreases it otherwise.
* The increment/decrement steps are equal to BK_POINTING_DEVICE_SNIPING_DPI_CONFIG_STEP.
*/
void bkpd_cycle_pointer_sniping_dpi_noeeprom(bool forward) {
    g_bkpd_config.pointer_sniping_dpi += forward ? 1 : -1;
    printf("bkpd_cycle_pointer_sniping_dpi_noeeprom: %d\n", g_bkpd_config.pointer_sniping_dpi);
    bkpd_maybe_update_cpi();
}

void bkpd_cycle_pointer_sniping_dpi(bool forward) {
    bkpd_cycle_pointer_sniping_dpi_noeeprom(forward);
    write_bkpd_config_to_eeprom();
}

bool bkpd_get_pointer_sniping_enabled(void) {
    return g_bkpd_config.is_sniping_enabled;
}

void bkpd_set_pointer_sniping_enabled(bool enable) {
    g_bkpd_config.is_sniping_enabled = enable;
    bkpd_maybe_update_cpi();
}

void bkpd_set_auto_mouse_layer_enabled(bool enabled) {
    g_bkpd_config.auto_mouse_layer_enabled = enabled;
    set_auto_mouse_enable(enabled);
    write_bkpd_config_to_eeprom();
}

void bkpd_set_auto_precision_on_mouse_layer_enabled(bool enabled) {
    g_bkpd_config.auto_precision_on_mouse_layer_enabled = enabled;
    bkpd_maybe_update_cpi();
}

bool bkpd_get_auto_mouse_layer_enabled(void) {
    return g_bkpd_config.auto_mouse_layer_enabled;
}

bool bkpd_get_auto_precision_on_mouse_layer_enabled(void) {
    return g_bkpd_config.auto_precision_on_mouse_layer_enabled;
}

bool bkpd_get_pointer_dragscroll_enabled(void) {
    return g_bkpd_config.is_dragscroll_enabled;
}

void bkpd_set_pointer_dragscroll_enabled(bool enable) {
    g_bkpd_config.is_dragscroll_enabled = enable;
    bkpd_maybe_update_cpi();
}

uint16_t bkpd_get_minimum_default_dpi(void) {
    return BK_POINTING_DEVICE_MINIMUM_DEFAULT_DPI;
}

uint16_t bkpd_get_default_dpi_config_step(void) {
    return BK_POINTING_DEVICE_DEFAULT_DPI_CONFIG_STEP;
}

uint16_t bkpd_get_minimum_sniping_dpi(void) {
    return BK_POINTING_DEVICE_MINIMUM_SNIPING_DPI;
}

uint16_t bkpd_get_sniping_dpi_config_step(void) {
    return BK_POINTING_DEVICE_SNIPING_DPI_CONFIG_STEP;
}

/**
* \brief Implement drag-scroll.
*/
report_mouse_t pointing_device_task_bk_pointing_device(report_mouse_t mouse_report) {
    if (is_keyboard_master()) {    
        static int16_t scroll_buffer_x = 0;
        static int16_t scroll_buffer_y = 0;
        if (g_bkpd_config.is_dragscroll_enabled) {
            scroll_buffer_x += (g_bkpd_config.dragscroll_axis_invert_x ? -1 : 1) * mouse_report.x;
            scroll_buffer_y += (g_bkpd_config.dragscroll_axis_invert_y ? -1 : 1) * mouse_report.y;
            mouse_report.x = 0;
            mouse_report.y = 0;
            if (abs(scroll_buffer_x) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE) {
                mouse_report.h = scroll_buffer_x > 0 ? 1 : -1;
                scroll_buffer_x = 0;
            }
            if (abs(scroll_buffer_y) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE) {
                mouse_report.v = scroll_buffer_y > 0 ? 1 : -1;
                scroll_buffer_y = 0;
            }
        }
        mouse_report = pointing_device_task_user(mouse_report);
    }
    return mouse_report;
}

// TODO missing && !NO_DILEMMA_KEYCODES?
//  #    if defined(BK_POINTING_DEVICE_ENABLE) && !defined(NO_BK_POINTING_DEVICE_KEYCODES)
/** \brief Whether SHIFT mod is enabled. */
static bool has_shift_mod(void) {
#        ifdef NO_ACTION_ONESHOT
    return mod_config(get_mods()) & MOD_MASK_SHIFT;
#        else
    return mod_config(get_mods() | get_oneshot_mods()) & MOD_MASK_SHIFT;
#        endif // NO_ACTION_ONESHOT
}
//  #    endif // BK_POINTING_DEVICE_ENABLE && !NO_BK_POINTING_DEVICE_KEYCODES

/**
* \brief Process keycodes related to the pointing device.
*/
bool process_record_bk_pointing_device(uint16_t keycode, keyrecord_t* record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case DPI_MOD:
            if (record->event.pressed) {
                // Step backward if shifted, forward otherwise.
                bkpd_cycle_pointer_default_dpi(/* forward= */ !has_shift_mod());
                changing_dpi_settings = true;
                changing_sniping_dpi_settings = false;
                printf("DPI_MOD: %d\n", g_bkpd_config.pointer_default_dpi);
            }
            break;
        case DPI_RMOD:
            if (record->event.pressed) {
                // Step forward if shifted, backward otherwise.
                bkpd_cycle_pointer_default_dpi(/* forward= */ has_shift_mod());
                changing_dpi_settings = true;
                changing_sniping_dpi_settings = false;
                printf("DPI_RMOD: %d\n", g_bkpd_config.pointer_default_dpi);
            }
            break;
        case S_D_MOD:
            if (record->event.pressed) {
                // Step backward if shifted, forward otherwise.
                bkpd_cycle_pointer_sniping_dpi(/* forward= */ !has_shift_mod());
                changing_sniping_dpi_settings = true;
                changing_dpi_settings = false;
                printf("S_D_MOD: %d\n", g_bkpd_config.pointer_sniping_dpi);
            }
            break;
        case S_D_RMOD:
            if (record->event.pressed) {
                // Step forward if shifted, backward otherwise.
                bkpd_cycle_pointer_sniping_dpi(/* forward= */ has_shift_mod());
                changing_sniping_dpi_settings = true;
                changing_dpi_settings = false;
                printf("S_D_RMOD: %d\n", g_bkpd_config.pointer_sniping_dpi);
            }
            break;
        case SNIPING:
            bkpd_set_pointer_sniping_enabled(record->event.pressed);
            break;
        case SNP_TOG:
            if (record->event.pressed) {
                bkpd_set_pointer_sniping_enabled(!bkpd_get_pointer_sniping_enabled());
            }
            break;
        case DRGSCRL:
            bkpd_set_pointer_dragscroll_enabled(record->event.pressed);
            break;
        case DRG_TOG:
            if (record->event.pressed) {
                bkpd_set_pointer_dragscroll_enabled(!bkpd_get_pointer_dragscroll_enabled());
            }
            break;
        default:
            changing_dpi_settings = false;
            changing_sniping_dpi_settings = false;
            break;
    }
    return true;
}

bool bpkd_is_changing_dpi_settings(void) {
    return changing_dpi_settings || changing_sniping_dpi_settings;
}

/*
*   \brief Manage a visual indicator of the DPI/Sniping DPI that's being changed.
*/
// TODO handle secondary side? ....
bool rgb_matrix_indicators_advanced_bk_pointing_device(uint8_t led_min, uint8_t led_max) {
    // printf("rgb_matrix_indicators_advanced_bk_pointing_device: %d\n", led_min);
    const uint8_t layer = get_highest_layer(layer_state);

    if(layer != AUTO_MOUSE_DEFAULT_LAYER) {
        changing_dpi_settings = false;
        changing_sniping_dpi_settings = false;
        return true; // process further in parent function
    }

    uint8_t steps_per_led = 1;
    uint8_t max_steps = 0;
    uint8_t current_step = 0;
    uint16_t min_index = LED_DPI_INDICATOR_INDEX;
    RGB color = {0, 255, 0};

    // handle brightness
    color.r = (color.r * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    color.g = (color.g * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    color.b = (color.b * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;

    if(changing_dpi_settings) {
        steps_per_led = 2;
        max_steps = pow(2, BK_POINTING_DEVICE_MAX_DPI_BYTES) / steps_per_led;
        current_step = g_bkpd_config.pointer_default_dpi;
    } else if(changing_sniping_dpi_settings) {
        steps_per_led = 1;
        max_steps = pow(2, BK_POINTING_DEVICE_MAX_SNIPING_DPI_BYTES) / steps_per_led;
        current_step = g_bkpd_config.pointer_sniping_dpi;
    }

    if(changing_dpi_settings || changing_sniping_dpi_settings) {
        // max leds we will light, we divide by 2 otherwise it's a lot of LEDs
        for(int i = led_min; i < led_max; i++) {
            // TODO handle non-argos? (not really possible right now)
            // light up only the primary side.
            uint8_t index_symmetric = i % (RGBLIGHT_LED_COUNT / 2);
       
            // handle LEDs if we are in range of the display bar
            if( index_symmetric >= min_index && index_symmetric < min_index + max_steps) {
                // handle last step (could be full or half brightness)
                uint8_t last_step = min_index + current_step/steps_per_led;
                if(index_symmetric == last_step) {
                    // half brightness for odd DPI steps (2 steps/LED)
                    if(steps_per_led == 2 && current_step % steps_per_led == 0) {
                        rgb_matrix_set_color(i, (color.r+255)/2, color.g/2, color.b/2);
                    }
                    // full brightness for sniping DPI and even DPI steps
                    else {
                        rgb_matrix_set_color(i, color.r, color.g, color.b);
                    }
                }
                // handle LEDs before the current step that are still active
                else if(index_symmetric < last_step) {
                    rgb_matrix_set_color(i, color.r, color.g, color.b);
                }
                // handle other LEDs (not active but in display bar)
                else{
                    // default red
                    RGB red = {255, 0, 0};                
                    // handle brightness
                    red.r = (red.r * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
                    red.g = (red.g * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
                    red.b = (red.b * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
                    rgb_matrix_set_color(i, red.r, red.g, red.b);
                }
            }
            // turn off all other leds
            else{
                rgb_matrix_set_color(i, 0, 0, 0);
            }
        }
        return false;
    }
    return true; // process further in parent function
}

/**
* \brief Initialize the pointing device.
* Manages memory space for Argos an non-Argos configuration.
* Copies the defined invert x/y axis configuration into dynamic memory.
*/
void keyboard_post_init_bk_pointing_device(void) {
    read_bkpd_config_from_eeprom();
    bkpd_maybe_update_cpi();
    // TODO: replace with per-module memory management
#ifdef COMMUNITY_MODULE_ARGOS_ENABLE
#else
    eeconfig_init_user();
#endif
    set_auto_mouse_layer(AUTO_MOUSE_DEFAULT_LAYER );
    if(g_bkpd_config.auto_mouse_layer_enabled) {
        set_auto_mouse_enable(true);
    } else {
        set_auto_mouse_enable(false);
    }

    if(!g_bkpd_config.has_copied_qmk_config) {
        g_bkpd_config.has_copied_qmk_config = true;
#ifdef BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_REVERSE_X
        g_bkpd_config.dragscroll_axis_invert_x = true;
#endif
#ifdef BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_REVERSE_Y
        g_bkpd_config.dragscroll_axis_invert_y = true;
#endif
        write_bkpd_config_to_eeprom();
    }
}


/**
* \brief Switch to precision mode on mouse layer if that option is enabled.
*/
layer_state_t layer_state_set_bk_pointing_device(layer_state_t state) {
    if(g_bkpd_config.auto_precision_on_mouse_layer_enabled) {
        bkpd_set_pointer_sniping_enabled(layer_state_cmp(state, AUTO_MOUSE_DEFAULT_LAYER));
    }
    return state;
}

void bkpd_set_dragscroll_axis_invert_x(bool invert) {
    g_bkpd_config.dragscroll_axis_invert_x = invert;
    write_bkpd_config_to_eeprom();
}

void bkpd_set_dragscroll_axis_invert_y(bool invert) {
    g_bkpd_config.dragscroll_axis_invert_y = invert;
    write_bkpd_config_to_eeprom();
}

void bkpd_set_dragscroll_dpi(uint16_t dpi) {
    // TODO
    // g_bkpd_config.dragscroll_dpi = dpi;
    // write_bkpd_config_to_eeprom(&g_bkpd_config);
}

bool bkpd_get_dragscroll_axis_invert_x(void) {
    return g_bkpd_config.dragscroll_axis_invert_x;
}

bool bkpd_get_dragscroll_axis_invert_y(void) {
    return g_bkpd_config.dragscroll_axis_invert_y;
}   

uint16_t bkpd_get_dragscroll_dpi(void) {
    // TODO
    return 0;
}

/**
* \brief Auto mouse layer implementation for trackpads
* We override the kb task, because QMK does not provide (as of coding this) a module-level override.
*/
#ifdef POINTING_DEVICE_DRIVER_digitizer
bool digitizer_task_kb(digitizer_t *const digitizer_state) {
    report_mouse_t report = {0};
    static digitizer_t last_report    = {0};
    uint16_t delta_x = 0;
    uint16_t delta_y = 0;

    for (int i = 0; i < DIGITIZER_CONTACT_COUNT; i++) {
#if DIGITIZER_FINGER_COUNT > 0
        if (i < DIGITIZER_FINGER_COUNT) {
            delta_x += digitizer_state->contacts[i].x - last_report.contacts[i].x;
            delta_y += digitizer_state->contacts[i].y - last_report.contacts[i].y;
        }
#endif
    }

    // "fake copy" it into the mouse report so that the auto mouse layer may trigger if needed
    report.x = delta_x;
    report.y = delta_y;
    pointing_device_task_auto_mouse(report);

    last_report = *digitizer_state; // copy the state to the last report

    // Dragscroll implementation for trackpads
    static int16_t scroll_buffer_x = 0;
    static int16_t scroll_buffer_y = 0;
    if (g_bkpd_config.is_dragscroll_enabled) {
        scroll_buffer_x += (g_bkpd_config.dragscroll_axis_invert_x ? -1 : 1) * report.x;
        scroll_buffer_y += (g_bkpd_config.dragscroll_axis_invert_y ? -1 : 1) * report.y;
        report.x = 0;
        report.y = 0;
        // prevent bounceback issues
        if(abs(scroll_buffer_x) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE_DIGITIZER+200) {
            scroll_buffer_x = 0;
        }
        if(abs(scroll_buffer_y) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE_DIGITIZER+200) {
            scroll_buffer_y = 0;
        }
        if (abs(scroll_buffer_x) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE_DIGITIZER) {
            report.h = scroll_buffer_x > 0 ? 1 : -1;
            scroll_buffer_x = 0;
        }
        if (abs(scroll_buffer_y) > BK_POINTING_DEVICE_BK_POINTING_DEVICE_DRAGSCROLL_BUFFER_SIZE_DIGITIZER) {
            report.v = scroll_buffer_y > 0 ? 1 : -1;
            scroll_buffer_y = 0;
        }
        // manually trigger scroll
        pointing_device_set_report(report);

        // if we are scrolling, cancel out cursor movement
        for (int i = 0; i < DIGITIZER_CONTACT_COUNT; i++) {
#if DIGITIZER_FINGER_COUNT > 0
            if (i < DIGITIZER_FINGER_COUNT) {
                digitizer_state->contacts[i].x = 0;
                digitizer_state->contacts[i].y = 0;
            }
#endif
        }
    } 
    // trigger a button state changed in master
    return true;
}

#endif