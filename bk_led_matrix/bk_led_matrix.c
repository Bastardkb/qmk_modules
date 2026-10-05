// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix.h"
#include "led_matrix_duck.h"
#include "led_matrix_layers.h"
#include "led_matrix_mods.h"
#include "led_matrix_pointer.h"
#include "led_matrix_motion.h"
#include "led_matrix_layer_anims.h"
#include "transactions.h"
#include "introspection.h"
#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
#    include "bk_pointing_modes.h"
#endif

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0); // 1.1.0 for pointing_device_task

static uint32_t bklm_last_input_ms;

/* Bumped on every key event and trackball move on the USB half and sent to the
 * panel half, whose own key processing never runs when it isn't the USB half. */
static uint8_t bklm_activity;

/* Any key event resets the idle clock used by housekeeping_task_bk_led_matrix. */
void process_records_bk_led_matrix_note_last_input(uint16_t keycode, keyrecord_t *record) {
    // unused(keycode);
    // unused(record);
    bklm_last_input_ms = timer_read32();
    bklm_activity++;
}

bool process_record_bk_led_matrix(uint16_t keycode, keyrecord_t *record) {
    process_records_bk_led_matrix_note_last_input(keycode, record);
    if (keycode == LED_MATRIX_ANIMATION_NEXT) {
        if (record->event.pressed) {
            bklm_motion_style_set(bklm_motion_cycle_next(bklm_motion_style, get_mods() & MOD_MASK_SHIFT), true);
        }
        return false;
    }
    return true;
}

/* bk_pointing_device keeps its mode on the USB half only, so that half sends it
 * to the panel half, along with activity, trackball motion and the animation. */
static uint8_t bklm_synced_pointer_mode;

typedef struct __attribute__((packed)) {
    uint8_t mode;
    uint8_t activity;
    uint8_t style; /* trackball animation, picked with LED_MATRIX_ANIMATION_NEXT */
    int16_t dx, dy; /* motion since the last delivered message */
} bklm_sync_t;

static int32_t bklm_motion_x, bklm_motion_y; /* USB half: not yet delivered */

/* Only the USB half sees pointer reports. Feed the animation directly when that is
 * also the panel half, otherwise batch the motion for the sync. */
report_mouse_t pointing_device_task_bk_led_matrix(report_mouse_t mouse_report) {
    if (mouse_report.x != 0 || mouse_report.y != 0) {
        bklm_last_input_ms = timer_read32();
        bklm_activity++;
        if (is_keyboard_left() == (LED_MATRIX_MODULE_ON_LEFT != 0)) {
            bklm_motion_feed(mouse_report.x, mouse_report.y);
        } else {
            bklm_motion_x += mouse_report.x;
            bklm_motion_y += mouse_report.y;
        }
    }
    return mouse_report;
}

uint8_t bklm_pointer_mode(void) {
#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
    if (is_keyboard_master()) {
        return bkpd_mode_get_active_id();
    }
#endif
    return bklm_synced_pointer_mode;
}

static void bklm_sync_handler(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    static uint8_t last_activity;
    static bool    first = true;
    if (in_len == sizeof(bklm_sync_t)) {
        const bklm_sync_t *msg   = (const bklm_sync_t *)in_data;
        bklm_synced_pointer_mode = msg->mode;
        // Adopt the USB half's animation; preview it unless this is just the
        // first message after power-on.
        if (msg->style != bklm_motion_style) {
            bklm_motion_style_set(msg->style, !first);
        }
        first = false;
        if (msg->activity != last_activity) {
            last_activity      = msg->activity;
            bklm_last_input_ms = timer_read32();
        }
        bklm_motion_feed(msg->dx, msg->dy);
    }
}

static void bklm_sync_task(void) {
    static bool     failed        = true; // nothing delivered yet
    static uint8_t  sent_mode     = 0xFF;
    static uint8_t  sent_activity = 0;
    static uint8_t  sent_style    = 0xFF;
    static uint32_t last_sent     = 0;
    const uint32_t  elapsed       = timer_elapsed32(last_sent);
#ifdef COMMUNITY_MODULE_BK_POINTING_DEVICE_ENABLE
    const uint8_t mode = bkpd_mode_get_active_id();
#else
    const uint8_t mode = 0;
#endif
    const bool changed = mode != sent_mode || bklm_activity != sent_activity || bklm_motion_style != sent_style;
    const bool moving  = bklm_motion_x != 0 || bklm_motion_y != 0;

    // Changes go out at once, motion in batches, a refresh every SYNC_MS;
    // failed sends retry every 100 ms, not every loop.
    const bool due = failed ? elapsed >= 100
                            : (changed && !moving) || (moving && elapsed >= LED_MATRIX_MODULE_MOTION_SYNC_MS) || elapsed >= LED_MATRIX_MODULE_SYNC_MS;
    if (!due) {
        return;
    }
    const bklm_sync_t msg = {
        .mode     = mode,
        .activity = bklm_activity,
        .style    = bklm_motion_style,
        .dx       = (int16_t)CONSTRAIN(bklm_motion_x, INT16_MIN, INT16_MAX),
        .dy       = (int16_t)CONSTRAIN(bklm_motion_y, INT16_MIN, INT16_MAX),
    };
    failed    = !transaction_rpc_send(RPC_ID_LED_MATRIX_SYNC, sizeof(msg), &msg);
    last_sent = timer_read32();
    if (!failed) {
        sent_mode     = msg.mode;
        sent_activity = msg.activity;
        sent_style    = msg.style;
        bklm_motion_x -= msg.dx;
        bklm_motion_y -= msg.dy;
    }
}

static bool bklm_on_panel_half(void) {
    return is_keyboard_left() == (LED_MATRIX_MODULE_ON_LEFT != 0);
}

/* Reset the strip so the first painted frame starts from a known-off pin. */
void keyboard_post_init_bk_led_matrix(void) {
    bklm_last_input_ms = timer_read32();
    bklm_motion_style_load();
    transaction_register_rpc(RPC_ID_LED_MATRIX_SYNC, bklm_sync_handler);
    if (bklm_on_panel_half()) {
        bklm_init();
    }
}

/* The sender holds interrupts for ~6 ms, so frames are paced.
 * Clear first: any cell no indicator claims must stay dark.
 *
 * The indicators are mutually exclusive, most urgent first: each returns false
 * when it has nothing to say and the next one gets the frame. A pointer mode,
 * a held modifier and the layer stack each fill the panel on their own, so
 * layering them would only cut holes in one another. The duck is last because
 * it is the resting state, not an indicator.
 *
 * A trackball animation goes after the modifiers while the ball moves; when it
 * idles (LED_MATRIX_MODULE_MOTION_IDLE) it goes behind the layer animation or
 * stack instead, in the duck's place. */

static bool strip_powered = true;

void housekeeping_task_bk_led_matrix(void) {
    static uint32_t last_update = 0;
    static RGB      frame[LED_MATRIX_MODULE_LED_COUNT];

    // The USB half feeds the panel half; only the panel half draws.
    if (is_keyboard_master() && !bklm_on_panel_half()) {
        bklm_sync_task();
    }
    if (!bklm_on_panel_half()) {
        return;
    }

    const uint32_t idle_ms = timer_elapsed32(bklm_last_input_ms);

    if (idle_ms >= LED_MATRIX_MODULE_OFF_MS) {
        if (strip_powered) {
            memset(frame, 0, sizeof(frame));
            bklm_set_idle_brightness_divisor(1);
            bklm_show(frame);
            strip_powered = false;
        }
        return;
    }

    strip_powered = true;
    bklm_set_idle_brightness_divisor(idle_ms >= LED_MATRIX_MODULE_DIM_MS ? 2 : 1);

    if (timer_elapsed32(last_update) < (bklm_motion_active() ? LED_MATRIX_MODULE_MOTION_REFRESH_MS : LED_MATRIX_MODULE_REFRESH_MS)) {
        return;
    }
    last_update = timer_read32();

    memset(frame, 0, sizeof(frame));
    if (bklm_motion_previewing()) {
        bklm_draw_motion(frame);
    } else if (!bklm_pointer_paint(frame) && !bklm_draw_active_modifier_names(frame)) {
        if (bklm_motion_moving()) {
            bklm_draw_motion(frame);
        } else if (!bklm_draw_layer_anim(frame) && !bklm_draw_layer_stack(frame) && !bklm_draw_motion(frame)) {
            bklm_draw_swimming_duck(frame);
        }
    }
    bklm_show(frame);
}

void suspend_power_down_bk_led_matrix(void) {
    static RGB      frame[LED_MATRIX_MODULE_LED_COUNT];
    if (!bklm_on_panel_half() || !strip_powered) {
        return;
    }
    memset(frame, 0, sizeof(frame));
    bklm_set_idle_brightness_divisor(1);
    bklm_show(frame);
    strip_powered = false;
}