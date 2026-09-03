#include "led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Board LED is connected active-low: GPIO low turns it on. */
#define LED_ON_LEVEL 0
#define LED_OFF_LEVEL 1

static volatile led_state_t s_state = LED_STATE_OFF;
static bool s_initialized;

static void led_task(void *argument)
{
    (void)argument;
    bool blink_on = false;

    for (;;) {
        switch (s_state) {
        case LED_STATE_OFF:
            gpio_set_level(LED_GPIO_PIN, LED_OFF_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_STATE_ON:
            gpio_set_level(LED_GPIO_PIN, LED_ON_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_STATE_BLINK_1HZ:
            blink_on = !blink_on;
            gpio_set_level(LED_GPIO_PIN, blink_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        case LED_STATE_BLINK_4HZ:
            blink_on = !blink_on;
            gpio_set_level(LED_GPIO_PIN, blink_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
            vTaskDelay(pdMS_TO_TICKS(125));
            break;
        default:
            s_state = LED_STATE_OFF;
            break;
        }
    }
}

esp_err_t led_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t gpio_init_struct = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO_PIN),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    const esp_err_t result = gpio_config(&gpio_init_struct);
    if (result != ESP_OK) {
        return result;
    }

    gpio_set_level(LED_GPIO_PIN, LED_OFF_LEVEL);
    if (xTaskCreate(led_task, "led_task", 2048, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t led_set_state(led_state_t state)
{
    if (state != LED_STATE_OFF && state != LED_STATE_ON && state != LED_STATE_BLINK_1HZ &&
        state != LED_STATE_BLINK_4HZ) {
        return ESP_ERR_INVALID_ARG;
    }
    s_state = state;
    return ESP_OK;
}
