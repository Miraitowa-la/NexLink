#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "debug_probe.h"
#include "nexlink_tx_frontend.h"

static QueueHandle_t s_requests;
static uint16_t s_sequence;

static void queue_frame(nexlink_protocol_message_type_t type, const uint8_t *data, size_t length)
{
    if (!s_requests || length > NEXLINK_PROTOCOL_MAX_PAYLOAD) return;
    nexlink_protocol_frame_t frame = { .type = type, .sequence = ++s_sequence, .length = length };
    memcpy(frame.payload, data, length);
    (void)xQueueSend(s_requests, &frame, 0);
}

static void dap_request(debug_probe_transport_t transport, const uint8_t *data, size_t length, void *context)
{
    (void)context;
    if (length + 1 > NEXLINK_PROTOCOL_MAX_PAYLOAD) return;
    uint8_t payload[NEXLINK_PROTOCOL_MAX_PAYLOAD];
    payload[0] = (uint8_t)transport;
    memcpy(payload + 1, data, length);
    queue_frame(NEXLINK_PROTOCOL_MSG_DAP_REQUEST, payload, length + 1);
}

static void cdc_rx(const uint8_t *data, size_t length, void *context)
{
    (void)context;
    queue_frame(NEXLINK_PROTOCOL_MSG_CDC_DATA_TX, data, length);
}

static void line_coding(uint32_t bit_rate, uint8_t data_bits, uint8_t parity, uint8_t stop_bits, void *context)
{
    (void)context;
    const uint8_t payload[] = { (uint8_t)bit_rate, (uint8_t)(bit_rate >> 8), (uint8_t)(bit_rate >> 16),
                                (uint8_t)(bit_rate >> 24), data_bits, parity, stop_bits };
    queue_frame(NEXLINK_PROTOCOL_MSG_CDC_LINE_CODING, payload, sizeof(payload));
}

esp_err_t nexlink_tx_frontend_init(const usb_device_config_t *usb_config)
{
    s_requests = xQueueCreate(8, sizeof(nexlink_protocol_frame_t));
    if (!s_requests) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(debug_probe_frontend_init(dap_request, NULL), "nexlink_tx", "DAP frontend failed");
    const usb_device_frontend_t frontend = { .cdc_rx = cdc_rx, .line_coding = line_coding };
    ESP_RETURN_ON_ERROR(usb_device_init_serial_number(), "nexlink_tx", "serial init failed");
    return usb_device_start_frontend(usb_config, &frontend);
}

esp_err_t nexlink_tx_frontend_take_request(nexlink_protocol_frame_t *frame, uint32_t timeout_ms)
{
    return frame && xQueueReceive(s_requests, frame, pdMS_TO_TICKS(timeout_ms)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t nexlink_tx_frontend_submit_cdc_data(const uint8_t *data, size_t length)
{
    return usb_device_frontend_write(data, length);
}
