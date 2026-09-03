#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

typedef void (*transport_uart_rx_cb_t)(void *context, const uint8_t *data, size_t length);

typedef struct {
    uart_port_t port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    uint32_t baud_rate;
} transport_uart_config_t;

typedef struct transport_uart transport_uart_t;

transport_uart_t *transport_uart_create(const transport_uart_config_t *config);
void transport_uart_destroy(transport_uart_t *transport);
esp_err_t transport_uart_write(transport_uart_t *transport, const uint8_t *data, size_t length);
esp_err_t transport_uart_set_line_format(transport_uart_t *transport, uint32_t baud_rate,
                                         uart_word_length_t data_bits, uart_parity_t parity,
                                         uart_stop_bits_t stop_bits);
void transport_uart_set_rx_callback(transport_uart_t *transport, transport_uart_rx_cb_t callback,
                                    void *context);
