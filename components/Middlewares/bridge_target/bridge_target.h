#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "transport_uart.h"

typedef struct {
    transport_uart_config_t uart;
} bridge_target_config_t;

typedef struct bridge_target bridge_target_t;
typedef void (*bridge_target_rx_cb_t)(void *context, const uint8_t *data, size_t length);

bridge_target_t *bridge_target_create(const bridge_target_config_t *config);
void bridge_target_destroy(bridge_target_t *target);
esp_err_t bridge_target_write(bridge_target_t *target, const uint8_t *data, size_t length);
esp_err_t bridge_target_set_line_format(bridge_target_t *target, uint32_t baud_rate,
                                        uart_word_length_t data_bits, uart_parity_t parity,
                                        uart_stop_bits_t stop_bits);
void bridge_target_set_rx_callback(bridge_target_t *target, bridge_target_rx_cb_t callback,
                                   void *context);
