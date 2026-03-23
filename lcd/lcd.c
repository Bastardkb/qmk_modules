#include QMK_KEYBOARD_H
#include "introspection.h"

#include "lcd.h"
#include "config.h"
#include "ui_elements.h"
#include "quantum.h"
#include <stdio.h>

#include "qp.h"
#include "qp_comms.h"
#include "qp_st77xx_opcodes.h"
#include "qp_surface.h"
#include "color.h"
#include "transactions.h"

lv_obj_t *ui_screen_base;

// static mod_button_pair_t mod_buttons[4];
// static mod_button_pair_t mouse_buttons[2];

lv_obj_t *ui_label_layer;
lv_obj_t *ui_button_layer;
lv_obj_t *ui_image_scroll;

obj_event_array_t event_with_objects_array;

enum ui_user_events {
    EVENT_LAYER_CHANGE = 0,
    EVENT_MOD_CHANGE,
    EVENT_LAST_EVENT,
};

static dilemma_status_t dilemma_lcd_status_prev = {0};
static dilemma_status_t dilemma_lcd_status      = {0};

// TODO keep this variable contained in theme.c
extern ui_styles_t current_style;

painter_device_t        lcd;
static painter_device_t surface;
// Buffer required for a 240x280 16bpp surface:
static uint8_t surface_buffer[SURFACE_REQUIRED_BUFFER_BYTE_SIZE(LCD_WIDTH, LCD_HEIGHT, 16)];

void init_obj_event_array(obj_event_array_t *a) {
    a->used            = 0;
    a->size            = 0;
    a->amount_elements = 0;
}

void add_obj_event_array(obj_event_array_t *a, obj_update_t element) {
    if (a->used == a->size) {
        a->size  = (a->size * 3) / 2 + 8;
        a->array = realloc(a->array, a->size * sizeof(obj_update_t));
    }
    a->array[a->used++] = element;
    a->amount_elements++;
}

void init_display(void) {
    // Display timeout
    wait_ms(LCD_WAIT_TIME);

    lcd = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_SPI_DIVISOR, SPI_MODE);
    qp_init(lcd, LCD_ROTATION);

    surface = qp_make_rgb565_surface(LCD_WIDTH, LCD_HEIGHT, surface_buffer);
    qp_init(surface, LCD_ROTATION);

    // Display offset
    qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);

    qp_lvgl_attach(lcd);

    // Power on display, fill with black
    qp_power(lcd, 1);
    // keep lcd power off until first loop, once everything has been synced
    // qp_power(lcd, 0);
    qp_rect(lcd, 0, 0, 300, 300, HSV_BLACK, 1);
    qp_flush(lcd);
    ui_screen_base = lv_obj_create(NULL);

    init_themes();
    init_styles();

    lv_obj_t *cont = lv_obj_create(ui_screen_base);
    lv_obj_set_size(cont, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    // TODO move this to theme.c, in eg. create_container
    lv_obj_add_style(cont, &current_style.flex_container, 0);

    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_layer_label(cont),
                                                       &update_layer_name,
                                                   });

    // mod buttons: SHIFT, ALT, CTRL, GUI
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "SHFT", true, MOD_MASK_SHIFT),
                                                       &update_mod_shift,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "ALT", false, MOD_MASK_ALT),
                                                       &update_mod_alt,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "CTRL", false, MOD_MASK_CTRL),
                                                       &update_mod_ctrl,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "GUI", false, MOD_MASK_GUI),
                                                       &update_mod_gui,
                                                   });
    ui_create_line_separator(cont, 1, 3);

    // display base layer screen upon init
    // TODO is this necessary here? can we move it to a spot that makes more sense?
    lv_disp_load_scr(ui_screen_base);

    // mouse special buttons
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "SCROLL", true, 0),
                                                       &update_mod_scroll,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_mod_button(cont, "SNIPE", false, 0),
                                                       &update_mod_snipe,
                                                   });

    // sniping DPI widgets
    ui_create_secondary_text(cont, "SNIPE DPI", true, 4);
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_progress_bar(cont, 4),
                                                       &update_mod_snipe_dpi_bar,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_number_label(cont, 2),
                                                       &update_mod_snipe_dpi_number,
                                                   });

    // regular DPI widgets
    ui_create_secondary_text(cont, "DPI", true, 2);
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_progress_bar(cont, 6),
                                                       &update_mod_dpi_bar,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_number_label(cont, 2),
                                                       &update_mod_dpi_number,
                                                   });

    // line separator
    ui_create_line_separator(cont, 1, 3);

    // rgb widgets
    ui_create_secondary_text(cont, "RGB", true, 2);

    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_progress_bar(cont, 6),
                                                       &update_rgb_bar,
                                                   });
    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_number_label(cont, 2),
                                                       &update_rgb_value,
                                                   });

    add_obj_event_array(&event_with_objects_array, (obj_update_t){
                                                       ui_create_secondary_text(cont, "effect...", true, 1),
                                                       &update_rgb_effect,
                                                   });

    // theme and backgrounds
    // lv_disp_t  *dispp = lv_disp_get_default();
    // lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(BK_PALETTE), lv_palette_main(BK_PALETTE), true, LV_FONT_DEFAULT);
    // lv_disp_set_theme(dispp, theme);
    // lv_obj_set_style_bg_color(cont, lv_color_black(), LV_PART_MAIN);
}

void keyboard_post_init_lcd(void) {
    load_dilemma_theme_config_from_eeprom();

    // copy only the relevant information from eeprom into local config
    // todo move this to update_dilemma_status?
    dilemma_lcd_status.current_theme_id = get_current_theme_id();

    // TODO: the load theme config needs to be done BEFORE the display init. How do we solve this?
    if (is_keyboard_left()) {
        init_obj_event_array(&event_with_objects_array); // TODO move this into init_display?
        init_display();
    }

    // register rpc mouse data syncing
    transaction_register_rpc(RPC_ID_MOUSE_SYNC, mouse_info_sync_handler);

    // if left side is connected, turn the power on
    // otherwise, it will be turned on by the right side when the first sync happens in the housekeeping task
    if (is_keyboard_left() && is_keyboard_master()) {
        // qp_power(lcd, 1);
    }
}

// TODO get colors based on real layer colors, instead of hardcoding them
void update_theme_color(void) {
    // static bool first_display = true;
    // if (get_current_theme().change_colors_on_layer_change) {
    //     if (dilemma_lcd_status.layer != dilemma_lcd_status_prev.layer || first_display) {
    //         HSV hsv;
    //         switch (dilemma_lcd_status.layer) {
    //             case 0:
    //             default:
    //                 hsv.h = 218;
    //                 hsv.s = 70;
    //                 hsv.v = 93;
    //                 break;
    //             case 1:
    //                 hsv.h = 250;
    //                 hsv.s = 100;
    //                 hsv.v = 80;
    //                 break;
    //             case 2:
    //                 hsv.h = 35;
    //                 hsv.s = 100;
    //                 hsv.v = 80;
    //                 break;
    //             case 3:
    //                 hsv.h = 195;
    //                 hsv.s = 30;
    //                 hsv.v = 80;
    //                 break;
    //         }

    //         change_style_colors(hsv);
    //     }
    // }
    // first_display = false;
}

void refresh_lcd_info(void) {
    update_theme_color();

    int i = 0;
    for (i = 0; i < event_with_objects_array.amount_elements; i++) {
        lv_obj_t *obj = event_with_objects_array.array[i].obj;
        if (obj && event_with_objects_array.array[i].update_function) event_with_objects_array.array[i].update_function(obj);
    }
}

void housekeeping_task_lcd(void) {
    if (is_keyboard_master()) {
        update_dilemma_status();
        // if the keyboard is left, nothing to do - just refresh the screen
        if (is_keyboard_left()) {
            refresh_lcd_info();
        }
        // if the keyboard is right, we need to send the sync info over to the left side
        // saving the theme id to eeprom has already been done in process_record
        else {
            bool            needs_sync   = false;
            static bool     needs_resync = true; // perform an initial first sync
            static uint32_t last_sync    = 0;
            // // Check if the state values are different.
            if (memcmp(&dilemma_lcd_status, &dilemma_lcd_status_prev, sizeof(dilemma_lcd_status))) {
                needs_sync = true;
            }
            // check if a previous sync has failed
            if (needs_resync) {
                // we only want to retry syncing after a set amount of time
                if (timer_elapsed32(last_sync) > 200) {
                    needs_sync = true;
                }
            }
            // perform the sync if requested
            if (needs_sync) {
                // try to sync, if it fails we will retry in the next housekeeping loop
                if (transaction_rpc_send(RPC_ID_MOUSE_SYNC, sizeof(dilemma_lcd_status), &dilemma_lcd_status) == false) {
                    needs_resync = true;
                }
                last_sync = timer_read32();
            }
        }

        dilemma_lcd_status_prev = dilemma_lcd_status;
    }
}

// TODO add dilemma layers, not only MAX
void update_layer_name(lv_obj_t *obj) {
    if (dilemma_lcd_status.layer != dilemma_lcd_status_prev.layer) {
        switch (dilemma_lcd_status.layer) {
            case 0:
            default:
                lv_label_set_text(obj, "LAYER: BASE");
                break;
            case 1:
                lv_label_set_text(obj, "LAYER: LOWER");
                break;
            case 2:
                lv_label_set_text(obj, "LAYER: RAISE");
                break;
            case 3:
                lv_label_set_text(obj, "LAYER: MOUSE");
                break;
        }
    }
}

void update_dilemma_status(void) {
    dilemma_lcd_status.mods            = get_mods();
    dilemma_lcd_status.layer           = get_highest_layer(layer_state);
    dilemma_lcd_status.sniping         = dilemma_get_pointer_sniping_enabled();
    dilemma_lcd_status.dpi             = dilemma_get_pointer_default_dpi();
    dilemma_lcd_status.s_dpi           = dilemma_get_pointer_sniping_dpi();
    dilemma_lcd_status.scrolling       = dilemma_get_pointer_dragscroll_enabled();
    dilemma_lcd_status.rgb_enabled     = rgb_matrix_is_enabled();
    dilemma_lcd_status.rgb_effect_mode = rgb_matrix_get_mode();
    dilemma_lcd_status.rgb_val         = rgb_matrix_get_val();
}

void update_mod_shift(lv_obj_t *obj) {
    update_mod_xx(obj, MOD_MASK_SHIFT);
}
void update_mod_ctrl(lv_obj_t *obj) {
    update_mod_xx(obj, MOD_MASK_CTRL);
}
void update_mod_alt(lv_obj_t *obj) {
    update_mod_xx(obj, MOD_MASK_ALT);
}
void update_mod_gui(lv_obj_t *obj) {
    update_mod_xx(obj, MOD_MASK_GUI);
}

void update_mod_xx(lv_obj_t *obj, uint8_t mod_mask) {
    if ((dilemma_lcd_status.mods & mod_mask) != (dilemma_lcd_status_prev.mods & mod_mask)) {
        if ((dilemma_lcd_status.mods & mod_mask)) {
            lv_event_send(obj, LV_EVENT_PRESSED, NULL);
        } else {
            lv_event_send(obj, LV_EVENT_RELEASED, NULL);
        }
    }
}

void update_rgb_effect(lv_obj_t *obj) {
    const bool rgb_change = (dilemma_lcd_status.rgb_enabled != dilemma_lcd_status_prev.rgb_enabled);

    if (!dilemma_lcd_status.rgb_enabled) {
        if (rgb_change) {
            lv_label_set_text(obj, "");
        }
    } else {
        if ((rgb_change) || (dilemma_lcd_status.rgb_effect_mode != dilemma_lcd_status_prev.rgb_effect_mode)) {
            const char *effect_name = rgb_matrix_get_effect_name();
            lv_label_set_text(obj, effect_name);
        }
    }
}

void update_rgb_value(lv_obj_t *obj) {
    const bool rgb_change = (dilemma_lcd_status.rgb_enabled != dilemma_lcd_status_prev.rgb_enabled);

    if (!dilemma_lcd_status.rgb_enabled) {
        if (rgb_change) {
            lv_label_set_text(obj, "Off");
        }
    } else {
        if ((rgb_change) || (dilemma_lcd_status.rgb_val != dilemma_lcd_status_prev.rgb_val)) {
            char rgbval[50];
            sprintf(rgbval, "%u", dilemma_lcd_status.rgb_val);
            lv_label_set_text(obj, rgbval);
        }
    }
}

void update_rgb_bar(lv_obj_t *obj) {
    const bool rgb_change = (dilemma_lcd_status.rgb_enabled != dilemma_lcd_status_prev.rgb_enabled);
    if (!dilemma_lcd_status.rgb_enabled) {
        if (rgb_change) {
            // lv_label_set_text(ui_label_rgb_number, "Off"); // test
            lv_bar_set_value(obj, 0, LV_ANIM_OFF);
        }
    } else {
        if ((rgb_change) || (dilemma_lcd_status.rgb_val != dilemma_lcd_status_prev.rgb_val)) {
            float rel = (float)(dilemma_lcd_status.rgb_val) * 100 / 156;
            lv_bar_set_value(obj, (uint16_t)rel, LV_ANIM_OFF);
        }
    }
}

void update_mod_scroll(lv_obj_t *obj) {
    if (dilemma_lcd_status.scrolling != dilemma_lcd_status_prev.scrolling) {
        if (dilemma_lcd_status.scrolling) {
            lv_event_send(obj, LV_EVENT_PRESSED, NULL);
        } else {
            lv_event_send(obj, LV_EVENT_RELEASED, NULL);
        }
    }
}

void update_mod_snipe(lv_obj_t *obj) {
    if (dilemma_lcd_status.sniping != dilemma_lcd_status_prev.sniping) {
        if (dilemma_lcd_status.sniping) {
            lv_event_send(obj, LV_EVENT_PRESSED, NULL);
        } else {
            lv_event_send(obj, LV_EVENT_RELEASED, NULL);
        }
    }
}

// TODO dynamically get max DPI, instead of using hardcoded values
void update_mod_snipe_dpi_number(lv_obj_t *obj) {
    if (dilemma_lcd_status.s_dpi != dilemma_lcd_status_prev.s_dpi) {
        char c_s_dpi[50];
        sprintf(c_s_dpi, "%u", (uint16_t)dilemma_lcd_status.s_dpi);
        lv_label_set_text(obj, c_s_dpi);
    }
}

// TODO dynamically get max DPI, instead of using hardcoded values
void update_mod_snipe_dpi_bar(lv_obj_t *obj) {
    if (dilemma_lcd_status.s_dpi != dilemma_lcd_status_prev.s_dpi) {
        static const uint16_t rel_max_s_dpi = 100 * 4;
        const float           rel           = (float)((dilemma_lcd_status.s_dpi + 100 - 200)) * 100 / rel_max_s_dpi;
        lv_bar_set_value(obj, (uint16_t)rel, LV_ANIM_OFF);
    }
}

// TODO dynamically get max DPI, instead of using hardcoded values
void update_mod_dpi_number(lv_obj_t *obj) {
    if (dilemma_lcd_status.dpi != dilemma_lcd_status_prev.dpi) {
        char c_dpi[50];
        sprintf(c_dpi, "%u", (uint16_t)dilemma_lcd_status.dpi);
        lv_label_set_text(obj, c_dpi);
    }
}

// TODO dynamically get max DPI, instead of using hardcoded values
void update_mod_dpi_bar(lv_obj_t *obj) {
    if (dilemma_lcd_status.dpi != dilemma_lcd_status_prev.dpi) {
        static const uint16_t rel_max_dpi = 200 * 16;
        const float           rel         = (float)((dilemma_lcd_status.dpi + 200 - 400)) * 100 / rel_max_dpi;
        lv_bar_set_value(obj, (uint16_t)rel, LV_ANIM_OFF);
    }
}

bool process_record_lcd(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LCD_MODULE_CHANGE_THEME:
            if (record->event.pressed) {
                if (is_keyboard_master()) {
                    cycle_theme_and_save_in_eeprom();
                    // if the keyboard is left, then we directly update the styles
                    // if the keyboard is right, we need to send the sync info over to the left side
                    // that will be done in housekeeping
                    if (is_keyboard_left()) {
                        update_styles_from_current_theme();
                    }
                    // TODO this is done in cycle_theme_and_save_in_eeprom, we can remove it
                    dilemma_lcd_status.current_theme_id = get_current_theme_id();
                }
            }
            break;
    }
    return true;
}

const char *rgb_matrix_get_effect_name(void) {
    // thank you drashna!
    static char    buf[32]     = {0};
    static uint8_t last_effect = 0;
    if (last_effect != rgb_matrix_get_mode()) {
        last_effect = rgb_matrix_get_mode();
        snprintf(buf, sizeof(buf), "%s", rgb_matrix_get_mode_name(rgb_matrix_get_mode()));
        for (uint8_t i = 1; i < sizeof(buf); ++i) {
            if (buf[i] == 0)
                break;
            else if (buf[i] == '_')
                buf[i] = ' ';
            else if (buf[i - 1] == ' ')
                buf[i] = toupper(buf[i]);
            else if (buf[i - 1] != ' ')
                buf[i] = tolower(buf[i]);
        }
    }
    return buf;
}

/*
called by right side, executed by left side (where the screen is)
we do not store the updated config in eeprom, this is done by master in cycle_theme
if later we would like to do that, first we need to sync halves in the dilemma code with kb eeprom, and then implement
theme sync here with user eeprom
*/
void mouse_info_sync_handler(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    if (is_keyboard_left()) {
        if (initiator2target_buffer_size == sizeof(dilemma_lcd_status)) {
            dilemma_lcd_status_prev = dilemma_lcd_status;
            dilemma_lcd_status      = *(const dilemma_status_t *)initiator2target_buffer;

            if (dilemma_lcd_status_prev.current_theme_id != dilemma_lcd_status.current_theme_id) {
                set_current_theme_id(dilemma_lcd_status.current_theme_id);
                update_styles_from_current_theme();
            }
            refresh_lcd_info();
        }
    }
}