/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debug_gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#define GET_IDX(mask)   (__builtin_ctz(mask))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#endif

typedef enum {
    DEBUG_PIN_MODE_OFF,
    DEBUG_PIN_MODE_SWD,
    DEBUG_PIN_MODE_JTAG,
} debug_pin_mode_t;

debug_gpio_config_t debug_gpio_pins;
dedic_gpio_bundle_handle_t dedic_gpio_out_bundle;
dedic_gpio_bundle_handle_t dedic_gpio_io_bundle;
gpio_dev_t *const dedic_gpio_dev = GPIO_LL_GET_HW(GPIO_PORT_0);
uint32_t dedic_gpio_conf;

static debug_pin_mode_t s_pin_mode = DEBUG_PIN_MODE_OFF;
static const char *TAG = "debug_gpio";
static debug_gpio_activity_cb_t s_activity_callback = NULL;

esp_err_t debug_gpio_init(const debug_gpio_config_t *config)
{
    if (!config ||
        !GPIO_IS_VALID_GPIO(config->tdi) ||
        !GPIO_IS_VALID_GPIO(config->tdo) ||
        !GPIO_IS_VALID_GPIO(config->tck) ||
        !GPIO_IS_VALID_GPIO(config->tms) ||
        !GPIO_IS_VALID_GPIO(config->nrst)) {
        return ESP_ERR_INVALID_ARG;
    }

    debug_gpio_pins = *config;
    debug_gpio_nrst_setup();
    return ESP_OK;
}

void debug_gpio_nrst_setup(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(GPIO_nRESET),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(GPIO_nRESET, 1); // Release nRESET; the target board supplies the pull-up.
}

void debug_gpio_nrst_write(uint32_t level)
{
    gpio_set_level(GPIO_nRESET, level ? 1 : 0);
}

uint32_t debug_gpio_nrst_read(void)
{
    return gpio_get_level(GPIO_nRESET) ? 1U : 0U;
}

void debug_gpio_register_activity_callback(debug_gpio_activity_cb_t callback)
{
    s_activity_callback = callback;
}

void debug_gpio_notify_activity(bool active)
{
    if (s_activity_callback) {
        s_activity_callback(active);
    }
}

void debug_gpio_reset_pins(void)
{
    debug_gpio_nrst_write(1);

    if (dedic_gpio_io_bundle) {
        ESP_ERROR_CHECK(dedic_gpio_del_bundle(dedic_gpio_io_bundle));
        dedic_gpio_io_bundle = NULL;
    }
    if (dedic_gpio_out_bundle) {
        ESP_ERROR_CHECK(dedic_gpio_del_bundle(dedic_gpio_out_bundle));
        dedic_gpio_out_bundle = NULL;
    }

    gpio_reset_pin(GPIO_SWDIO); // TMS
    gpio_reset_pin(GPIO_SWCLK); // TCK
    gpio_reset_pin(GPIO_TDI);
    gpio_reset_pin(GPIO_TDO);
    s_pin_mode = DEBUG_PIN_MODE_OFF;
}

void debug_gpio_init_jtag_pins(void)
{
    if (s_pin_mode == DEBUG_PIN_MODE_JTAG) {
        return;
    }

    debug_gpio_reset_pins();

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(GPIO_TDI) | BIT64(GPIO_TCK) | BIT64(GPIO_TMS),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT64(GPIO_TDO);
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    int bundle_out_gpios[] = { GPIO_TCK, GPIO_TDI, GPIO_TMS };
    int bundle_in_gpios[] = { GPIO_TDO };

    dedic_gpio_bundle_config_t out_bundle_config = {
        .gpio_array = bundle_out_gpios,
        .array_size = ARRAY_SIZE(bundle_out_gpios),
        .flags = { .out_en = 1 },
    };
    dedic_gpio_bundle_config_t in_bundle_config = {
        .gpio_array = bundle_in_gpios,
        .array_size = ARRAY_SIZE(bundle_in_gpios),
        .flags = { .in_en = 1 },
    };

    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&out_bundle_config, &dedic_gpio_out_bundle));
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&in_bundle_config, &dedic_gpio_io_bundle));

    dedic_gpio_cpu_ll_write_mask(GPIO_TMS_MASK, GPIO_TMS_MASK);
    dedic_gpio_cpu_ll_write_mask(GPIO_TCK_MASK, 0);
    s_pin_mode = DEBUG_PIN_MODE_JTAG;
    ESP_LOGI(TAG, "JTAG GPIO init done");
}

void debug_gpio_init_swd_pins(void)
{
    if (s_pin_mode == DEBUG_PIN_MODE_SWD) {
        return;
    }

    debug_gpio_reset_pins();

    debug_gpio_mode_in_out_enable(GPIO_SWDIO);
    gpio_set_pull_mode(GPIO_SWDIO, GPIO_PULLUP_ONLY);
    debug_gpio_mode_out_enable(GPIO_SWCLK);

    int bundle_out_gpios[GET_IDX(GPIO_SWD_OUT_MAX_MASK)] = { 0 };
    int bundle_io_gpios[GET_IDX(GPIO_SWDIO_MAX_MASK)] = { 0 };

    bundle_io_gpios[GET_IDX(GPIO_SWDIO_MASK)] = GPIO_SWDIO;
    dedic_gpio_bundle_config_t io_bundle_config = {
        .gpio_array = bundle_io_gpios,
        .array_size = ARRAY_SIZE(bundle_io_gpios),
        .flags = { .out_en = 1, .in_en = 1 },
    };

    bundle_out_gpios[GET_IDX(GPIO_SWCLK_MASK)] = GPIO_SWCLK;
    dedic_gpio_bundle_config_t out_bundle_config = {
        .gpio_array = bundle_out_gpios,
        .array_size = ARRAY_SIZE(bundle_out_gpios),
        .flags = { .out_en = 1 },
    };

    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&out_bundle_config, &dedic_gpio_out_bundle));
    ESP_ERROR_CHECK(dedic_gpio_new_bundle(&io_bundle_config, &dedic_gpio_io_bundle));
    dedic_gpio_conf = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + (GPIO_SWDIO * 4));
    s_pin_mode = DEBUG_PIN_MODE_SWD;
    ESP_LOGI(TAG, "SWD GPIO init done");
}

