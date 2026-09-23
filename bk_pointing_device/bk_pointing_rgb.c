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

 #include "bk_pointing_rgb.h"
 #include "bk_pointing_device.h"
 #include "argos_rgb.h"

 extern int8_t changing_dpi_settings_for_mode;
 extern bkpd_config_t g_bkpd_config;

/*
 *   \brief Manage a visual indicator of the DPI/Sniping DPI that's being changed.
 */
#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)

bool bkpd_is_changing_dpi_settings(void) {
    return (changing_dpi_settings_for_mode >= 0);
}

// RGB indicators on the mouse layer
bool rgb_matrix_indicators_advanced_bk_pointing_device(uint8_t led_min, uint8_t led_max) {
    const uint8_t layer = get_highest_layer(layer_state);

    if (layer != AUTO_MOUSE_DEFAULT_LAYER) {
        return true; // process further in parent function
    }

    // DPI change  -------------------------------------------------------

    if (changing_dpi_settings_for_mode >= 0) {
        // TODO move calculations here directly
        uint8_t  steps_per_led = 1;
        uint8_t  max_steps     = 0;
        uint8_t  current_step  = 0;
        uint16_t min_index     = LED_DPI_INDICATOR_INDEX;
        RGB      color         = {0, 255, 0};

        // handle brightness
        color.r = (color.r * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        color.g = (color.g * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        color.b = (color.b * rgb_matrix_get_val()) / RGB_MATRIX_MAXIMUM_BRIGHTNESS;

        max_steps    = g_bkpd_config.modes_config[changing_dpi_settings_for_mode].max_dpi_steps;
        current_step = g_bkpd_config.modes_config[changing_dpi_settings_for_mode].current_dpi_step;

        // up to 8 LEDs MAX, otherwise we divide
        uint8_t max_led_steps = 8;
        if (max_steps > max_led_steps) {
            uint8_t modulo = max_steps / max_led_steps + 1; // round up
            steps_per_led  = modulo;
            max_steps      = max_steps / modulo + max_steps % (max_led_steps-1);
        } else {
            steps_per_led = 1;
        }

        // max leds we will light, we divide by 2 otherwise it's a lot of LEDs
        for (int i = led_min; i < led_max; i++) {
            // TODO handle non-argos? (not really possible right now)
            // light up only the primary side.
            uint8_t index_symmetric = i % (RGBLIGHT_LED_COUNT / 2);

            // handle LEDs if we are in range of the display bar
            if (index_symmetric >= min_index && index_symmetric < min_index + max_steps) {
                // handle last step (could be full or half brightness)
                uint8_t last_step = min_index + current_step / steps_per_led;
                if (index_symmetric == last_step) {
                    // half brightness for odd DPI steps (2 steps/LED)
                    // if (steps_per_led == 2 && current_step % steps_per_led == 0) {
                    //     rgb_matrix_set_color(i, (color.r + 255) / 2, color.g / 2, 0);
                    // }
                    if(steps_per_led > 1 && current_step % steps_per_led != 0) {
                        uint8_t offset = current_step % steps_per_led;
                        // TODO this is hardcoded, meh
                        rgb_matrix_set_color(i, 255*offset / steps_per_led, 255 * offset / steps_per_led, 0);
                    }
                    // red
                    else {
                        rgb_matrix_set_color(i, 255, 0, 0);
                    }
                }
                // handle LEDs before the current step that are still active
                else if (index_symmetric < last_step) {
                    rgb_matrix_set_color(i, color.r, color.g, color.b);
                }
                // handle other LEDs (not active but in display bar)
                else {
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
            else {
                rgb_matrix_set_color(i, 0, 0, 0);
            }
        }
        return false;
    }
    return true; // process further in parent function
}

#endif