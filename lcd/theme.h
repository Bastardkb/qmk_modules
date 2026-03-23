#include "lvgl.h"

/*
    This structure is used to store the LCD configuration in eeprom.
    At the moment it only stores the theme id.
    Ideally we would sync all the data (rgb, dpi, etc) across both halves - but this needs to be done in dilemma.c directly.
    Meanwhile, we trigger a manual sync when the keyboard starts up.
*/
// TODO remove raw?
// TODO add brightness configuration
typedef union {
    uint8_t raw;
    struct {
        uint8_t current_theme_id;
    } __attribute__((packed));
} dilemma_config_lcd_t;

dilemma_config_lcd_t dilemma_config_lcd;

// TODO once removed current_style from lcd.c, move this into ui_elements.c to make it private to ui_elements 
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
    lv_style_t value_labels;
} ui_styles_t;

ui_styles_t current_style;

void init_themes(void);
void read_dilemma_theme_config_from_eeprom(dilemma_config_lcd_t *config);
void load_dilemma_theme_config_from_eeprom(void);
void write_dilemma_theme_config_to_eeprom(dilemma_config_lcd_t *config);

void     init_styles(void);
void     update_styles_from_current_theme(void);
uint8_t  get_current_theme_id(void);
void set_current_theme_id(uint8_t id);