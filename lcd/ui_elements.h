#include "lvgl.h"

/* mod button pairs: GUI, ALT, CTRL, SHIFT */
// TODO get rid of this structure
typedef struct {
    lv_obj_t *button;
    lv_obj_t *label;
    uint8_t   mod_mask;
} mod_button_pair_t;

void              ui_init_button_mod_indicator(lv_obj_t *label);
lv_obj_t* ui_create_mod_button(lv_obj_t *cont, const char *text, bool force_new_track, uint8_t mod_mask);
lv_obj_t         *ui_create_secondary_text(lv_obj_t *cont, const char *text, bool new_track, uint8_t flex);
lv_obj_t         *ui_create_progress_bar(lv_obj_t *cont, uint8_t flex);
lv_obj_t         *ui_create_number_label(lv_obj_t *cont, uint8_t flex);
lv_obj_t         *ui_create_line_separator(lv_obj_t *cont, uint8_t flex, uint8_t height);
lv_obj_t *ui_create_layer_label(lv_obj_t *cont);