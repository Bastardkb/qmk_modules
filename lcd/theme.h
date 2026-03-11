#include "lvgl.h"

typedef struct {
    const lv_font_t *font;
    uint8_t          border_radius;
    uint16_t         bg_opacity;
    lv_color_t       bg_color;
    lv_color_t       border_color;
    lv_color_t       text_color;
    uint8_t          border_width;
    lv_color_t       bg_grad_color;
    lv_grad_dir_t    bg_grad_dir;
    lv_color_t       shadow_color;
    int16_t          shadow_width;
    int16_t          shadow_ofs_x;
    int16_t          shadow_ofs_y;
    int16_t          shadow_spread;
    lv_color_t       outline_color;
    lv_coord_t       outline_width;
    lv_opa_t         outline_opa;
    lv_coord_t       outline_pad;
    lv_opa_t         text_opa;
} ui_theme_mod_btn;

typedef struct {
    const lv_font_t *font;
    uint8_t          border_radius;
    lv_color_t       border_color;
    lv_color_t       text_color;
    lv_color_t       bg_color;
    uint8_t          border_width;
} ui_theme_layer_name;

typedef struct {
    const lv_font_t *font;
    lv_color_t       text_color;
} ui_theme_secondary_label;

typedef struct {
    uint8_t    border_radius;
    lv_color_t bg_color;
    uint16_t height;
    uint16_t bg_opacity;
} ui_theme_bar;

typedef struct {
    uint8_t border_radius;
    uint8_t border_width;
} ui_theme_bar_background;

typedef struct {
    ui_theme_mod_btn         btn_normal;
    ui_theme_mod_btn         btn_pressed;
    ui_theme_layer_name      layer_name;
    ui_theme_secondary_label secondary_labels;
    ui_theme_bar             bar;
    ui_theme_bar_background  bar_background;
    // SEPARATOR LINE -- TODO
} ui_theme;

void init_themes(void);
void apply_theme_btn(lv_style_t *btn, ui_theme_mod_btn theme_btn);
void apply_theme_layer_name(lv_style_t *style, ui_theme_layer_name theme);
void apply_theme_secondary_label(lv_style_t *style, ui_theme_secondary_label theme);
void apply_theme_bar(lv_style_t *style, ui_theme_bar theme);
void apply_theme_bar_background(lv_style_t *style, ui_theme_bar_background theme);