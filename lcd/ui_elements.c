#include QMK_KEYBOARD_H
#include "ui_elements.h"
#include "lvgl.h"
#include "lcd.h"

lv_obj_t *ui_create_progress_bar(lv_obj_t *cont, uint8_t flex) {
    lv_obj_t *bar = lv_bar_create(cont);
    // lv_obj_set_height(bar, get_current_theme().bar.height);
    lv_obj_add_style(bar, &current_style.bar, LV_PART_INDICATOR);
    lv_obj_add_style(bar, &current_style.bar_background, 0);
    lv_obj_set_flex_grow(bar, flex);
    return bar;
}

lv_obj_t *ui_create_number_label(lv_obj_t *cont, uint8_t flex) {
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "1234");
    lv_obj_add_style(lbl, &current_style.value_labels, 0);
    lv_obj_set_flex_grow(lbl, flex);
    return lbl;
}

lv_obj_t *ui_create_layer_label(lv_obj_t *cont){
    lv_obj_t *ui_button_layer = lv_btn_create(cont);
    lv_obj_add_style(ui_button_layer, &current_style.layer_name, 0);
    lv_obj_set_flex_grow(ui_button_layer, 1); // take all remaining space in line

    lv_obj_t *ui_label_layer = lv_label_create(ui_button_layer);
    // TODO, this should not be here
    lv_label_set_text(ui_label_layer, "LAYER: BASE");
    lv_obj_center(ui_label_layer);
    lv_obj_set_width(ui_label_layer, LV_SIZE_CONTENT);
    // lv_obj_set_x(ui_label_layer, 0);
    // lv_obj_set_y(ui_label_layer, 20);

    return ui_label_layer;
}

lv_obj_t *ui_create_line_separator(lv_obj_t *cont, uint8_t flex, uint8_t height) {
    lv_obj_t *bar = lv_bar_create(cont);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_flex_grow(bar, flex);
    lv_obj_set_height(bar, height);
    lv_obj_add_style(bar, &current_style.line, LV_PART_INDICATOR);
    lv_obj_add_style(bar, &current_style.line_background, 0);
    return bar;
}

// todo get rid of mod_button_pair_t structure
lv_obj_t *ui_create_mod_button(lv_obj_t *cont, const char *text, bool force_new_track, uint8_t mod_mask) {
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

    return b.button;
}

void ui_init_button_mod_indicator(lv_obj_t *button) {
    lv_obj_add_style(button, &current_style.mod_btn, 0);
    lv_obj_add_style(button, &current_style.mod_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_height(button, 33);
    lv_obj_set_flex_grow(button, 1);
}

lv_obj_t *ui_create_secondary_text(lv_obj_t *cont, const char *text, bool new_track, uint8_t flex) {
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, text);
    lv_obj_add_style(lbl, &current_style.secondary_labels, 0);
    if (new_track) {
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }
    lv_obj_set_flex_grow(lbl, flex);
    return lbl;
}