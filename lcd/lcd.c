#include QMK_KEYBOARD_H
#include "introspection.h"

#include "lcd.h"
#include "config.h"
#include "quantum.h"
#include <stdio.h>

#include "qp.h"
#include "qp_comms.h"
#include "qp_st77xx_opcodes.h"
#include "qp_surface.h"
#include "color.h"
#include "transactions.h"

lv_obj_t *ui_screen_base;

/* mod button pairs: GUI, ALT, CTRL, SHIFT */
struct mod_button_pair_t {
    lv_obj_t *button;
    lv_obj_t *label;
    uint8_t   mod_mask;
};

static mod_button_pair_t mod_buttons[4];
static mod_button_pair_t mouse_buttons[2];

lv_obj_t *ui_label_layer;
lv_obj_t *ui_button_layer;
lv_obj_t *ui_label_dpi;
lv_obj_t *ui_label_dpi_number;
lv_obj_t *ui_bar_dpi;
lv_obj_t *ui_label_s_dpi;
lv_obj_t *ui_bar_s_dpi;
lv_obj_t *ui_label_s_dpi_number;
lv_obj_t *ui_image_scroll;
lv_obj_t *ui_label_rgb;
lv_obj_t *ui_bar_rgb;
lv_obj_t *ui_label_rgb_effect;
lv_obj_t *ui_label_rgb_number;

lv_obj_t *ui_line_1;
lv_obj_t *ui_line_2;

typedef struct {
    lv_style_t mod_btn;
    lv_style_t bar;
    lv_style_t bar_background;
    lv_style_t mod_btn_pressed;
    lv_style_t flex_container;
    lv_style_t line;
    lv_style_t layer_name;
    lv_style_t line_background;
    lv_style_t secondary_labels;
} ui_styles_t;

static ui_styles_t ui_styles = {0};

enum ui_user_events {
    EVENT_LAYER_CHANGE = 0,
    EVENT_MOD_CHANGE,
    EVENT_LAST_EVENT,
};

// todo define bits
typedef struct {
    uint8_t  mods;
    bool     sniping;
    bool     scrolling;
    uint8_t  rgb_enabled;
    uint8_t  rgb_effect_mode;
    uint16_t rgb_val;
    uint16_t dpi;
    uint16_t s_dpi;
    uint8_t  layer;
} dilemma_status_t;

static dilemma_status_t g_dilemma_status_prev = {0};
static dilemma_status_t g_dilemma_status      = {0};

const char *ui_layer_strings[] = {"BASE", "FUNCTION", "NAV", "MED/RGB", "POINTER", "NUM", "SYM"};

// themes
extern ui_theme default_theme;
extern ui_theme skeu_dark_theme;
extern ui_theme terminal_theme;
uint8_t         current_theme = 0;
ui_theme       *themes[]      = {&default_theme, &skeu_dark_theme, &terminal_theme};

painter_device_t        lcd;
static painter_device_t surface;
// Buffer required for a 240x280 16bpp surface:
static uint8_t surface_buffer[SURFACE_REQUIRED_BUFFER_BYTE_SIZE(LCD_WIDTH, LCD_HEIGHT, 16)];

lv_obj_t *ui_create_secondary_text(lv_obj_t *cont, const char *text, bool new_track, uint8_t flex) {
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, text);
    lv_obj_add_style(lbl, &ui_styles.secondary_labels, 0);
    if (new_track) {
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }
    lv_obj_set_flex_grow(lbl, flex);
    return lbl;
}

ui_theme get_current_theme(void) {
    return *themes[current_theme];
}

lv_obj_t *ui_create_progress_bar(lv_obj_t *cont, uint8_t flex) {
    lv_obj_t *bar = lv_bar_create(cont);
    lv_obj_set_height(bar, get_current_theme().bar.height);
    lv_obj_add_style(bar, &ui_styles.bar, LV_PART_INDICATOR);
    lv_obj_add_style(bar, &ui_styles.bar_background, 0);
    lv_obj_set_flex_grow(bar, flex);
    return bar;
}

lv_obj_t *ui_create_number_label(lv_obj_t *cont, uint8_t flex) {
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "1234");
    lv_obj_add_style(lbl, &ui_styles.secondary_labels, 0);
    lv_obj_set_flex_grow(lbl, flex);
    return lbl;
}

lv_obj_t *ui_create_line_separator(lv_obj_t *cont, uint8_t flex, uint8_t height) {
    lv_obj_t *bar = lv_bar_create(cont);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_flex_grow(bar, flex);
    lv_obj_set_height(bar, height);
    lv_obj_add_style(bar, &ui_styles.line, LV_PART_INDICATOR);
    lv_obj_add_style(bar, &ui_styles.line_background, 0);
    return bar;
}

void keyboard_post_init_lcd(void) {
    if (is_keyboard_left()) {
        // Display timeout
        wait_ms(LCD_WAIT_TIME);

        lcd = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_SPI_DIVISOR, SPI_MODE);
        qp_init(lcd, LCD_ROTATION);

        surface = qp_make_rgb565_surface(LCD_WIDTH, LCD_HEIGHT, surface_buffer);
        qp_init(surface, LCD_ROTATION);

        // Display offset
        qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);

        // if(qp_lvgl_attach(lcd)){
        // TODO is this done automagically? add defines?
        //    keyboard_post_init_lcd();
        // }
        qp_lvgl_attach(lcd);

        // Power on display, fill with black
        qp_power(lcd, 1);
        qp_rect(lcd, 0, 0, 300, 300, HSV_BLACK, 1);
        qp_flush(lcd);
        ui_screen_base = lv_obj_create(NULL);

        init_themes();
        style_init_all();

        lv_obj_t *cont = lv_obj_create(ui_screen_base);
        lv_obj_set_size(cont, LCD_WIDTH, LCD_HEIGHT);
        lv_obj_center(cont);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_add_style(cont, &ui_styles.flex_container, 0);

        ui_button_layer = lv_btn_create(cont);
        lv_obj_add_style(ui_button_layer, &ui_styles.layer_name, 0);
        lv_obj_set_height(ui_button_layer, 33);
        lv_obj_set_flex_grow(ui_button_layer, 1); // take all remaining space in line

        // layer title
        ui_label_layer = lv_label_create(ui_button_layer);
        ui_init_layer_name(ui_label_layer);
        lv_label_set_text(ui_label_layer, "LAYER: BASE");
        lv_obj_center(ui_label_layer);

        // mod buttons: SHIFT, ALT, CTRL, GUI
        mod_buttons[0] = ui_create_mod_button(cont, "SHFT", true, MOD_MASK_SHIFT);
        mod_buttons[1] = ui_create_mod_button(cont, "ALT", false, MOD_MASK_ALT);
        mod_buttons[2] = ui_create_mod_button(cont, "CTRL", false, MOD_MASK_CTRL);
        mod_buttons[3] = ui_create_mod_button(cont, "GUI", false, MOD_MASK_GUI);

        // line separator
        ui_line_1 = ui_create_line_separator(cont, 1, 3);

        // display base layer screen upon init
        lv_disp_load_scr(ui_screen_base);

        // mouse special buttons
        mouse_buttons[0] = ui_create_mod_button(cont, "SNIPE", true, 0);
        mouse_buttons[1] = ui_create_mod_button(cont, "SCROLL", false, 0);

        // sniping DPI widgets
        ui_label_s_dpi        = ui_create_secondary_text(cont, "SNIPE DPI", true, 4);
        ui_bar_s_dpi          = ui_create_progress_bar(cont, 4);
        ui_label_s_dpi_number = ui_create_number_label(cont, 2);

        // regular DPI widgets
        ui_label_dpi        = ui_create_secondary_text(cont, "DPI", true, 2);
        ui_bar_dpi          = ui_create_progress_bar(cont, 6);
        ui_label_dpi_number = ui_create_number_label(cont, 2);

        // line separator
        ui_line_2 = ui_create_line_separator(cont, 1, 3);

        // rgb widgets
        ui_label_rgb        = ui_create_secondary_text(cont, "RGB", true, 2);
        ui_bar_rgb          = ui_create_progress_bar(cont, 6);
        ui_label_rgb_number = ui_create_number_label(cont, 2);

        ui_label_rgb_effect = ui_create_secondary_text(cont, "effect...", true, 1);

        // theme and backgrounds
        lv_disp_t  *dispp = lv_disp_get_default();
        lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(BK_PALETTE), lv_palette_main(BK_PALETTE), true, LV_FONT_DEFAULT);
        lv_disp_set_theme(dispp, theme);
        lv_obj_set_style_bg_color(cont, lv_color_black(), LV_PART_MAIN);

        // sync mouse data across halves
        transaction_register_rpc(RPC_ID_MOUSE_SYNC, mouse_info_sync_handler);
    }
}

void update_styles(ui_theme theme) {
    apply_theme_btn(&(ui_styles.mod_btn), theme.btn_normal);
    apply_theme_btn(&(ui_styles.mod_btn_pressed), theme.btn_pressed);
    apply_theme_layer_name(&(ui_styles.layer_name), theme.layer_name);
    apply_theme_secondary_label(&(ui_styles.secondary_labels), theme.secondary_labels);
    apply_theme_bar(&(ui_styles.bar), theme.bar);
    apply_theme_bar_background(&(ui_styles.bar_background), theme.bar_background);
}

void style_init_all(void) {
    // mod button
    lv_style_init(&ui_styles.mod_btn);

    // pressed mod indicator
    lv_style_init(&ui_styles.mod_btn_pressed);

    // layer name label
    lv_style_init(&ui_styles.layer_name);

    // separator line
    lv_style_set_radius(&ui_styles.line, 0);
    lv_style_set_bg_color(&ui_styles.line, lv_color_make(50, 55, 67));
    lv_style_set_radius(&ui_styles.line_background, 3);
    lv_style_set_border_width(&ui_styles.line_background, 0);
    lv_style_set_bg_color(&ui_styles.line_background, lv_color_make(50, 55, 67));

    // secondary labels
    lv_style_init(&ui_styles.secondary_labels);

    // bars
    // TODO move this out? this is an apply theme...
    apply_theme_bar(&(ui_styles.bar), get_current_theme().bar);
    apply_theme_bar_background(&(ui_styles.bar_background), get_current_theme().bar_background);

    // flex container
    lv_style_set_bg_color(&ui_styles.flex_container, lv_color_black());
    lv_style_set_pad_row(&ui_styles.flex_container, 10);
    lv_style_set_border_width(&ui_styles.flex_container, 0);
    // lv_style_set_pad_all(&ui_styles.flex_container, 0);

    update_styles(get_current_theme());
}

void ui_init_layer_name(lv_obj_t *label) {
    lv_obj_remove_style_all(label);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 20);
}

mod_button_pair_t ui_create_mod_button(lv_obj_t *cont, const char *text, bool force_new_track, uint8_t mod_mask) {
    mod_button_pair_t b = {0};

    b.mod_mask = mod_mask;
    b.button   = lv_btn_create(cont);
    ui_init_button_mod_indicator(b.button);

    if (force_new_track) {
        lv_obj_add_flag(b.button, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }
    b.label = lv_label_create(b.button);
    lv_label_set_text(b.label, text);
    lv_obj_center(b.label);

    return b;
}

// TODO colors....
void update_theme_color(void) {
    // if (g_dilemma_status.layer != g_dilemma_status_prev.layer) {
    //     HSV hsv;
    //     switch (g_dilemma_status.layer) {
    //         case 0:
    //         default:
    //             hsv.h = 218;
    //             hsv.s = 70;
    //             hsv.v = 93;
    //             break;
    //         case 1:
    //             hsv.h = 250;
    //             hsv.s = 100;
    //             hsv.v = 80;
    //             break;
    //         case 2:
    //             hsv.h = 35;
    //             hsv.s = 100;
    //             hsv.v = 80;
    //             break;
    //         case 3:
    //             hsv.h = 195;
    //             hsv.s = 30;
    //             hsv.v = 80;
    //             break;
    //     }

    //     lv_style_set_bg_color(&ui_styles.mod_btn_pressed, lv_color_hsv_to_rgb(hsv.h, hsv.s, hsv.v));
    //     lv_obj_report_style_change(&ui_styles.mod_btn_pressed);
    //     lv_style_set_bg_color(&ui_styles.bar, lv_color_hsv_to_rgb(hsv.h, hsv.s, hsv.v));
    //     lv_obj_report_style_change(&ui_styles.bar);
    // }
}

void ui_init_button_mod_indicator(lv_obj_t *button) {
    lv_obj_add_style(button, &ui_styles.mod_btn, 0);
    lv_obj_add_style(button, &ui_styles.mod_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, event_screen_base_update_mods, LV_EVENT_ALL, NULL);
    lv_obj_set_height(button, 33);
    lv_obj_set_flex_grow(button, 1);
}

void event_screen_pointer_sniping_toggle(lv_event_t *e) {}
void event_screen_pointer_scroll_toggle(lv_event_t *e) {}

// TODO what is this?
void event_screen_base_update_mods(lv_event_t *e) {}

void refresh_lcd_info(void) {
    update_layer_name();
    update_mods();
    update_rgb_info();
    update_mouse_info();
    update_theme_color();
}

void housekeeping_task_lcd(void) {
    if (is_keyboard_master()) {
        if (is_keyboard_left()) {
            // no need to sync
            update_dilemma_status();
            refresh_lcd_info();
            g_dilemma_status_prev = g_dilemma_status;
        } else {
            if (is_keyboard_master()) {
                // update the dilemma status, and let the left half handle the LCD update
                bool            needs_sync = false;
                static uint32_t last_sync  = 0;
                update_dilemma_status();
                // // Check if the state values are different.
                if (memcmp(&g_dilemma_status, &g_dilemma_status_prev, sizeof(g_dilemma_status))) {
                    needs_sync            = true;
                    g_dilemma_status_prev = g_dilemma_status;
                }
                // Send to slave every 500ms regardless of state change.
                if (timer_elapsed32(last_sync) > 500) {
                    needs_sync = true;
                }
                // Perform the sync if requested.
                if (needs_sync) {
                    if (transaction_rpc_send(RPC_ID_MOUSE_SYNC, sizeof(g_dilemma_status), &g_dilemma_status)) {
                        last_sync = timer_read32();
                    }
                }
                g_dilemma_status_prev = g_dilemma_status;
            }
        }
    }
}

void update_layer_name(void) {
    if (g_dilemma_status.layer != g_dilemma_status_prev.layer) {
        switch (g_dilemma_status.layer) {
            case 0:
            default:
                lv_label_set_text(ui_label_layer, "LAYER: BASE");
                break;
            case 1:
                lv_label_set_text(ui_label_layer, "LAYER: LOWER");
                break;
            case 2:
                lv_label_set_text(ui_label_layer, "LAYER: RAISE");
                break;
            case 3:
                lv_label_set_text(ui_label_layer, "LAYER: MOUSE");
                break;
        }
    }
}

void update_dilemma_status(void) {
    g_dilemma_status.mods            = get_mods();
    g_dilemma_status.layer           = get_highest_layer(layer_state);
    g_dilemma_status.sniping         = dilemma_get_pointer_sniping_enabled();
    g_dilemma_status.dpi             = dilemma_get_pointer_default_dpi();
    g_dilemma_status.s_dpi           = dilemma_get_pointer_sniping_dpi();
    g_dilemma_status.scrolling       = dilemma_get_pointer_dragscroll_enabled();
    g_dilemma_status.rgb_enabled     = rgb_matrix_is_enabled();
    g_dilemma_status.rgb_effect_mode = rgb_matrix_get_mode();
    g_dilemma_status.rgb_val         = rgb_matrix_get_val();
}

void update_mods(void) {
    int i = 0;
    for (i = 0; i < 4; i++) {
        if ((g_dilemma_status.mods & mod_buttons[i].mod_mask) != (g_dilemma_status_prev.mods & mod_buttons[i].mod_mask)) {
            if ((g_dilemma_status.mods & mod_buttons[i].mod_mask)) {
                lv_event_send(mod_buttons[i].button, LV_EVENT_PRESSED, NULL);
            } else {
                lv_event_send(mod_buttons[i].button, LV_EVENT_RELEASED, NULL);
            }
        }
    }
}

void update_rgb_info(void) {
    const bool rgb_change = (g_dilemma_status.rgb_enabled != g_dilemma_status_prev.rgb_enabled);

    if (!g_dilemma_status.rgb_enabled) {
        if (rgb_change) {
            lv_label_set_text(ui_label_rgb_number, "Off");
            lv_bar_set_value(ui_bar_rgb, 0, LV_ANIM_OFF);
            lv_label_set_text(ui_label_rgb_effect, "");
        }
    } else {
        if ((rgb_change) || (g_dilemma_status.rgb_val != g_dilemma_status_prev.rgb_val)) {
            char rgbval[50];
            sprintf(rgbval, "%u", g_dilemma_status.rgb_val);
            lv_label_set_text(ui_label_rgb_number, rgbval);
            float rel = (float)(g_dilemma_status.rgb_val) * 100 / 156;
            lv_bar_set_value(ui_bar_rgb, (uint16_t)rel, LV_ANIM_OFF);
        }
        if ((rgb_change) || (g_dilemma_status.rgb_effect_mode != g_dilemma_status_prev.rgb_effect_mode)) {
            const char *effect_name = rgb_matrix_get_effect_name();
            lv_label_set_text(ui_label_rgb_effect, effect_name);
        }
    }
}

void update_mouse_info(void) {
    // TODO dynamically get max DPI, instead of using hardcoded values
    if (g_dilemma_status.dpi != g_dilemma_status_prev.dpi) {
        static const uint16_t rel_max_dpi = 200 * 16;
        const float           rel         = (float)((g_dilemma_status.dpi + 200 - 400)) * 100 / rel_max_dpi;
        lv_bar_set_value(ui_bar_dpi, (uint16_t)rel, LV_ANIM_OFF);

        char c_dpi[50];
        sprintf(c_dpi, "%u", (uint16_t)g_dilemma_status.dpi);
        lv_label_set_text(ui_label_dpi_number, c_dpi);
    }

    if (g_dilemma_status.s_dpi != g_dilemma_status_prev.s_dpi) {
        char                  c_s_dpi[50];
        static const uint16_t rel_max_s_dpi = 100 * 4;
        const float           rel           = (float)((g_dilemma_status.s_dpi + 100 - 200)) * 100 / rel_max_s_dpi;
        lv_bar_set_value(ui_bar_s_dpi, (uint16_t)rel, LV_ANIM_OFF);
        sprintf(c_s_dpi, "%u", (uint16_t)g_dilemma_status.s_dpi);
        lv_label_set_text(ui_label_s_dpi_number, c_s_dpi);
    }

    if (g_dilemma_status.sniping != g_dilemma_status_prev.sniping) {
        if (g_dilemma_status.sniping) {
            lv_event_send(mouse_buttons[0].button, LV_EVENT_PRESSED, NULL);
        } else {
            lv_event_send(mouse_buttons[0].button, LV_EVENT_RELEASED, NULL);
        }
    }

    if (g_dilemma_status.scrolling != g_dilemma_status_prev.scrolling) {
        if (g_dilemma_status.scrolling) {
            lv_event_send(mouse_buttons[1].button, LV_EVENT_PRESSED, NULL);
        } else {
            lv_event_send(mouse_buttons[1].button, LV_EVENT_RELEASED, NULL);
        }
    }
}

bool process_record_lcd(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LCD_MODULE_CHANGE_THEME:
            if (record->event.pressed) {
                current_theme = (current_theme + 1) % (sizeof(themes) / sizeof(ui_theme *));
                update_styles(get_current_theme());
                housekeeping_task_lcd();
                qp_flush(lcd);
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

// called by primary, executed by secondary
void mouse_info_sync_handler(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer) {
    if (initiator2target_buffer_size == sizeof(g_dilemma_status)) {
        g_dilemma_status_prev = g_dilemma_status;
        g_dilemma_status      = *(const dilemma_status_t *)initiator2target_buffer;
        refresh_lcd_info();
    }
}