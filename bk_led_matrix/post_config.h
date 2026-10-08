// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// Copyright 2026 Ying Kun Zhan <ying@zhan.co.nl>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
 * Second WS2812 chain. QMK's driver is limited to 256 LEDs and is already
 * used for the per-key matrix, so this strip is bit-banged on its own pin.
 *
 * Define LED_MATRIX_MODULE_PIN in the keyboard or keymap config
 * (this file runs after keymap config.h).
 */
#ifndef LED_MATRIX_MODULE_PIN
#    error "led_matrix module requires LED_MATRIX_MODULE_PIN"
#endif
/* Argos registers the sync RPC and stores the chosen animation. */
#ifndef COMMUNITY_MODULE_ARGOS_ENABLE
#    error "led_matrix module requires the bastardkb/argos module"
#endif
#ifndef LED_MATRIX_MODULE_LED_COUNT
#    define LED_MATRIX_MODULE_LED_COUNT 192
#endif
/* A frame holds interrupts for ~6 ms, so it is paced instead of sent every loop. */
#ifndef LED_MATRIX_MODULE_REFRESH_MS
#    define LED_MATRIX_MODULE_REFRESH_MS 100
#endif
#ifndef LED_MATRIX_MODULE_DIM_MS
#    define LED_MATRIX_MODULE_DIM_MS 30000 // 30 seconds
#endif
#ifndef LED_MATRIX_MODULE_OFF_MS
#    define LED_MATRIX_MODULE_OFF_MS 120000 // 2 minutes
#endif
/* The panel is on the left half only. Drawing on the other half would
 * just hold its interrupts off for ~6 ms a frame. Define as 0 for a right panel. */
#ifndef LED_MATRIX_MODULE_ON_LEFT
#    define LED_MATRIX_MODULE_ON_LEFT 1
#endif
/* How often the USB half re-sends the pointer mode even if unchanged,
 * so a half that reconnects or missed a sync catches up. */
#ifndef LED_MATRIX_MODULE_SYNC_MS
#    define LED_MATRIX_MODULE_SYNC_MS 500
#endif
/* Trackball motion is batched and sent to the panel half this often. */
#ifndef LED_MATRIX_MODULE_MOTION_SYNC_MS
#    define LED_MATRIX_MODULE_MOTION_SYNC_MS 20
#endif

/* Trackball animations (led_matrix_motion*.c).
 *
 * LED_MATRIX_MODULE_MOTION_CYCLE: the styles LED_MATRIX_ANIMATION_NEXT cycles
 * through, in order, as bklm_motion_style_t values (led_matrix_motion.h), e.g.
 * BKLM_MOTION_SPARKS, BKLM_MOTION_USER + 0. Unset: every built-in style, then
 * every user style. */
#ifndef LED_MATRIX_MODULE_MOTION_USER_COUNT // styles drawn by the keymap's bklm_motion_user()
#    define LED_MATRIX_MODULE_MOTION_USER_COUNT 0
#endif
#ifndef LED_MATRIX_MODULE_MOTION_STYLE // starting style, until one is picked and saved
#    define LED_MATRIX_MODULE_MOTION_STYLE BKLM_MOTION_SPARKS
#endif
/* 1: the animation keeps running, calmly, while the ball is still, in place of
 * the duck (the layer stack still shows over it while a layer is held).
 * 0: it fades out after the ball stops and the duck comes back. */
#ifndef LED_MATRIX_MODULE_MOTION_IDLE
#    define LED_MATRIX_MODULE_MOTION_IDLE 1
#endif
/* With MOTION_IDLE 1: fade the idle animation out this long after the ball
 * stops, so the duck comes back. 0: never. */
#ifndef LED_MATRIX_MODULE_MOTION_IDLE_MS
#    define LED_MATRIX_MODULE_MOTION_IDLE_MS 0
#endif
#ifndef LED_MATRIX_MODULE_MOTION_IDLE_SPEED // the drift while still, counts/s along the last direction
#    define LED_MATRIX_MODULE_MOTION_IDLE_SPEED 400
#endif
/* Frames come faster while an animation shows: each still holds interrupts
 * ~6 ms, so 50 ms is ~12% of the time. */
#ifndef LED_MATRIX_MODULE_MOTION_REFRESH_MS
#    define LED_MATRIX_MODULE_MOTION_REFRESH_MS 50
#endif
#ifndef LED_MATRIX_MODULE_MOTION_HOLD_MS // full strength this long after the ball stops
#    define LED_MATRIX_MODULE_MOTION_HOLD_MS 150
#endif
#ifndef LED_MATRIX_MODULE_MOTION_FADE_MS // then fades out over this long
#    define LED_MATRIX_MODULE_MOTION_FADE_MS 400
#endif
#ifndef LED_MATRIX_MODULE_MOTION_MIN_SPEED // counts/s below which the direction is kept
#    define LED_MATRIX_MODULE_MOTION_MIN_SPEED 40
#endif
#ifndef LED_MATRIX_MODULE_MOTION_PREVIEW_MS // after LED_MATRIX_ANIMATION_NEXT, play the new style this long
#    define LED_MATRIX_MODULE_MOTION_PREVIEW_MS 1000
#endif
#ifndef LED_MATRIX_MODULE_MOTION_FULL_SPEED // counts/s for full brightness and speed
#    define LED_MATRIX_MODULE_MOTION_FULL_SPEED 4000
#endif

#ifndef LED_MATRIX_MODULE_OCEAN_LENGTH // main swell, crest to crest, in cells
#    define LED_MATRIX_MODULE_OCEAN_LENGTH 8
#endif
#ifndef LED_MATRIX_MODULE_ASTEROIDS_TURN_RATE // ship turning, in 1/256 turns per second
#    define LED_MATRIX_MODULE_ASTEROIDS_TURN_RATE 320
#endif
#ifndef LED_MATRIX_MODULE_ASTEROIDS_FIRE_MS // average time between shots (random, half to 1.5x this)
#    define LED_MATRIX_MODULE_ASTEROIDS_FIRE_MS 550
#endif
