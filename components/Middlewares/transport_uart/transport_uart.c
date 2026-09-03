#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nexlink_task_config.h"
#include "transport_uart.h"

#define UART_DRIVER_BUFFER_SIZE 2048
#define UART_EVENT_QUEUE_LENGTH 16
#define UART_TASK_STACK_SIZE    4096
#define UART_TASK_PRIORITY      5

struct transport_uart {
    transport_uart_config_t config;
    QueueHandle_t event_queue;
    TaskHandle_t task;
    transport_uart_rx_cb_t rx_callback;
    void *rx_context;
};

static void transport_uart_task(void *argument)
{
    transport_uart_t *transport = argument;
    uart_event_t event;
    uint8_t buffer[256];

    for (;;) {
        if (xQueueReceive(transport->event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (event.type == UART_DATA) {
            size_t remaining = event.size;
            while (remaining > 0) {
                const size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
                const int read = uart_read_bytes(transport->config.port, buffer, wanted,
                                                 pdMS_TO_TICKS(20));
                if (read <= 0) {
                    break;
                }
                if (transport->rx_callback) {
                    transport->rx_callback(transport->rx_context, buffer, (size_t)read);
                }
                remaining -= (size_t)read;
            }
        } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
            uart_flush_input(transport->config.port);
            xQueueReset(transport->event_queue);
        }
    }
}

transport_uart_t *transport_uart_create(const transport_uart_config_t *config)
{
    if (!config) {
        return NULL;
    }

    transport_uart_t *transport = calloc(1, sizeof(*transport));
    if (!transport) {
        return NULL;
    }
    transport->config = *config;

    const uart_config_t uart_config = {
        .baud_rate = (int)config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_param_config(config->port, &uart_config) != ESP_OK ||
        uart_set_pin(config->port, config->tx_pin, config->rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_driver_install(config->port, UART_DRIVER_BUFFER_SIZE, UART_DRIVER_BUFFER_SIZE,
                            UART_EVENT_QUEUE_LENGTH, &transport->event_queue, 0) != ESP_OK) {
        free(transport);
        return NULL;
    }

    if (xTaskCreatePinnedToCore(transport_uart_task, "target_uart", UART_TASK_STACK_SIZE, transport,
                                UART_TASK_PRIORITY, &transport->task, NEXLINK_CPU_TARGET) != pdPASS) {
        uart_driver_delete(config->port);
        free(transport);
        return NULL;
    }
    return transport;
}

void transport_uart_destroy(transport_uart_t *transport)
{
    if (!transport) {
        return;
    }
    if (transport->task) {
        vTaskDelete(transport->task);
    }
    uart_driver_delete(transport->config.port);
    free(transport);
}

esp_err_t transport_uart_write(transport_uart_t *transport, const uint8_t *data, size_t length)
{
    if (!transport || (!data && length != 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    return uart_write_bytes(transport->config.port, data, length) == (int)length ? ESP_OK : ESP_FAIL;
}

esp_err_t transport_uart_set_line_format(transport_uart_t *transport, uint32_t baud_rate,
                                         uart_word_length_t data_bits, uart_parity_t parity,
                                         uart_stop_bits_t stop_bits)
{
    if (!transport || baud_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const uart_config_t uart_config = {
        .baud_rate = (int)baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    const esp_err_t result = uart_param_config(transport->config.port, &uart_config);
    if (result == ESP_OK) {
        transport->config.baud_rate = baud_rate;
    }
    return result;
}

void transport_uart_set_rx_callback(transport_uart_t *transport, transport_uart_rx_cb_t callback,
                                    void *context)
{
    if (transport) {
        transport->rx_callback = callback;
        transport->rx_context = context;
    }
}
