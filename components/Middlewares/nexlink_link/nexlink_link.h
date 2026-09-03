#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    NEXLINK_LINK_STATE_WAITING = 0,
    NEXLINK_LINK_STATE_CONNECTED,
} nexlink_link_state_t;

typedef void (*nexlink_link_state_callback_t)(nexlink_link_state_t state, void *context);
typedef void (*nexlink_link_payload_callback_t)(const uint8_t *data, size_t length, void *context);

/** Initialize the ESP-NOW transport and P1 handshake/heartbeat state machine. */
esp_err_t nexlink_link_init(const uint8_t peer_mac[6], uint8_t channel,
                            nexlink_link_state_callback_t state_callback, void *context);
esp_err_t nexlink_link_init_encrypted(const uint8_t peer_mac[6], uint8_t channel, const uint8_t lmk[16],
                                      nexlink_link_state_callback_t state_callback, void *context);
esp_err_t nexlink_link_send_payload(const uint8_t *data, size_t length);
void nexlink_link_set_payload_callback(nexlink_link_payload_callback_t callback, void *context);
