/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "debug_probe.h"
#include "debug_gpio.h"
#include "DAP_config.h"
#include "DAP.h"
#include "nexlink_task_config.h"

static const char *TAG = "cmsis_dap";

#define DAP_RCVBUF_SIZE     1024
#define DAP_SNDBUF_SIZE     1024

typedef struct {
    debug_probe_transport_t transport;
    uint8_t data[DAP_PACKET_SIZE];
} dap_request_t;

static RingbufHandle_t s_dap_rcvbuf = NULL;
static RingbufHandle_t s_dap_sndbuf[DEBUG_PROBE_TRANSPORT_COUNT] = {0};
static TaskHandle_t s_dap_task_handle = NULL;
static debug_probe_transport_t s_owner_transport = DEBUG_PROBE_TRANSPORT_COUNT;
static debug_probe_request_callback_t s_frontend_callback;
static void *s_frontend_context;

void debug_probe_register_activity_callback(debug_activity_notify_cb_t callback)
{
    debug_gpio_register_activity_callback(callback);
}

static bool cmsis_dap_is_allowed(debug_probe_transport_t transport, uint8_t command)
{
    return s_owner_transport == DEBUG_PROBE_TRANSPORT_COUNT ||
           s_owner_transport == transport ||
           command == ID_DAP_Info ||
           command == ID_DAP_Connect;
}

static void cmsis_dap_disconnect(void)
{
    uint8_t request[DAP_PACKET_SIZE] = {ID_DAP_Disconnect};
    uint8_t response[DAP_PACKET_SIZE];

    DAP_ProcessCommand(request, response);
    s_owner_transport = DEBUG_PROBE_TRANSPORT_COUNT;
}

static void cmsis_dap_select_port(uint8_t requested_port)
{
    static const uint8_t line_reset[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t jtag_to_swd[] = {0x9E, 0xE7};
    static const uint8_t swd_to_jtag[] = {0x3C, 0xE7};
    const uint8_t port = requested_port == DAP_PORT_AUTODETECT ? DAP_DEFAULT_PORT : requested_port;

    switch (port) {
    case DAP_PORT_SWD:
        PORT_SWD_SETUP();
        SWJ_Sequence(56, line_reset);
        SWJ_Sequence(16, jtag_to_swd);
        SWJ_Sequence(56, line_reset);
        break;
    case DAP_PORT_JTAG:
        PORT_JTAG_SETUP();
        SWJ_Sequence(56, line_reset);
        SWJ_Sequence(16, swd_to_jtag);
        SWJ_Sequence(56, line_reset);
        break;
    default:
        break;
    }
}

esp_err_t debug_probe_process_data(debug_probe_transport_t transport, const uint8_t *data, size_t len)
{
    if (transport >= DEBUG_PROBE_TRANSPORT_COUNT || len > DAP_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    dap_request_t request = {
        .transport = transport,
    };
    memcpy(request.data, data, len);

    BaseType_t res = xRingbufferSend(s_dap_rcvbuf, &request, sizeof(request), pdMS_TO_TICKS(1000));
    if (res != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to CMSIS-DAP receive buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_dap_rcvbuf), DAP_RCVBUF_SIZE);
        return ESP_FAIL;
    }

    return ESP_OK;
}

uint8_t *debug_probe_get_data_to_send(debug_probe_transport_t transport, size_t *len, TickType_t timeout)
{
    if (transport >= DEBUG_PROBE_TRANSPORT_COUNT) {
        return NULL;
    }

    uint8_t *data = xRingbufferReceive(s_dap_sndbuf[transport], len, timeout);
    if (data && *len > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, *len, ESP_LOG_DEBUG);
    }
    return data;
}

void debug_probe_free_sent_data(debug_probe_transport_t transport, uint8_t *data)
{
    if (transport < DEBUG_PROBE_TRANSPORT_COUNT && data != NULL) {
        vRingbufferReturnItem(s_dap_sndbuf[transport], data);
    }
}

static esp_err_t cmsis_dap_send_data(debug_probe_transport_t transport, const void *buf, const size_t size)
{
    if (xRingbufferSend(s_dap_sndbuf[transport], buf, size, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Cannot write to CMSIS-DAP send buffer (free %zu of %d)!",
                 xRingbufferGetCurFreeSize(s_dap_sndbuf), DAP_SNDBUF_SIZE);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void cmsis_dap_task(void *pvParameters)
{
    size_t total_bytes = 0;
    uint8_t response[DAP_PACKET_SIZE];

    s_dap_task_handle = xTaskGetCurrentTaskHandle();

    while (1) {
        dap_request_t *request = xRingbufferReceive(s_dap_rcvbuf, &total_bytes, portMAX_DELAY);
        if (total_bytes != sizeof(*request)) {
            ESP_LOGE(TAG, "Invalid CMSIS-DAP request size: %zu", total_bytes);
            vRingbufferReturnItem(s_dap_rcvbuf, request);
            continue;
        }

        const uint8_t command = request->data[0];
        uint32_t resp_len;

        if (s_frontend_callback) {
            s_frontend_callback(request->transport, request->data, DAP_PACKET_SIZE, s_frontend_context);
            vRingbufferReturnItem(s_dap_rcvbuf, request);
            continue;
        }

        if (!cmsis_dap_is_allowed(request->transport, command)) {
            response[0] = command;
            response[1] = DAP_ERROR;
            resp_len = 2;
        } else {
            if (command == ID_DAP_Connect) {
                if (s_owner_transport != DEBUG_PROBE_TRANSPORT_COUNT &&
                    (s_owner_transport != request->transport ||
                     DAP_Data.debug_port != request->data[1])) {
                    cmsis_dap_disconnect();
                }
                cmsis_dap_select_port(request->data[1]);
            }

            resp_len = DAP_ProcessCommand(request->data, response) & 0xFFFF;

            if (command == ID_DAP_Connect && resp_len >= 2 && response[1] != DAP_PORT_DISABLED) {
                s_owner_transport = request->transport;
            } else if (command == ID_DAP_Disconnect && s_owner_transport == request->transport) {
                s_owner_transport = DEBUG_PROBE_TRANSPORT_COUNT;
            }
        }

        if (cmsis_dap_send_data(request->transport, response, resp_len) != ESP_OK) {
            ESP_LOGE(TAG, "Cannot queue CMSIS-DAP response");
        }
        vRingbufferReturnItem(s_dap_rcvbuf, request);
    }
}

static esp_err_t debug_probe_start(bool target_executor, debug_probe_request_callback_t callback, void *context)
{
    ESP_LOGI(TAG, "Initializing CMSIS-DAP probe with SWD and JTAG support");

    s_dap_rcvbuf = xRingbufferCreate(DAP_RCVBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    s_owner_transport = DEBUG_PROBE_TRANSPORT_COUNT;
    s_frontend_callback = callback;
    s_frontend_context = context;
    for (size_t i = 0; i < DEBUG_PROBE_TRANSPORT_COUNT; ++i) {
        s_dap_sndbuf[i] = xRingbufferCreate(DAP_SNDBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    }
    if (!s_dap_rcvbuf || !s_dap_sndbuf[DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V2_BULK] ||
        !s_dap_sndbuf[DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID]) {
        ESP_LOGE(TAG, "Cannot allocate CMSIS-DAP buffers!");
        return ESP_FAIL;
    }

    _Static_assert(DEBUG_PROBE_PACKET_SIZE == DAP_PACKET_SIZE,
                   "DEBUG_PROBE_PACKET_SIZE must equal DAP_PACKET_SIZE");
    if (target_executor) DAP_Setup();

    BaseType_t res = xTaskCreatePinnedToCore(cmsis_dap_task,
                     "cmsis_dap_task",
                     4 * 1024,
                     NULL,
                     DEBUG_PROBE_TASK_PRI,
                     &s_dap_task_handle,
                     NEXLINK_CPU_TARGET);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Cannot create CMSIS-DAP task!");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t debug_probe_init(void)
{
    return debug_probe_start(true, NULL, NULL);
}

esp_err_t debug_probe_frontend_init(debug_probe_request_callback_t callback, void *context)
{
    return callback ? debug_probe_start(false, callback, context) : ESP_ERR_INVALID_ARG;
}

esp_err_t debug_probe_submit_response(debug_probe_transport_t transport, const uint8_t *data, size_t length)
{
    if (transport >= DEBUG_PROBE_TRANSPORT_COUNT || !data || length == 0 || length > DAP_PACKET_SIZE) return ESP_ERR_INVALID_ARG;
    return cmsis_dap_send_data(transport, data, length);
}
