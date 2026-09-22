/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_device.h"

#include <stdio.h>
#include <string.h>

#include "bridge_target.h"
#include "cmsis_dap_v1_hid.h"
#include "cmsis_dap_v2_bulk.h"
#include "esp_cpu.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led.h"
#include "nexlink_task_config.h"
#include "sdkconfig.h"
#include "tusb.h"
#include "usb_layout.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_CDC_DESC_LEN)
#define MAC_BYTES 6
#define USB_DEVICE_MANUFACTURER_MAX_LEN 31
#define CDC_INSTANCE 0
#define CDC_RX_CHUNK_SIZE 64
#define CDC_TX_CHUNK_SIZE 64
#define CDC_UART_TO_USB_BUFFER_SIZE 2048
#define CDC_TX_TIMEOUT_MS 50

static tusb_desc_device_t s_device_descriptor = {
    .bLength = sizeof(s_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0210,
    // Composite device: each interface declares its own class (Vendor + HID).
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
#ifdef CFG_TUD_ENDPOINT0_SIZE
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
#else
    .bMaxPacketSize0 = CFG_TUD_ENDOINT0_SIZE,
#endif
    .idVendor = 0,
    .idProduct = 0,
    .bcdDevice = BCDDEVICE,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

#define TUD_CMSIS_DAP_V2_BULK_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize)                       \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, CMSIS_DAP_V2_BULK_IFACE_SUBCLASS, \
        CMSIS_DAP_V2_BULK_IFACE_PROTOCOL, _stridx, 7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK,        \
        U16_TO_U8S_LE(_epsize), 0, 7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

static uint8_t const s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE),
};

static uint8_t const s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, 0, 100),
    TUD_CMSIS_DAP_V2_BULK_DESCRIPTOR(ITF_NUM_CMSIS_DAP_V2_BULK, USB_STRID_CMSIS_DAP_V2_BULK, EPNUM_CMSIS_DAP_V2_BULK,
                                     0x80 | EPNUM_CMSIS_DAP_V2_BULK, CMSIS_DAP_V2_BULK_EPSIZE),
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_CMSIS_DAP_V1_HID, USB_STRID_CMSIS_DAP_V1_HID, HID_ITF_PROTOCOL_NONE,
                             sizeof(s_hid_report_descriptor), EPNUM_CMSIS_DAP_V1_HID, 0x80 | EPNUM_CMSIS_DAP_V1_HID,
                             CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_CONTROL, USB_STRID_CDC, 0x80 | EPNUM_CDC_NOTIFICATION, 8, EPNUM_CDC_DATA,
                       0x80 | EPNUM_CDC_DATA, CFG_TUD_CDC_EP_BUFSIZE),
};

static char s_serial_descriptor[MAC_BYTES * 2 + 1] = {'\0'};
static char s_manufacturer[USB_DEVICE_MANUFACTURER_MAX_LEN + 1] = "NexLink";

static char const *s_string_descriptors[] = {
    (const char[]){0x09, 0x04}, s_manufacturer, "CMSIS-DAP", s_serial_descriptor, "CMSIS-DAP v2",
    "CMSIS-DAP v1 HID",         "CDC UART",
};

static bridge_target_t *s_cdc_target;
static usb_device_frontend_t s_frontend;
static RingbufHandle_t s_cdc_uart_to_usb;
static SemaphoreHandle_t s_cdc_tx_requested;
static SemaphoreHandle_t s_cdc_tx_done;

static void cdc_target_rx(void *context, const uint8_t *data, size_t length) {
    (void)context;
    if (xRingbufferSend(s_cdc_uart_to_usb, data, length, 0) == pdTRUE) {
        led_cdc_rx_activity();
    } else {
        // The UART producer must never block the debug USB service.
    }
}

static bool cdc_map_line_coding(const cdc_line_coding_t *line_coding, uart_word_length_t *data_bits,
                                uart_parity_t *parity, uart_stop_bits_t *stop_bits) {
    switch (line_coding->data_bits) {
        case 5:
            *data_bits = UART_DATA_5_BITS;
            break;
        case 6:
            *data_bits = UART_DATA_6_BITS;
            break;
        case 7:
            *data_bits = UART_DATA_7_BITS;
            break;
        case 8:
            *data_bits = UART_DATA_8_BITS;
            break;
        default:
            return false;
    }
    switch (line_coding->parity) {
        case CDC_LINE_CODING_PARITY_NONE:
            *parity = UART_PARITY_DISABLE;
            break;
        case CDC_LINE_CODING_PARITY_ODD:
            *parity = UART_PARITY_ODD;
            break;
        case CDC_LINE_CODING_PARITY_EVEN:
            *parity = UART_PARITY_EVEN;
            break;
        default:
            return false;
    }
    switch (line_coding->stop_bits) {
        case CDC_LINE_CODING_STOP_BITS_1:
            *stop_bits = UART_STOP_BITS_1;
            break;
        case CDC_LINE_CODING_STOP_BITS_1_5:
            *stop_bits = UART_STOP_BITS_1_5;
            break;
        case CDC_LINE_CODING_STOP_BITS_2:
            *stop_bits = UART_STOP_BITS_2;
            break;
        default:
            return false;
    }
    return line_coding->bit_rate != 0;
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding) {
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    if (itf != CDC_INSTANCE || !line_coding) {
        return;
    }
    if (s_cdc_target && cdc_map_line_coding(line_coding, &data_bits, &parity, &stop_bits)) {
        bridge_target_set_line_format(s_cdc_target, line_coding->bit_rate, data_bits, parity, stop_bits);
    } else if (s_frontend.line_coding) {
        s_frontend.line_coding(line_coding->bit_rate, line_coding->data_bits, line_coding->parity,
                               line_coding->stop_bits, s_frontend.context);
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t buffer[CDC_RX_CHUNK_SIZE];
    if (itf != CDC_INSTANCE) {
        return;
    }
    while (tud_cdc_n_available(CDC_INSTANCE)) {
        const uint32_t length = tud_cdc_n_read(CDC_INSTANCE, buffer, sizeof(buffer));
        if (length > 0) {
            if (s_cdc_target) {
                if (bridge_target_write(s_cdc_target, buffer, length) == ESP_OK) {
                    led_cdc_tx_activity();
                }
            } else if (s_frontend.cdc_rx) {
                s_frontend.cdc_rx(buffer, length, s_frontend.context);
            }
        }
    }
}

void tud_cdc_tx_complete_cb(uint8_t itf) {
    if (itf == CDC_INSTANCE && xSemaphoreTake(s_cdc_tx_requested, 0) == pdTRUE) {
        xSemaphoreGive(s_cdc_tx_done);
    }
}

static void cdc_sender_task(void *parameter) {
    (void)parameter;
    uint8_t tx_buffer[CDC_TX_CHUNK_SIZE];
    for (;;) {
        size_t length = 0;
        uint8_t *data = xRingbufferReceiveUpTo(s_cdc_uart_to_usb, &length, portMAX_DELAY, sizeof(tx_buffer));
        if (!data) {
            continue;
        }
        memcpy(tx_buffer, data, length);
        vRingbufferReturnItem(s_cdc_uart_to_usb, data);

        for (size_t sent = 0; sent < length;) {
            xSemaphoreTake(s_cdc_tx_done, 0);
            xSemaphoreGive(s_cdc_tx_requested);
            const uint32_t written = tud_cdc_n_write(CDC_INSTANCE, tx_buffer + sent, length - sent);
            tud_cdc_n_write_flush(CDC_INSTANCE);
            if (written == 0 || xSemaphoreTake(s_cdc_tx_done, pdMS_TO_TICKS(CDC_TX_TIMEOUT_MS)) != pdTRUE) {
                xSemaphoreTake(s_cdc_tx_requested, 0);
                tud_cdc_n_write_clear(CDC_INSTANCE);
                break;
            }
            sent += written;
        }
    }
}

// Microsoft OS 1.0 signature string. Windows requests index 0xEE first, then
// obtains the WCID descriptor through VENDOR_REQUEST_MICROSOFT.
static uint16_t const s_ms_os_10_string_descriptor[] = {
    (TUSB_DESC_STRING << 8) | 18, 'M', 'S', 'F', 'T', '1', '0', '0', VENDOR_REQUEST_MICROSOFT,
};

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static uint8_t const s_bos_descriptor[] = {
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT),
};

uint8_t const *tud_descriptor_bos_cb(void) { return s_bos_descriptor; }

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return s_configuration_descriptor;
}

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&s_device_descriptor; }

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t const *tud_descriptor_string_cb(const uint8_t index, const uint16_t langid) {
    (void)langid;
    static uint16_t descriptor[32];
    uint8_t character_count;

    if (index == MS_OS_10_STRING_INDEX) {
        return s_ms_os_10_string_descriptor;
    }

    if (index == 0) {
        memcpy(&descriptor[1], s_string_descriptors[0], 2);
        character_count = 1;
    } else {
        if (index >= sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0])) {
            return NULL;
        }

        const char *string = s_string_descriptors[index];
        character_count = strlen(string);
        if (character_count > 31) {
            character_count = 31;
        }

        for (uint8_t i = 0; i < character_count; i++) {
            descriptor[1 + i] = string[i];
        }
    }

    descriptor[0] = (TUSB_DESC_STRING << 8) | (2 * character_count + 2);
    return descriptor;
}

esp_err_t usb_device_init_serial_number(void) {
    uint8_t mac[MAC_BYTES] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK) {
        return err;
    }

    snprintf(s_serial_descriptor, sizeof(s_serial_descriptor), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    return ESP_OK;
}

static void usb_device_task(void *parameter) {
    (void)parameter;
    for (;;) {
        tud_task();
        vTaskDelay(1);
    }
}

esp_err_t usb_device_start(const usb_device_config_t *config, bridge_target_t *cdc_target) {
    static bool started;
    if (!config || !config->manufacturer || (!cdc_target && !s_frontend.cdc_rx)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (started) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t manufacturer_length = strlen(config->manufacturer);
    if (manufacturer_length > USB_DEVICE_MANUFACTURER_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    s_device_descriptor.idVendor = config->vendor_id;
    s_device_descriptor.idProduct = config->product_id;
    memcpy(s_manufacturer, config->manufacturer, manufacturer_length + 1);

    s_cdc_uart_to_usb = xRingbufferCreate(CDC_UART_TO_USB_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    s_cdc_tx_requested = xSemaphoreCreateBinary();
    s_cdc_tx_done = xSemaphoreCreateBinary();
    if (!s_cdc_uart_to_usb || !s_cdc_tx_requested || !s_cdc_tx_done) {
        return ESP_ERR_NO_MEM;
    }

    tusb_init();
    cmsis_dap_v2_bulk_start();
    cmsis_dap_v1_hid_start();
    if (xTaskCreatePinnedToCore(usb_device_task, "usb_device_task", 4 * 1024, NULL, 5, NULL, NEXLINK_CPU_WIRELESS) !=
        pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(cdc_sender_task, "cdc_sender", 4 * 1024, NULL, 5, NULL, NEXLINK_CPU_WIRELESS) !=
        pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_cdc_target = cdc_target;
    bridge_target_set_rx_callback(cdc_target, cdc_target_rx, NULL);
    started = true;
    return ESP_OK;
}

esp_err_t usb_device_start_frontend(const usb_device_config_t *config, const usb_device_frontend_t *frontend) {
    if (!frontend || !frontend->cdc_rx) return ESP_ERR_INVALID_ARG;
    s_frontend = *frontend;
    return usb_device_start(config, NULL);
}

esp_err_t usb_device_frontend_write(const uint8_t *data, size_t length) {
    if (!data || length == 0 || !s_frontend.cdc_rx || !s_cdc_uart_to_usb) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xRingbufferSend(s_cdc_uart_to_usb, data, length, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    led_cdc_rx_activity();
    return ESP_OK;
}
