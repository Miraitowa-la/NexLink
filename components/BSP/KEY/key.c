#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "key.h"

#define KEY_DEBOUNCE_MS 30
#define KEY_PAIR_HOLD_MS 2000
#define KEY_RESET_HOLD_MS 5000

static QueueHandle_t s_key_events;
static key_click_callback_t s_click_callback;
static void *s_click_context;
static key_hold_callback_t s_pair_callback, s_reset_callback;
static void *s_pair_context, *s_reset_context;

static void IRAM_ATTR key_isr(void *argument)
{
    const uint32_t pin = (uint32_t)argument;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(s_key_events, &pin, &higher_priority_task_woken);
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void key_task(void *argument)
{
    (void)argument;
    uint32_t pin;
    for (;;) {
        if (xQueueReceive(s_key_events, &pin, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(KEY_DEBOUNCE_MS));
        if (gpio_get_level((gpio_num_t)pin) != 0) continue;
        uint32_t held_ms = 0;
        while (gpio_get_level((gpio_num_t)pin) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            held_ms += 10;
        }
        if (held_ms >= KEY_RESET_HOLD_MS && s_reset_callback) s_reset_callback(s_reset_context);
        else if (held_ms >= KEY_PAIR_HOLD_MS && s_pair_callback) s_pair_callback(s_pair_context);
        else if (s_click_callback) s_click_callback(s_click_context);
    }
}

void key_register_hold_callbacks(key_hold_callback_t pair_callback, void *pair_context,
                                 key_hold_callback_t reset_callback, void *reset_context)
{
    s_pair_callback = pair_callback; s_pair_context = pair_context;
    s_reset_callback = reset_callback; s_reset_context = reset_context;
}

esp_err_t key_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << KEY_GPIO_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }
    s_key_events = xQueueCreate(4, sizeof(uint32_t));
    if (!s_key_events) {
        return ESP_ERR_NO_MEM;
    }
    result = gpio_install_isr_service(0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    result = gpio_isr_handler_add(KEY_GPIO_PIN, key_isr, (void *)KEY_GPIO_PIN);
    if (result != ESP_OK) {
        return result;
    }
    return xTaskCreate(key_task, "key_task", 2048, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void key_register_click_callback(key_click_callback_t callback, void *context)
{
    s_click_callback = callback;
    s_click_context = context;
}
