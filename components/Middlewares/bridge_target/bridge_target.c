#include <stdlib.h>

#include "bridge_target.h"

struct bridge_target {
    transport_uart_t *uart;
    bridge_target_rx_cb_t rx_callback;
    void *rx_context;
};

static void bridge_target_uart_rx(void *context, const uint8_t *data, size_t length)
{
    bridge_target_t *target = context;
    if (target->rx_callback) {
        target->rx_callback(target->rx_context, data, length);
    }
}

bridge_target_t *bridge_target_create(const bridge_target_config_t *config)
{
    if (!config) {
        return NULL;
    }
    bridge_target_t *target = calloc(1, sizeof(*target));
    if (!target) {
        return NULL;
    }
    target->uart = transport_uart_create(&config->uart);
    if (!target->uart) {
        free(target);
        return NULL;
    }
    transport_uart_set_rx_callback(target->uart, bridge_target_uart_rx, target);
    return target;
}

void bridge_target_destroy(bridge_target_t *target)
{
    if (target) {
        transport_uart_destroy(target->uart);
        free(target);
    }
}

esp_err_t bridge_target_write(bridge_target_t *target, const uint8_t *data, size_t length)
{
    return target ? transport_uart_write(target->uart, data, length) : ESP_ERR_INVALID_ARG;
}

esp_err_t bridge_target_set_line_format(bridge_target_t *target, uint32_t baud_rate,
                                        uart_word_length_t data_bits, uart_parity_t parity,
                                        uart_stop_bits_t stop_bits)
{
    return target ? transport_uart_set_line_format(target->uart, baud_rate, data_bits, parity, stop_bits)
                  : ESP_ERR_INVALID_ARG;
}

void bridge_target_set_rx_callback(bridge_target_t *target, bridge_target_rx_cb_t callback,
                                   void *context)
{
    if (target) {
        target->rx_callback = callback;
        target->rx_context = context;
    }
}
