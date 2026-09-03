#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define ESPNOW_TRANSPORT_MAX_PAYLOAD 128

typedef struct {
    uint8_t source_mac[6];
    size_t length;
    uint8_t data[ESPNOW_TRANSPORT_MAX_PAYLOAD];
} espnow_transport_packet_t;

esp_err_t espnow_transport_init(const uint8_t peer_mac[6], uint8_t channel);
esp_err_t espnow_transport_init_encrypted(const uint8_t peer_mac[6], uint8_t channel, const uint8_t lmk[16]);
esp_err_t espnow_transport_init_broadcast(uint8_t channel);
esp_err_t espnow_transport_get_local_mac(uint8_t mac[6]);
esp_err_t espnow_transport_send(const uint8_t *data, size_t length);
esp_err_t espnow_transport_receive(espnow_transport_packet_t *packet, uint32_t timeout_ms);
