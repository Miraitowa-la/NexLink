#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <string.h>

#include "espnow_transport.h"
#include "nexlink_link.h"
#include "nexlink_task_config.h"

#define NEXLINK_LINK_MAGIC 0x4E4C
#define NEXLINK_LINK_VERSION 1
#define NEXLINK_LINK_HEARTBEAT_MS 500
#define NEXLINK_LINK_TIMEOUT_MS 3000

typedef enum {
    NEXLINK_LINK_MSG_HELLO = 1,
    NEXLINK_LINK_MSG_HELLO_ACK = 2,
    NEXLINK_LINK_MSG_HEARTBEAT = 3,
} nexlink_link_message_type_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
} nexlink_link_frame_t;

static uint8_t s_peer_mac[6];
static nexlink_link_state_callback_t s_state_callback;
static void *s_state_context;
static nexlink_link_state_t s_state;
static uint16_t s_next_sequence;
static TickType_t s_last_transmit;
static TickType_t s_last_receive;
static nexlink_link_payload_callback_t s_payload_callback;
static void *s_payload_context;

static void nexlink_link_set_state(nexlink_link_state_t state)
{
    if (state == s_state) {
        return;
    }
    s_state = state;
    ESP_LOGI("nexlink_link", "link %s", state == NEXLINK_LINK_STATE_CONNECTED ? "connected" : "waiting");
    if (s_state_callback) {
        s_state_callback(state, s_state_context);
    }
}

static esp_err_t nexlink_link_send(nexlink_link_message_type_t type)
{
    const nexlink_link_frame_t frame = {
        .magic = NEXLINK_LINK_MAGIC,
        .version = NEXLINK_LINK_VERSION,
        .type = type,
        .sequence = ++s_next_sequence,
    };
    return espnow_transport_send((const uint8_t *)&frame, sizeof(frame));
}

static void nexlink_link_handle_packet(const espnow_transport_packet_t *packet)
{
    if (memcmp(packet->source_mac, s_peer_mac, sizeof(s_peer_mac)) != 0) {
        return;
    }

    if (packet->length != sizeof(nexlink_link_frame_t)) {
        if (s_payload_callback) s_payload_callback(packet->data, packet->length, s_payload_context);
        return;
    }

    nexlink_link_frame_t frame;
    memcpy(&frame, packet->data, sizeof(frame));
    if (frame.magic != NEXLINK_LINK_MAGIC || frame.version != NEXLINK_LINK_VERSION) {
        return;
    }

    if (frame.type != NEXLINK_LINK_MSG_HELLO && frame.type != NEXLINK_LINK_MSG_HELLO_ACK &&
        frame.type != NEXLINK_LINK_MSG_HEARTBEAT) {
        return;
    }

    s_last_receive = xTaskGetTickCount();
    if (frame.type == NEXLINK_LINK_MSG_HELLO) {
        (void)nexlink_link_send(NEXLINK_LINK_MSG_HELLO_ACK);
    }
    nexlink_link_set_state(NEXLINK_LINK_STATE_CONNECTED);
}

esp_err_t nexlink_link_send_payload(const uint8_t *data, size_t length)
{
    return espnow_transport_send(data, length);
}

void nexlink_link_set_payload_callback(nexlink_link_payload_callback_t callback, void *context)
{
    s_payload_callback = callback;
    s_payload_context = context;
}

static void nexlink_link_task(void *argument)
{
    (void)argument;
    const TickType_t heartbeat_ticks = pdMS_TO_TICKS(NEXLINK_LINK_HEARTBEAT_MS);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(NEXLINK_LINK_TIMEOUT_MS);

    for (;;) {
        espnow_transport_packet_t packet;
        if (espnow_transport_receive(&packet, 100) == ESP_OK) {
            nexlink_link_handle_packet(&packet);
        }

        const TickType_t now = xTaskGetTickCount();
        if (now - s_last_transmit >= heartbeat_ticks) {
            const nexlink_link_message_type_t type = s_state == NEXLINK_LINK_STATE_CONNECTED
                                                         ? NEXLINK_LINK_MSG_HEARTBEAT
                                                         : NEXLINK_LINK_MSG_HELLO;
            if (nexlink_link_send(type) == ESP_OK) {
                s_last_transmit = now;
            }
        }

        if (s_state == NEXLINK_LINK_STATE_CONNECTED && now - s_last_receive >= timeout_ticks) {
            nexlink_link_set_state(NEXLINK_LINK_STATE_WAITING);
        }
    }
}

static esp_err_t nexlink_link_start(nexlink_link_state_callback_t state_callback, void *context)
{
    s_state_callback = state_callback;
    s_state_context = context;
    s_state = NEXLINK_LINK_STATE_WAITING;
    s_last_transmit = xTaskGetTickCount() - pdMS_TO_TICKS(NEXLINK_LINK_HEARTBEAT_MS);
    s_last_receive = xTaskGetTickCount();

    if (s_state_callback) {
        s_state_callback(NEXLINK_LINK_STATE_WAITING, s_state_context);
    }
    return xTaskCreatePinnedToCore(nexlink_link_task, "nexlink_link", 3072, NULL, 6, NULL,
                                   NEXLINK_CPU_WIRELESS) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t nexlink_link_init(const uint8_t peer_mac[6], uint8_t channel,
                            nexlink_link_state_callback_t state_callback, void *context)
{
    ESP_RETURN_ON_ERROR(espnow_transport_init(peer_mac, channel), "nexlink_link", "transport init failed");
    memcpy(s_peer_mac, peer_mac, sizeof(s_peer_mac));
    return nexlink_link_start(state_callback, context);
}

esp_err_t nexlink_link_init_encrypted(const uint8_t peer_mac[6], uint8_t channel, const uint8_t lmk[16],
                                      nexlink_link_state_callback_t state_callback, void *context)
{
    ESP_RETURN_ON_ERROR(espnow_transport_init_encrypted(peer_mac, channel, lmk), "nexlink_link", "encrypted transport init failed");
    memcpy(s_peer_mac, peer_mac, sizeof(s_peer_mac));
    return nexlink_link_start(state_callback, context);
}
