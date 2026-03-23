#include QMK_KEYBOARD_H
#include "introspection.h"
#include "lvgl.h"
#include <ctype.h>
#include "theme.h"

// todo define bits
typedef struct {
    uint8_t                mods;
    bool                   sniping;
    bool                   scrolling;
    uint8_t                layer;
    uint8_t                current_theme_id;
    uint8_t                rgb_enabled;
    uint8_t                rgb_effect_mode;
    uint16_t               rgb_val;
    uint16_t               dpi;
    uint16_t               s_dpi;
} dilemma_status_t;

typedef struct{
    lv_obj_t *obj;
    void (*update_function)(lv_obj_t*);
} obj_update_t;

typedef struct{
    obj_update_t *array;
    size_t used;
    size_t size;
    int amount_elements;
}obj_event_array_t;

void keyboard_post_init_lcd(void);

void              housekeeping_task_lcd(void);
void update_layer_name(lv_obj_t *obj);
void              update_theme_color(void);

void update_dilemma_status(void);

const char *rgb_matrix_get_effect_name(void);

bool      process_record_lcd(uint16_t keycode, keyrecord_t *record);

void mouse_info_sync_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data);
void refresh_lcd_info(void);
void cycle_theme_and_save_in_eeprom(void);
void init_display(void);
void write_config_to_eeprom(void);
void update_rgb_value(lv_obj_t* obj);
void update_rgb_bar(lv_obj_t* obj);
void update_mod_shift(lv_obj_t *obj);
void update_mod_ctrl(lv_obj_t *obj);
void update_mod_alt(lv_obj_t *obj);
void update_mod_gui(lv_obj_t *obj);
void update_mod_snipe(lv_obj_t *obj);
void update_mod_scroll(lv_obj_t *obj);
void update_mod_snipe_dpi_bar(lv_obj_t *obj);
void update_mod_snipe_dpi_number(lv_obj_t *obj);
void update_mod_dpi_bar(lv_obj_t *obj);
void update_mod_dpi_number(lv_obj_t *obj);
void update_mod_xx(lv_obj_t *obj, uint8_t mod_mask);
void update_rgb_effect(lv_obj_t *obj);
void init_obj_event_array(obj_event_array_t *a);
void add_obj_event_array(obj_event_array_t *a, obj_update_t element);