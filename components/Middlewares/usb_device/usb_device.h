/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct bridge_target bridge_target_t;
typedef void (*usb_device_frontend_cdc_rx_cb_t)(const uint8_t *data, size_t length, void *context);
typedef void (*usb_device_frontend_line_coding_cb_t)(uint32_t bit_rate, uint8_t data_bits,
                                                      uint8_t parity, uint8_t stop_bits, void *context);
typedef struct {
    usb_device_frontend_cdc_rx_cb_t cdc_rx;
    usb_device_frontend_line_coding_cb_t line_coding;
    void *context;
} usb_device_frontend_t;

/** @brief USB device descriptor values that are fixed for one firmware build. */
typedef struct
{
    uint16_t vendor_id;
    uint16_t product_id;
    const char *manufacturer;
} usb_device_config_t;

/**
 * @brief Generate the stable USB serial number from the ESP32-S3 factory MAC.
 *
 * This function must run before USB enumeration starts.
 */
esp_err_t usb_device_init_serial_number(void);

/**
 * @brief Start the composite CMSIS-DAP USB device.
 *
 * The device exposes CMSIS-DAP v2 Vendor Bulk, CMSIS-DAP v1 HID, and one CDC
 * virtual serial port bridged to target UART.
 * It is a process-wide singleton and may only be started once.
 * @param config Static USB descriptor configuration owned by the caller.
 */
esp_err_t usb_device_start(const usb_device_config_t *config, bridge_target_t *cdc_target);
esp_err_t usb_device_start_frontend(const usb_device_config_t *config, const usb_device_frontend_t *frontend);
esp_err_t usb_device_frontend_write(const uint8_t *data, size_t length);
