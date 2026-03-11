#include "theme.h"

LV_FONT_DECLARE(montserratbold14);
LV_FONT_DECLARE(montserratbold13);
LV_FONT_DECLARE(dmsans13);
LV_FONT_DECLARE(dmsans14);
LV_FONT_DECLARE(dmsans14bold);
LV_FONT_DECLARE(jetbrainsmono13);
LV_FONT_DECLARE(jetbrainsmono14bold);

ui_theme default_theme;
ui_theme skeu_dark_theme;
ui_theme terminal_theme;

void init_themes(void) {
    default_theme = (ui_theme){
        .btn_normal =
            {
                .font          = &montserratbold14,
                .border_radius = 6,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(23, 26, 31),
                .border_color  = lv_color_make(50, 55, 67),
                .text_color    = lv_color_white(),
                .border_width  = 2,
                .text_opa      = LV_OPA_COVER,
            },
        .btn_pressed =
            {
                .font          = &montserratbold14,
                .border_radius = 6,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(71, 133, 239),
                .border_color  = lv_color_white(),
                .text_color    = lv_color_white(),
                .border_width  = 2,
                .text_opa      = LV_OPA_COVER,
            },
        .layer_name =
            {
                .font          = &montserratbold14,
                .border_radius = 6,
                .bg_color      = lv_color_black(),
                .border_color  = lv_color_make(50, 55, 67),
                .text_color    = lv_color_white(),
                .border_width  = 2,
            },
        .secondary_labels =
            {
                .font = &montserratbold13,
                .text_color    = lv_color_make(150, 150, 150),
            },
        .bar =
            {
                .border_radius = 0,
                .bg_color      = lv_color_make(71, 133, 239),
                .height        = 6,
                .bg_opacity    = LV_OPA_COVER,
            },
        .bar_background =
            {
                .border_radius = 3,
                .border_width  = 0,
            },
    };
    skeu_dark_theme = (ui_theme){
        .btn_normal =
            {
                .font          = &dmsans14bold,
                .border_radius = 8,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(38, 38, 38),
                .border_color  = lv_color_make(15, 15, 15),
                .text_color    = lv_color_make(150, 150, 150),
                .border_width  = 1,
                .bg_grad_color = lv_color_make(23, 26, 31),
                .bg_grad_dir   = LV_GRAD_DIR_VER,
                .text_opa      = LV_OPA_COVER,
                .shadow_color  = lv_color_black(),
                .shadow_width  = 0,
                .shadow_ofs_x  = 0,
                .shadow_ofs_y  = 0,
                .shadow_spread = 0,
                .outline_color = lv_color_black(),
                .outline_width = 0,
                .outline_opa   = LV_OPA_TRANSP,
                .outline_pad   = 0,
            },
        .btn_pressed =
            {
                .font          = &dmsans14bold,
                .border_radius = 8,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(29, 29, 29),
                .border_color  = lv_color_make(8, 8, 8),
                .text_color    = lv_color_make(232, 152, 10),
                .border_width  = 1,
                .bg_grad_color = lv_color_make(30, 22, 0),
                .bg_grad_dir   = LV_GRAD_DIR_VER,
                .text_opa      = LV_OPA_COVER,
                .shadow_color  = lv_color_make(200, 130, 0),
                .shadow_width  = 20,
                .shadow_ofs_x  = 0,
                .shadow_ofs_y  = 0,
                .shadow_spread = 0,
                .outline_color = lv_color_make(200, 130, 0),
                .outline_width = 1,
                .outline_opa   = LV_OPA_20,
                .outline_pad   = 4,
            },
        .layer_name =
            {
                .font          = &dmsans14bold,
                .border_radius = 6,
                .bg_color      = lv_color_make(26, 26, 26),
                .border_color  = lv_color_make(12, 12, 12),
                .text_color    = lv_color_make(166, 166, 166),
                .border_width  = 1,
            },
        .secondary_labels =
            {
                .font       = &dmsans13,
                .text_color    = lv_color_make(150, 150, 150),
            },
        .bar =
            {
                .border_radius = 3,
                .bg_color      = lv_color_make(232, 152, 10),
                .height        = 4,
                .bg_opacity    = LV_OPA_COVER,
            },
        .bar_background =
            {
                .border_radius = 3,
                .border_width  = 0,
            },
    };
    terminal_theme = (ui_theme){
        // Green phosphor CRT — inactive: near-black bg, dim green border/text
        .btn_normal =
            {
                .font          = &jetbrainsmono14bold,
                .border_radius = 0,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(5, 14, 5),
                .border_color  = lv_color_make(26, 58, 26),
                .text_color    = lv_color_make(42, 106, 42),
                .border_width  = 1,
                .bg_grad_color = lv_color_make(5, 14, 5),
                .bg_grad_dir   = LV_GRAD_DIR_NONE,
                .text_opa      = LV_OPA_COVER,
                .shadow_color  = lv_color_black(),
                .shadow_width  = 0,
                .shadow_ofs_x  = 0,
                .shadow_ofs_y  = 0,
                .shadow_spread = 0,
                .outline_color = lv_color_black(),
                .outline_width = 0,
                .outline_opa   = LV_OPA_TRANSP,
                .outline_pad   = 0,
            },
        // Active/pressed: solid phosphor green fill, dark text — like .term-mode-btn.on
        .btn_pressed =
            {
                .font          = &jetbrainsmono14bold,
                .border_radius = 0,
                .bg_opacity    = LV_OPA_COVER,
                .bg_color      = lv_color_make(0, 255, 65),
                .border_color  = lv_color_make(0, 255, 65),
                .text_color    = lv_color_make(5, 14, 5),
                .border_width  = 1,
                .bg_grad_color = lv_color_make(0, 255, 65),
                .bg_grad_dir   = LV_GRAD_DIR_NONE,
                .text_opa      = LV_OPA_COVER,
                .shadow_color  = lv_color_make(0, 255, 65),
                .shadow_width  = 10,
                .shadow_ofs_x  = 0,
                .shadow_ofs_y  = 0,
                .shadow_spread = 0,
                .outline_color = lv_color_make(0, 255, 65),
                .outline_width = 1,
                .outline_opa   = LV_OPA_20,
                .outline_pad   = 3,
            },
        // Layer name: bright phosphor green text on near-black bg
        .layer_name =
            {
                .font          = &jetbrainsmono14bold,
                .border_radius = 0,
                .bg_color      = lv_color_make(5, 14, 5),
                .border_color  = lv_color_make(26, 58, 26),
                .text_color    = lv_color_make(0, 255, 65),
                .border_width  = 1,
            },
        // Secondary labels: dim green, like .term-key / .term-layer-sub
        .secondary_labels =
            {
                .font       = &jetbrainsmono13,
                .text_color = lv_color_make(42, 106, 42),
            },
        // Bar fill: bright phosphor green, sharp corners
        .bar =
            {
                .border_radius = 0,
                .bg_color      = lv_color_make(0, 255, 65),
                .height        = 4,
                .bg_opacity    = LV_OPA_COVER,
            },
        // Bar background: dark green trough, no border radius
        .bar_background =
            {
                .border_radius = 0,
                .border_width  = 1,
            },
    };
}

void apply_theme_btn(lv_style_t *style, ui_theme_mod_btn theme) {
    lv_style_init(style);
    lv_style_set_text_font(style, theme.font);
    lv_style_set_radius(style, theme.border_radius);
    lv_style_set_bg_opa(style, theme.bg_opacity);
    lv_style_set_bg_color(style, theme.bg_color);
    lv_style_set_border_color(style, theme.border_color);
    lv_style_set_border_width(style, theme.border_width);
    lv_style_set_text_color(style, theme.text_color);

    lv_style_set_bg_grad_color(style, theme.bg_grad_color);
    lv_style_set_bg_grad_dir(style, theme.bg_grad_dir);
    lv_style_set_text_opa(style, theme.text_opa);
    lv_style_set_shadow_color(style, theme.shadow_color);
    lv_style_set_shadow_width(style, theme.shadow_width);
    lv_style_set_shadow_ofs_x(style, theme.shadow_ofs_x);
    lv_style_set_shadow_ofs_y(style, theme.shadow_ofs_y);
    lv_style_set_shadow_spread(style, theme.shadow_spread);
    lv_style_set_outline_color(style, theme.outline_color);
    lv_style_set_outline_width(style, theme.outline_width);
    lv_style_set_outline_opa(style, theme.outline_opa);
    lv_style_set_outline_pad(style, theme.outline_pad);
    lv_obj_report_style_change(style);
}

void apply_theme_layer_name(lv_style_t *style, ui_theme_layer_name theme) {
    lv_style_init(style);
    lv_style_set_text_font(style, theme.font);
    lv_style_set_radius(style, theme.border_radius);
    lv_style_set_bg_color(style, theme.bg_color);
    lv_style_set_border_color(style, theme.border_color);
    lv_style_set_border_width(style, theme.border_width);
    lv_style_set_text_color(style, theme.text_color);
    lv_obj_report_style_change(style);
}

void apply_theme_secondary_label(lv_style_t *style, ui_theme_secondary_label theme) {
    lv_style_init(style);
    lv_style_set_text_font(style, theme.font);
    lv_style_set_text_color(style, theme.text_color);
    lv_obj_report_style_change(style);
}

void apply_theme_bar(lv_style_t *style, ui_theme_bar theme) {
    lv_style_init(style);
    lv_style_set_radius(style, theme.border_radius);
    lv_style_set_bg_color(style, theme.bg_color);
    lv_style_set_bg_opa(style, theme.bg_opacity);
    lv_style_set_height(style, theme.height);
    lv_obj_report_style_change(style);
}

void apply_theme_bar_background(lv_style_t *style, ui_theme_bar_background theme) {
    lv_style_set_radius(style, theme.border_radius);
    lv_style_set_border_width(style, theme.border_width);
    lv_obj_report_style_change(style);
}