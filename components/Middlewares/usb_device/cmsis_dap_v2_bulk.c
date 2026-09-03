/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "tusb_config.h"
#include "esp_timer.h"

#include "cmsis_dap_v2_bulk.h"
#include "debug_probe.h"
#include "nexlink_task_config.h"
#include "usb_layout.h"

static const char *TAG = "cmsis_dap_v2_bulk";
#define USB_BUSY_TOUT_US    100000 /* 100ms */

typedef struct {
    uint8_t ep_in;
    uint8_t ep_out;
    TaskHandle_t usb_tx_task_handle;
    uint8_t epout_buf[CMSIS_DAP_V2_BULK_EPSIZE];  /* Endpoint Transfer buffer */
} cmsis_dap_v2_bulk_interface_t;

static cmsis_dap_v2_bulk_interface_t s_cmsis_dap_v2_bulk_itf;
static const uint8_t s_rhport = 0;

static void cmsis_dap_v2_bulk_init(void)
{
    ESP_LOGD(TAG, "%s", __func__);

    memset(&s_cmsis_dap_v2_bulk_itf, 0x00, sizeof(s_cmsis_dap_v2_bulk_itf));
}

static void cmsis_dap_v2_bulk_reset(uint8_t rhport)
{
    ESP_LOGD(TAG, "%s", __func__);

    s_cmsis_dap_v2_bulk_itf.ep_in = 0;
    s_cmsis_dap_v2_bulk_itf.ep_out = 0;
    /* do not reset the FreeRTOS handlers */
}

static uint16_t cmsis_dap_v2_bulk_open(uint8_t rhport, tusb_desc_interface_t const *desc_intf, uint16_t max_len)
{
    ESP_LOGD(TAG, "%s", __func__);

    TU_VERIFY(ITF_NUM_CMSIS_DAP_V2_BULK == desc_intf->bInterfaceNumber &&
              TUSB_CLASS_VENDOR_SPECIFIC == desc_intf->bInterfaceClass &&
              CMSIS_DAP_V2_BULK_IFACE_SUBCLASS == desc_intf->bInterfaceSubClass &&
              CMSIS_DAP_V2_BULK_IFACE_PROTOCOL == desc_intf->bInterfaceProtocol &&
              USB_STRID_CMSIS_DAP_V2_BULK == desc_intf->iInterface, 0);

    uint8_t const *p_desc = tu_desc_next(desc_intf);
    cmsis_dap_v2_bulk_interface_t *p_dap = &s_cmsis_dap_v2_bulk_itf;

    // Open endpoint pair with usbd helper
    usbd_open_edpt_pair(rhport, p_desc, desc_intf->bNumEndpoints, TUSB_XFER_BULK, &p_dap->ep_out, &p_dap->ep_in);

    ESP_LOGI(TAG, "EP_OUT:0x%x EP_IN:0x%x", p_dap->ep_out, p_dap->ep_in);

    p_desc += desc_intf->bNumEndpoints * sizeof(tusb_desc_endpoint_t);

    // Prepare for incoming data
    if (p_dap->ep_out) {
        usbd_edpt_xfer(rhport, p_dap->ep_out, p_dap->epout_buf, sizeof(p_dap->epout_buf));
    }

    return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_intf);
}

static const uint8_t desc_ms_os_20[] = {
    // Microsoft OS 2.0 Descriptor
    // Values are taken from https://github.com/hathach/tinyusb/tree/master/examples/device/webusb_serial
    //
    // Set header: length, type, windows version, total length
    // 0x000A = size of the header
    // MS_OS_20_SET_HEADER_DESCRIPTOR = 0x00 (descriptor type)
    // 0x06030000 = Windows version 6.3 (Windows 8.1 and later)
    // MS_OS_20_DESC_LEN = total length of all descriptors
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header
    // 0x0008 = size of the header
    // MS_OS_20_SUBSET_HEADER_CONFIGURATION = 0x01 (descriptor type)
    // 0 = configuration index (first configuration)
    // 0 = reserved
    // MS_OS_20_DESC_LEN - 0x0A = remaining length (total - header length)
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

    // Function Subset header
    // 0x0008 = size of the header
    // MS_OS_20_SUBSET_HEADER_FUNCTION = 0x02 (descriptor type)
    // ITF_NUM_CMSIS_DAP_V2_BULK = CMSIS-DAP v2 Bulk interface number
    // 0 = reserved
    // MS_OS_20_DESC_LEN - 0x0A - 0x08 = remaining length (total - header - config subset)
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_CMSIS_DAP_V2_BULK, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

    // MS OS 2.0 Compatible ID descriptor
    // 0x0014 = size of the descriptor
    // MS_OS_20_FEATURE_COMPATBLE_ID = 0x03 (descriptor type)
    // 'WINUSB' = compatible ID string
    // 8 bytes of zeros = sub-compatible ID (not used)
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

    // MS OS 2.0 Registry property descriptor
    // length = remaining bytes (total - all previous sections)
    // MS_OS_20_FEATURE_REG_PROPERTY = 0x04 (descriptor type)
    // 0x0007 = REG_MULTI_SZ (property data type)
    // 0x002A = 42 bytes for property name length ("DeviceInterfaceGUIDs" in UTF-16)
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08 - 0x08 - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
    'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
    // 0x0050 = 80 bytes for property data length (GUID string in UTF-16)
    U16_TO_U8S_LE(0x0050), // wPropertyDataLength
    // Property data: GUID "{80869ad8-c37f-476a-a6b4-ae241c30a473}" in UTF-16 format
    // Each character is followed by 0x00 for UTF-16 encoding
    '{', 0x00, '8', 0x00, '0', 0x00, '8', 0x00, '6', 0x00, '9', 0x00, 'a', 0x00, 'd', 0x00, '8', 0x00, '-', 0x00,
    'c', 0x00, '3', 0x00, '7', 0x00, 'f', 0x00, '-', 0x00, '4', 0x00, '7', 0x00, '6', 0x00, 'a', 0x00, '-', 0x00,
    'a', 0x00, '6', 0x00, 'b', 0x00, '4', 0x00, '-', 0x00, 'a', 0x00, 'e', 0x00, '2', 0x00, '4', 0x00, '1', 0x00,
    'c', 0x00, '3', 0x00, '0', 0x00, 'a', 0x00, '4', 0x00, '7', 0x00, '3', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

// Microsoft OS 1.0 Extended Compatible ID descriptor. It is the WCID fallback
// for Windows 7 and older Windows 10 installations. Only the Bulk interface is
// associated with WinUSB; the CMSIS-DAP v1 HID interface keeps the inbox driver.
static const uint8_t desc_ms_os_10_compat_id[] = {
    U32_TO_U8S_LE(MS_OS_10_COMPAT_ID_DESC_LEN), U16_TO_U8S_LE(0x0100),
    U16_TO_U8S_LE(MS_OS_10_COMPAT_ID_INDEX), 1,
    0, 0, 0, 0, 0, 0, 0,
    ITF_NUM_CMSIS_DAP_V2_BULK, 1,
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_10_compat_id) == MS_OS_10_COMPAT_ID_DESC_LEN,
                 "Incorrect MS OS 1.0 descriptor size");

bool tud_vendor_control_xfer_cb(const uint8_t rhport, const uint8_t stage, tusb_control_request_t const *request)
{
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) {
        return true;
    }

    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR ||
        request->bRequest != VENDOR_REQUEST_MICROSOFT ||
        request->bmRequestType_bit.direction != TUSB_DIR_IN) {
        return false;
    }

    switch (request->wIndex) {
    case MS_OS_10_COMPAT_ID_INDEX:
        return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_10_compat_id,
                                sizeof(desc_ms_os_10_compat_id));
    case MS_OS_20_DESCRIPTOR_INDEX:
        return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_20,
                                sizeof(desc_ms_os_20));
    default:
        return false;
    }
}

static bool cmsis_dap_v2_bulk_transfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buf, uint16_t total_bytes)
{
    uint64_t end = esp_timer_get_time() + USB_BUSY_TOUT_US;

    while (usbd_edpt_busy(rhport, ep_addr)) {
        if (esp_timer_get_time() > end) {
            ESP_LOGE(TAG, "usbd_edpt_busy timeout!");
            return false;
        }
    }

    return usbd_edpt_xfer(s_rhport, ep_addr, buf, total_bytes);
}

static bool cmsis_dap_v2_bulk_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    TU_VERIFY(xferred_bytes > 0 && xferred_bytes <= 64, false);
    TU_VERIFY(ep_addr == s_cmsis_dap_v2_bulk_itf.ep_out || ep_addr == s_cmsis_dap_v2_bulk_itf.ep_in, false);

    const uint8_t ep_dir = tu_edpt_dir(ep_addr);

    ESP_LOGD(TAG, "%s xfer from ep:%x recvd:%" PRId32 " bytes", __func__, ep_addr, xferred_bytes);

    if (ep_dir == TUSB_DIR_IN) {
        /* nothing to do for now */
        return true;
    } else if (ep_dir == TUSB_DIR_OUT) {
        esp_err_t process_result = debug_probe_process_data(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V2_BULK,
                                                            s_cmsis_dap_v2_bulk_itf.epout_buf, xferred_bytes);
        if (process_result != ESP_OK) {
            ESP_LOGW(TAG, "Debug probe failed to process data: %s", esp_err_to_name(process_result));
            abort();
        }

        // Prepare for next incoming data
        if (!cmsis_dap_v2_bulk_transfer(rhport, ep_addr, s_cmsis_dap_v2_bulk_itf.epout_buf,
                                        CMSIS_DAP_V2_BULK_EPSIZE)) {
            ESP_LOGE(TAG, "USB transfer error on EP:%x", ep_addr);
            abort();
        }

        return true;
    }

    return false;
}

static void usb_send_task(void *pvParameters)
{
    size_t total_bytes;

    ESP_LOGI(TAG, "usb_send_task is ready!");

    for (;;) {
        uint8_t *buf_copy = debug_probe_get_data_to_send(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V2_BULK,
                                                          &total_bytes, portMAX_DELAY);

        if (!cmsis_dap_v2_bulk_transfer(s_rhport, s_cmsis_dap_v2_bulk_itf.ep_in, buf_copy, total_bytes)) {
            ESP_LOGE(TAG, "USB transfer error on EP:%x", s_cmsis_dap_v2_bulk_itf.ep_in);
            abort();
        }

        debug_probe_free_sent_data(DEBUG_PROBE_TRANSPORT_CMSIS_DAP_V2_BULK, buf_copy);
    }

    vTaskDelete(NULL);
}

void cmsis_dap_v2_bulk_start(void)
{
    ESP_LOGD(TAG, "%s", __func__);

    static bool init = false;

    if (!init) {
        if (xTaskCreatePinnedToCore(usb_send_task,
                                    "usb_send_task",
                                    4 * 1024,
                                    NULL,
                                    DEBUG_PROBE_TASK_PRI - 1,
                                    &s_cmsis_dap_v2_bulk_itf.usb_tx_task_handle,
                                    NEXLINK_CPU_WIRELESS) != pdPASS) {
            ESP_LOGE(TAG, "Cannot create USB send task!");
            abort();
        }

        init = true;
    }
}

const usbd_class_driver_t s_cmsis_dap_v2_bulk_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "CMSIS-DAP",
#endif
    .init = cmsis_dap_v2_bulk_init,
    .reset = cmsis_dap_v2_bulk_reset,
    .open = cmsis_dap_v2_bulk_open,
    .control_xfer_cb = tud_vendor_control_xfer_cb,
    .xfer_cb = cmsis_dap_v2_bulk_xfer_cb,
    .sof = NULL,
};

const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    ESP_LOGD(TAG, "%s", __func__);

    *driver_count = 1;
    return &s_cmsis_dap_v2_bulk_driver;
}
