#include QMK_KEYBOARD_H
#include "introspection.h"
#include "lvgl.h"
#include <ctype.h>
#include "theme.h"

typedef struct mod_button_pair_t mod_button_pair_t;

void keyboard_post_init_lcd(void);
void style_init_all(void);

void              ui_init_layer_name(lv_obj_t *label);
void              housekeeping_task_lcd(void);
void              update_mods(void);
void              update_mouse_info(void);
void              update_rgb_info(void);
void              update_rgb_info(void);
void              update_layer_name(void);
void              ui_init_button_mod_indicator(lv_obj_t *label);
mod_button_pair_t ui_create_mod_button(lv_obj_t *cont, const char *text, bool force_new_track, uint8_t mod_mask);

void update_theme_color(void);
void update_dilemma_status(void);

const char *rgb_matrix_get_effect_name(void);

lv_obj_t *ui_create_secondary_text(lv_obj_t *cont, const char *text, bool new_track, uint8_t flex);
lv_obj_t *ui_create_progress_bar(lv_obj_t *cont, uint8_t flex);
lv_obj_t *ui_create_number_label(lv_obj_t *cont, uint8_t flex);
lv_obj_t *ui_create_line_separator(lv_obj_t *cont, uint8_t flex, uint8_t height);
void      update_styles(ui_theme theme);
bool process_record_lcd(uint16_t keycode, keyrecord_t *record);
ui_theme get_current_theme(void);

void mouse_info_sync_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data);
void refresh_lcd_info(void);
void cycle_theme(void);
void init_display(void);