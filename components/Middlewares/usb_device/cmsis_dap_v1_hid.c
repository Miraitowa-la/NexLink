/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#include "cmsis_dap_v1_hid.h"
#include "debug_probe.h"
#include "nexlink_task_config.h"

static const char *TAG = "cmsis_dap_v1_hid";

static void cmsis_dap_v1_hid_task(void *parameter)
{
    (void)parameter;

    for (;;) {
        size_t response_length;
        uint8_t *response = debug_probe_get_data_to_send(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID,
                                                          &response_length, portMAX_DELAY);
        uint8_t report[CFG_TUD_HID_EP_BUFSIZE] = {0};

        if (response_length > sizeof(report)) {
            ESP_LOGE(TAG, "CMSIS-DAP response exceeds HID report size: %zu", response_length);
            debug_probe_free_sent_data(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID, response);
            continue;
        }

        memcpy(report, response, response_length);
        while (!tud_hid_n_ready(0)) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if (!tud_hid_n_report(0, 0, report, sizeof(report))) {
            ESP_LOGW(TAG, "Cannot send CMSIS-DAP HID report");
        }
        debug_probe_free_sent_data(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID, response);
    }
}

void cmsis_dap_v1_hid_start(void)
{
    static bool started;

    if (started) {
        return;
    }

    if (xTaskCreatePinnedToCore(cmsis_dap_v1_hid_task, "cmsis_dap_v1_hid", 4 * 1024, NULL,
                                DEBUG_PROBE_TASK_PRI - 1, NULL, NEXLINK_CPU_WIRELESS) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create CMSIS-DAP HID task");
        return;
    }

    started = true;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t request_length)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)request_length;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t buffer_size)
{
    if (instance != 0 || report_id != 0 || report_type != HID_REPORT_TYPE_OUTPUT ||
        buffer_size != CFG_TUD_HID_EP_BUFSIZE) {
        ESP_LOGW(TAG, "Invalid CMSIS-DAP HID report");
        return;
    }

    esp_err_t result = debug_probe_process_data(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V1_HID,
                                                buffer, buffer_size);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Cannot queue CMSIS-DAP HID request: %s", esp_err_to_name(result));
    }
}

