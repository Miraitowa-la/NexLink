/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <driver/gpio.h>
#include <driver/dedic_gpio.h>
#include <esp_err.h>
#include <hal/dedic_gpio_cpu_ll.h>
#include "soc/gpio_reg.h"
#include "hal/gpio_ll.h"
#include "compiler.h"

/** @brief Physical ESP32-S3 pin binding for the shared SWD/JTAG connector. */
typedef struct
{
    gpio_num_t tdi;
    gpio_num_t tdo;
    gpio_num_t tck;
    gpio_num_t tms;
    gpio_num_t nrst;
} debug_gpio_config_t;

/*
 * Pin values used by the performance-sensitive inline operations below.
 * Only debug_gpio_init() may update this binding, before debug_probe_init().
 */
extern debug_gpio_config_t debug_gpio_pins;

/* Debug pins - JTAG */
#define GPIO_TDI        (debug_gpio_pins.tdi)
#define GPIO_TDO        (debug_gpio_pins.tdo)
#define GPIO_TCK        (debug_gpio_pins.tck)
#define GPIO_TMS        (debug_gpio_pins.tms)

/* Debug pins - SWD */
#define GPIO_SWCLK      (debug_gpio_pins.tck)
#define GPIO_SWDIO      (debug_gpio_pins.tms)
#define GPIO_nRESET     (debug_gpio_pins.nrst)

/* GPIO bundle mask values for high-speed operations */
/* SWD io pin mask values */
#define GPIO_SWDIO_MASK         0x01    /* bundle_io_gpios[0] */ /* input/output */
#define GPIO_SWDIO_MAX_MASK     0x02    /* will be used as io array size */

/* SWD out pin mask values */
#define GPIO_SWCLK_MASK         0x01    /* bundle_out_gpios[0] */
#define GPIO_SWD_BLINK_MASK     0x02    /* bundle_out_gpios[1] */
#define GPIO_SWD_OUT_MAX_MASK   0x04    /* will be used as out array size */
#define GPIO_SWDIO_OUT_MASK     0x04    /* will not be in the out array, but it should follow the previous pin mask */

/* JTAG out pin mask values */
#define GPIO_TCK_MASK           0x01
#define GPIO_TDI_MASK           0x02
#define GPIO_TMS_MASK           0x04
#define GPIO_TMS_TDI_MASK       0x06

/* JTAG input pin mask values */
#define GPIO_TDO_MASK           0x01

extern dedic_gpio_bundle_handle_t dedic_gpio_out_bundle;
extern dedic_gpio_bundle_handle_t dedic_gpio_io_bundle;
extern gpio_dev_t *const dedic_gpio_dev;
extern uint32_t dedic_gpio_conf;

typedef void (*debug_gpio_activity_cb_t)(bool active);

/** @brief Store and validate the fixed board pin binding used by SWD and JTAG. */
esp_err_t debug_gpio_init(const debug_gpio_config_t *config);
void debug_gpio_nrst_setup(void);
void debug_gpio_nrst_write(uint32_t level);
uint32_t debug_gpio_nrst_read(void);
void debug_gpio_register_activity_callback(debug_gpio_activity_cb_t callback);
void debug_gpio_notify_activity(bool active);

/**
 * @brief Initialize debug GPIO pins for JTAG mode
 */
void debug_gpio_init_jtag_pins(void);

/**
 * @brief Initialize debug GPIO pins for SWD mode
 */
void debug_gpio_init_swd_pins(void);

/**
 * @brief Reset and cleanup debug GPIO pins
 */
void debug_gpio_reset_pins(void);

/**
 * @brief Notify debug activity
 * @param active true when debug activity is happening, false when idle
 */
void debug_probe_notify_activity(bool active);

/* High-performance inline GPIO functions for debug operations */

__STATIC_FORCEINLINE void debug_gpio_swdio_out_enable(void)
{
    REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + (GPIO_SWDIO * 4), dedic_gpio_conf);
    gpio_ll_output_enable(dedic_gpio_dev, GPIO_SWDIO);
}

__STATIC_FORCEINLINE void debug_gpio_swdio_out_disable(void)
{
    gpio_ll_output_disable(dedic_gpio_dev, GPIO_SWDIO);
}

__STATIC_FORCEINLINE int debug_gpio_swdio_read(void)
{
    return dedic_gpio_cpu_ll_read_in();
}

__STATIC_FORCEINLINE void debug_gpio_swclk_set(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_SWCLK_MASK, GPIO_SWCLK_MASK);
}

__STATIC_FORCEINLINE void debug_gpio_swclk_clr(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_SWCLK_MASK, 0);
}

__STATIC_FORCEINLINE void debug_gpio_swdio_set(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_SWDIO_OUT_MASK, GPIO_SWDIO_OUT_MASK);
}

__STATIC_FORCEINLINE void debug_gpio_swdio_clr(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_SWDIO_OUT_MASK, 0);
}

__STATIC_FORCEINLINE void debug_gpio_swdio_write(int val)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_SWDIO_OUT_MASK, (val & 0x01) ? GPIO_SWDIO_OUT_MASK : 0);
}

__STATIC_FORCEINLINE void debug_gpio_swd_blink(int on)
{
    debug_gpio_notify_activity(on);
}

__STATIC_FORCEINLINE void debug_gpio_jtag_led_off(void)
{
    debug_gpio_notify_activity(false);
}

__STATIC_FORCEINLINE void debug_gpio_jtag_led_on(void)
{
    debug_gpio_notify_activity(true);
}

__STATIC_FORCEINLINE int debug_gpio_tdo_read(void)
{
    return dedic_gpio_cpu_ll_read_in();
}

__STATIC_FORCEINLINE void debug_gpio_tdi_write(int val)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_TDI_MASK, (val & 0x01) ? GPIO_TDI_MASK : 0);
}

__STATIC_FORCEINLINE void debug_gpio_tck_set(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_TCK_MASK, GPIO_TCK_MASK);
}

__STATIC_FORCEINLINE void debug_gpio_tck_clr(void)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_TCK_MASK, 0);
}

__STATIC_FORCEINLINE void debug_gpio_write_tmstck(uint8_t tms_tdi_mask)
{
    dedic_gpio_cpu_ll_write_mask(GPIO_TMS_TDI_MASK, tms_tdi_mask);
}

/* Basic GPIO mode functions for debug pins */
__STATIC_FORCEINLINE void debug_gpio_mode_input_enable(int gpio_num)
{
    gpio_ll_output_disable(dedic_gpio_dev, gpio_num);
    gpio_ll_input_enable(dedic_gpio_dev, gpio_num);
}

__STATIC_FORCEINLINE void debug_gpio_mode_out_enable(int gpio_num)
{
    gpio_ll_input_disable(dedic_gpio_dev, gpio_num);
    gpio_ll_output_enable(dedic_gpio_dev, gpio_num);
}

__STATIC_FORCEINLINE void debug_gpio_mode_in_out_enable(int gpio_num)
{
    gpio_ll_input_enable(dedic_gpio_dev, gpio_num);
    gpio_ll_output_enable(dedic_gpio_dev, gpio_num);
}

__STATIC_FORCEINLINE void debug_gpio_set(int gpio_num)
{
    gpio_ll_set_level(dedic_gpio_dev, gpio_num, 1);
}

__STATIC_FORCEINLINE void debug_gpio_clear(int gpio_num)
{
    gpio_ll_set_level(dedic_gpio_dev, gpio_num, 0);
}
