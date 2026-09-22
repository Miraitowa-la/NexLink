#include <string.h>
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "debug_probe.h"
#include "bridge_target.h"
#include "led.h"
#include "nexlink_dap_bridge.h"
#include "nexlink_link.h"
#include "nexlink_protocol.h"
#include "nexlink_task_config.h"
#include "nexlink_tx_frontend.h"

#define NEXLINK_DAP_RETRY_COUNT 3
#define NEXLINK_DAP_RESPONSE_TIMEOUT_MS 1000
#define NEXLINK_CDC_WINDOW 4
static QueueHandle_t s_rx_frames;
static SemaphoreHandle_t s_tx_response;
static uint16_t s_waiting_sequence;
static bool s_cached_response_valid;
static nexlink_protocol_frame_t s_cached_response;
static SemaphoreHandle_t s_cdc_credit;
static bridge_target_t *s_cdc_target;

static void payload_received(const uint8_t *data, size_t length, void *context)
{
    (void)context; nexlink_protocol_frame_t frame;
    if (nexlink_protocol_decode(data, length, &frame) != ESP_OK) return;
    if (frame.type == NEXLINK_PROTOCOL_MSG_CDC_DATA_RX) {
        (void)nexlink_tx_frontend_submit_cdc_data(frame.payload, frame.length);
    } else if (frame.type == NEXLINK_PROTOCOL_MSG_CDC_FLOW && s_cdc_credit) {
        (void)xSemaphoreGive(s_cdc_credit);
    } else if (frame.type == NEXLINK_PROTOCOL_MSG_DAP_RESPONSE && frame.sequence == s_waiting_sequence && frame.length > 1 && frame.payload[0] < DEBUG_PROBE_TRANSPORT_COUNT) {
        (void)debug_probe_submit_response((debug_probe_transport_t)frame.payload[0], frame.payload + 1, frame.length - 1);
        xSemaphoreGive(s_tx_response);
    } else if (s_rx_frames) {
        (void)xQueueSend(s_rx_frames, &frame, 0);
    }
}

static esp_err_t send_frame(const nexlink_protocol_frame_t *frame)
{
    uint8_t data[228]; size_t length = sizeof(data);
    ESP_RETURN_ON_ERROR(nexlink_protocol_encode(frame, data, &length), "nexlink_dap", "encode failed");
    const esp_err_t result = nexlink_link_send_payload(data, length);
    if (result == ESP_OK) {
        if (frame->type == NEXLINK_PROTOCOL_MSG_CDC_DATA_TX) {
            led_cdc_tx_activity();
        } else if (frame->type == NEXLINK_PROTOCOL_MSG_CDC_DATA_RX) {
            led_cdc_rx_activity();
        }
    }
    return result;
}

static void tx_task(void *context)
{
    (void)context;
    for (;;) { nexlink_protocol_frame_t request;
        if (nexlink_tx_frontend_take_request(&request, portMAX_DELAY) != ESP_OK) continue;
        if (request.type == NEXLINK_PROTOCOL_MSG_CDC_DATA_TX) {
            if (xSemaphoreTake(s_cdc_credit, pdMS_TO_TICKS(NEXLINK_DAP_RESPONSE_TIMEOUT_MS)) == pdTRUE) (void)send_frame(&request);
            continue;
        }
        if (request.type != NEXLINK_PROTOCOL_MSG_DAP_REQUEST) { (void)send_frame(&request); continue; }
        s_waiting_sequence = request.sequence;
        for (int retry = 0; retry < NEXLINK_DAP_RETRY_COUNT; ++retry) {
            xSemaphoreTake(s_tx_response, 0);
            if (send_frame(&request) == ESP_OK && xSemaphoreTake(s_tx_response, pdMS_TO_TICKS(NEXLINK_DAP_RESPONSE_TIMEOUT_MS)) == pdTRUE) break;
        }
        s_waiting_sequence = 0;
    }
}

static void grant_cdc_credit(void)
{
    const nexlink_protocol_frame_t frame = { .type = NEXLINK_PROTOCOL_MSG_CDC_FLOW };
    (void)send_frame(&frame);
}

static void target_uart_rx(void *context, const uint8_t *data, size_t length)
{
    (void)context;
    while (length > 0) {
        const size_t chunk = length > NEXLINK_PROTOCOL_MAX_PAYLOAD ? NEXLINK_PROTOCOL_MAX_PAYLOAD : length;
        nexlink_protocol_frame_t frame = { .type = NEXLINK_PROTOCOL_MSG_CDC_DATA_RX, .length = chunk };
        memcpy(frame.payload, data, chunk);
        (void)send_frame(&frame);
        data += chunk;
        length -= chunk;
    }
}

static void process_cdc(const nexlink_protocol_frame_t *request)
{
    if (!s_cdc_target) return;
    if (request->type == NEXLINK_PROTOCOL_MSG_CDC_DATA_TX) {
        if (bridge_target_write(s_cdc_target, request->payload, request->length) == ESP_OK) {
            led_cdc_tx_activity();
        }
        grant_cdc_credit();
    } else if (request->type == NEXLINK_PROTOCOL_MSG_CDC_LINE_CODING && request->length == 7) {
        const uint32_t baud = (uint32_t)request->payload[0] | ((uint32_t)request->payload[1] << 8) |
                              ((uint32_t)request->payload[2] << 16) | ((uint32_t)request->payload[3] << 24);
        const uart_word_length_t bits = request->payload[4] == 7 ? UART_DATA_7_BITS : UART_DATA_8_BITS;
        const uart_parity_t parity = request->payload[5] == 1 ? UART_PARITY_ODD : request->payload[5] == 2 ? UART_PARITY_EVEN : UART_PARITY_DISABLE;
        const uart_stop_bits_t stop = request->payload[6] == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
        (void)bridge_target_set_line_format(s_cdc_target, baud, bits, parity, stop);
    }
}

static void rx_task(void *context)
{
    (void)context;
    for (;;) { nexlink_protocol_frame_t request;
        if (xQueueReceive(s_rx_frames, &request, portMAX_DELAY) != pdTRUE) continue;
        if (request.type == NEXLINK_PROTOCOL_MSG_CDC_DATA_TX || request.type == NEXLINK_PROTOCOL_MSG_CDC_LINE_CODING) { process_cdc(&request); continue; }
        if (request.type != NEXLINK_PROTOCOL_MSG_DAP_REQUEST || request.length < 2 || request.payload[0] >= DEBUG_PROBE_TRANSPORT_COUNT) continue;
        if (s_cached_response_valid && request.sequence == s_cached_response.sequence) {
            (void)send_frame(&s_cached_response);
            continue;
        }
        const debug_probe_transport_t transport = (debug_probe_transport_t)request.payload[0];
        if (debug_probe_process_data(transport, request.payload + 1, request.length - 1) != ESP_OK) continue;
        size_t response_length = 0; uint8_t *response = debug_probe_get_data_to_send(transport, &response_length, pdMS_TO_TICKS(NEXLINK_DAP_RESPONSE_TIMEOUT_MS));
        if (!response || response_length + 1 > NEXLINK_PROTOCOL_MAX_PAYLOAD) continue;
        nexlink_protocol_frame_t frame = { .type = NEXLINK_PROTOCOL_MSG_DAP_RESPONSE, .sequence = request.sequence, .length = response_length + 1 };
        frame.payload[0] = (uint8_t)transport; memcpy(frame.payload + 1, response, response_length);
        debug_probe_free_sent_data(transport, response);
        s_cached_response = frame;
        s_cached_response_valid = true;
        (void)send_frame(&frame);
    }
}

static esp_err_t start_bridge(TaskFunction_t task, const char *name)
{
    s_rx_frames = xQueueCreate(8, sizeof(nexlink_protocol_frame_t));
    if (!s_rx_frames) return ESP_ERR_NO_MEM;
    nexlink_link_set_payload_callback(payload_received, NULL);
    return xTaskCreatePinnedToCore(task, name, 4096, NULL, 6, NULL, NEXLINK_CPU_TARGET) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t nexlink_dap_bridge_start_tx(void) { s_tx_response = xSemaphoreCreateBinary(); s_cdc_credit = xSemaphoreCreateCounting(NEXLINK_CDC_WINDOW, NEXLINK_CDC_WINDOW); return s_tx_response && s_cdc_credit ? start_bridge(tx_task, "nexlink_dap_tx") : ESP_ERR_NO_MEM; }
esp_err_t nexlink_dap_bridge_start_rx(void) { return start_bridge(rx_task, "nexlink_dap_rx"); }
void nexlink_dap_bridge_set_cdc_target(bridge_target_t *target) { s_cdc_target = target; if (target) bridge_target_set_rx_callback(target, target_uart_rx, NULL); }
