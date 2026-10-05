// Copyright 2026 Quentin LEBASTARD <bstkbd@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "led_matrix_display.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 0, 0);

#if !defined(MCU_RP)
#    error "LED matrix module bitbang is written for the RP2040"
#endif
#if SYS_CLK_KHZ != 125000
#    error "LED matrix module bitbang timings assume a 125 MHz system clock"
#endif

/*
 * Bit-banged WS2812 on LED_MATRIX_MODULE_PIN.
 *
 * bklm_show receives full-scale RGB in visual order. Brightness is applied
 * while copying into bklm_wire as GRB, which is what the LEDs expect.
 * The sender runs from SRAM and holds interrupts off for the frame (~6 ms).
 *
 * Thumb-16 shifts and adds are raw .hword encodings; this assembler rejects
 * the lsls/adds/subs mnemonics. Cycle counts at 125 MHz:
 *   0-bit high 50 cycles (400 ns), 1-bit high 101 cycles (808 ns)
 *   0-bit low ~108 cycles (864 ns), 1-bit low ~55 cycles (440 ns)
 */
static uint8_t bklm_wire[LED_MATRIX_MODULE_LED_COUNT * 3];
static uint8_t bklm_idle_brightness_divisor = 1;

void bklm_set_idle_brightness_divisor(uint8_t divisor) {
    bklm_idle_brightness_divisor = (divisor == 0) ? 1 : divisor;
}

_Static_assert(LED_MATRIX_MODULE_LED_COUNT == BKLM_COLS * BKLM_ROWS, "LED matrix geometry does not match LED_MATRIX_MODULE_LED_COUNT");

/*
 * r0 = pin mask, r1 = byte count, r2 = GRB bytes.
 * The pointer is an asm input so the scaled buffer cannot be optimized away.
 */
/* Bit-bangs one GRB frame. The caller disables interrupts for the duration. */
static void __attribute__((noinline, noipa, section(".time_critical.bklm_send"))) bklm_send(uint32_t pin_mask, uint32_t byte_count, const uint8_t *data) {
    register uint32_t      mask asm("r0")  = pin_mask;
    register uint32_t      count asm("r1") = byte_count;
    register const uint8_t *ptr asm("r2")  = data;

    __asm__ volatile(
        "push {r4, r5, r6, r7}\n\t"
        "movs r4, r0\n\t" /* pin mask */
        "movs r5, r1\n\t" /* bytes left */
        "movs r6, r2\n\t" /* data pointer */
        "movs r0, #0xD0\n\t"
        ".hword 0x0600\n\t" /* lsls r0, r0, #24 -> SIO_BASE */
        "movs r1, r0\n\t"
        ".hword 0x3014\n\t" /* adds r0, #0x14 -> GPIO_OUT_SET */
        ".hword 0x3118\n\t" /* adds r1, #0x18 -> GPIO_OUT_CLR */
        ".hword 0x7837\n\t" /* ldrb r7, [r6] */
        ".hword 0x3601\n\t" /* adds r6, #1 */
        ".hword 0x063F\n\t" /* lsls r7, r7, #24 */
        "movs r2, #8\n\t"
        "1:\n\t"            /* next bit, MSB first */
        ".hword 0x007F\n\t" /* lsls r7, r7, #1 */
        "bcs 3f\n\t"
        "str r4, [r0]\n\t" /* 0-bit high */
        "movs r3, #45\n\t"
        "4:\n\t"
        ".hword 0x3B03\n\t" /* subs r3, #3 */
        "bcs 4b\n\t"
        "str r4, [r1]\n\t"
        "movs r3, #96\n\t" /* 0-bit low */
        "5:\n\t"
        ".hword 0x3B03\n\t" /* subs r3, #3 */
        "bcs 5b\n\t"
        "b 6f\n\t"
        "3:\n\t"
        "str r4, [r0]\n\t" /* 1-bit high */
        "movs r3, #96\n\t"
        "7:\n\t"
        ".hword 0x3B03\n\t" /* subs r3, #3 */
        "bcs 7b\n\t"
        "str r4, [r1]\n\t"
        "movs r3, #45\n\t" /* 1-bit low */
        "8:\n\t"
        ".hword 0x3B03\n\t" /* subs r3, #3 */
        "bcs 8b\n\t"
        "6:\n\t"
        ".hword 0x3A01\n\t" /* subs r2, #1  bit count */
        "bne 1b\n\t"
        ".hword 0x3D01\n\t" /* subs r5, #1  byte count */
        "beq 9f\n\t"
        ".hword 0x7837\n\t" /* ldrb r7, [r6] */
        ".hword 0x3601\n\t" /* adds r6, #1 */
        ".hword 0x063F\n\t" /* lsls r7, r7, #24 */
        "movs r2, #8\n\t"
        "b 1b\n\t"
        "9:\n\t"
        "pop {r4, r5, r6, r7}\n\t"
        : "+l"(mask), "+l"(count), "+l"(ptr)
        :
        : "r3", "r4", "r5", "r6", "r7", "cc", "memory");
}

#define BKLM_MAX_BRIGHTNESS_DIVISOR 40

/* Visual (x, y) → wire order. Index 0 is bottom-right: even rows (from the
 * bottom) run right to left, odd rows run left to right. */
static uint16_t bklm_index(uint8_t x, uint8_t y) {
    if ((y & 1) == 0) {
        return (uint16_t)y * BKLM_COLS + (BKLM_COLS - 1 - x);
    }
    return (uint16_t)y * BKLM_COLS + x;
}

/* Paints one visual framebuffer and returns after the strip latches. No-op when pixels is NULL. */
void bklm_show(const RGB *pixels) {
    if (pixels == NULL) {
        return;
    }

    const uint8_t brightness = rgb_matrix_get_val();

    for (uint8_t y = 0; y < BKLM_ROWS; y++) {
        for (uint8_t x = 0; x < BKLM_COLS; x++) {
            const RGB *src = &pixels[(uint16_t)y * BKLM_COLS + x];
            uint8_t      *dst  = &bklm_wire[bklm_index(x, y) * 3];

            dst[0] = src->g;
            dst[1] = src->r;
            dst[2] = src->b;
            dst[0]   = (dst[0] * brightness) / RGB_MATRIX_MAXIMUM_BRIGHTNESS / BKLM_MAX_BRIGHTNESS_DIVISOR / bklm_idle_brightness_divisor;
            dst[1]   = (dst[1] * brightness) / RGB_MATRIX_MAXIMUM_BRIGHTNESS / BKLM_MAX_BRIGHTNESS_DIVISOR / bklm_idle_brightness_divisor;
            dst[2]   = (dst[2] * brightness) / RGB_MATRIX_MAXIMUM_BRIGHTNESS / BKLM_MAX_BRIGHTNESS_DIVISOR / bklm_idle_brightness_divisor;
        }
    }

    __asm__ volatile("" ::: "memory");
    __asm__ volatile("cpsid i" ::: "memory");
    bklm_send(1u << LED_MATRIX_MODULE_PIN, LED_MATRIX_MODULE_LED_COUNT * 3, bklm_wire);
    __asm__ volatile("cpsie i" ::: "memory");
    wait_us(280); /* latch / reset */
}

/* Drives the data pin low long enough for the strip to reset. */
void bklm_init(void) {
    gpio_set_pin_output(LED_MATRIX_MODULE_PIN);
    gpio_write_pin_low(LED_MATRIX_MODULE_PIN);
    wait_us(280);
}
