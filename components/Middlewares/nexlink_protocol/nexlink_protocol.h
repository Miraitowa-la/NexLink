#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define NEXLINK_PROTOCOL_MAX_PAYLOAD 220

typedef enum {
    NEXLINK_PROTOCOL_MSG_DAP_REQUEST = 1,
    NEXLINK_PROTOCOL_MSG_DAP_RESPONSE,
    NEXLINK_PROTOCOL_MSG_CDC_DATA_TX,
    NEXLINK_PROTOCOL_MSG_CDC_DATA_RX,
    NEXLINK_PROTOCOL_MSG_CDC_LINE_CODING,
    NEXLINK_PROTOCOL_MSG_CDC_CONTROL_LINE,
    NEXLINK_PROTOCOL_MSG_CDC_FLOW,
    NEXLINK_PROTOCOL_MSG_ERROR,
} nexlink_protocol_message_type_t;

typedef struct {
    nexlink_protocol_message_type_t type;
    uint16_t sequence;
    uint16_t length;
    uint8_t payload[NEXLINK_PROTOCOL_MAX_PAYLOAD];
} nexlink_protocol_frame_t;

esp_err_t nexlink_protocol_encode(const nexlink_protocol_frame_t *frame, uint8_t *data, size_t *length);
esp_err_t nexlink_protocol_decode(const uint8_t *data, size_t length, nexlink_protocol_frame_t *frame);
