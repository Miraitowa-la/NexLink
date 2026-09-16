#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define KEY_GPIO_PIN GPIO_NUM_38

typedef void (*key_click_callback_t)(void *context);
typedef void (*key_hold_callback_t)(void *context);

esp_err_t key_init(void);
void key_register_click_callback(key_click_callback_t callback, void *context);
void key_register_hold_callbacks(key_hold_callback_t pair_callback, void *pair_context,
                                 key_hold_callback_t reset_callback, void *reset_context);
