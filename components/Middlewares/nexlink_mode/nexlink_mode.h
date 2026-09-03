#pragma once

#include "esp_err.h"

typedef enum {
    NEXLINK_RX_MODE_DIRECT = 0,
    NEXLINK_RX_MODE_WIRELESS = 1,
} nexlink_rx_mode_t;

typedef enum {
    NEXLINK_TX_MODE_OFF = 0,
    NEXLINK_TX_MODE_ACTIVE = 1,
} nexlink_tx_mode_t;

nexlink_rx_mode_t nexlink_mode_get_rx(void);
nexlink_tx_mode_t nexlink_mode_get_tx(void);

/** Store the requested RX mode and restart the ESP32-S3 on success. */
esp_err_t nexlink_mode_set_rx_and_restart(nexlink_rx_mode_t mode);

/** Store the requested TX mode and restart the ESP32-S3 on success. */
esp_err_t nexlink_mode_set_tx_and_restart(nexlink_tx_mode_t mode);
