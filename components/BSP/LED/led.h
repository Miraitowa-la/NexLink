#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define LED_GPIO_PIN GPIO_NUM_48
#define LED_CDC_RX_GPIO_PIN GPIO_NUM_9
#define LED_CDC_TX_GPIO_PIN GPIO_NUM_46

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK_1HZ,
    LED_STATE_BLINK_4HZ,
} led_state_t;

esp_err_t led_init(void);
esp_err_t led_set_state(led_state_t state);
void led_cdc_rx_activity(void);
void led_cdc_tx_activity(void);
