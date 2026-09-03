/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>

// Maximum packet size for the debug probe command and response data.
// Typical vales are 64 for Full-speed USB HID or WinUSB,
// 1024 for High-speed USB HID and 512 for High-speed USB WinUSB.
// In case of SWD, the value needs to be the same as DAP_PACKET_SIZE in DAP_config.h
#define DEBUG_PROBE_PACKET_SIZE 64

#define DEBUG_PROBE_TASK_PRI        5

typedef enum {
    DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V2_BULK = 0,
    DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID,
    DEBUG_PROBE_TRANSPORT_COUNT,
} debug_probe_transport_t;

typedef void (*debug_probe_request_callback_t)(debug_probe_transport_t transport,
                                               const uint8_t *data, size_t length, void *context);

/**
 * @brief Initialize the debug probe subsystem
 *
 * This function initializes the debug probe functionality, including SWD/JTAG
 * interfaces, task creation, and internal data structures. Must be called
 * before any other debug probe operations.
 *
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t debug_probe_init(void);
esp_err_t debug_probe_frontend_init(debug_probe_request_callback_t callback, void *context);
esp_err_t debug_probe_submit_response(debug_probe_transport_t transport, const uint8_t *data, size_t length);

/**
 * @brief Process incoming debug probe data
 *
 * Processes raw data received from the host debugger software. This function
 * parses and handles debug commands, SWD/JTAG transactions, and other
 * debug-related communications.
 *
 * @param data Received data buffer
 * @param len Length of received data
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t debug_probe_process_data(debug_probe_transport_t transport, const uint8_t *data, size_t len);

/**
 * @brief Get data ready to be sent to host
 *
 * Retrieves debug probe response data that is ready to be transmitted
 * back to the host debugger. This function may block waiting for data
 * to become available.
 *
 * @param len Pointer to store the length of returned data
 * @param timeout Maximum time to wait for data (FreeRTOS ticks)
 * @return uint8_t* Pointer to data buffer, or NULL if no data available within timeout
 */
uint8_t *debug_probe_get_data_to_send(debug_probe_transport_t transport, size_t *len, TickType_t timeout);

/**
 * @brief Free previously sent data buffer
 *
 * Releases a data buffer that was previously obtained from
 * debug_probe_get_data_to_send() and has been successfully transmitted.
 * This allows the debug probe to reuse or deallocate the buffer.
 *
 * @param data Pointer to data buffer to free
 */
void debug_probe_free_sent_data(debug_probe_transport_t transport, uint8_t *data);

/**
 * @brief Debug activity notification callback
 * @param active true when debug activity is happening, false when idle
 */
typedef void (*debug_activity_notify_cb_t)(bool active);

/**
 * @brief Register callback for debug activity notifications
 *
 * This callback will be called to indicate debug probe activity (JTAG/SWD operations).
 * Useful for driving activity LEDs or other status indicators.
 *
 * @param callback Callback function, can be NULL to disable notifications
 */
void debug_probe_register_activity_callback(debug_activity_notify_cb_t callback);

