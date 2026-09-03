#include <string.h>
#include "nexlink_protocol.h"

#define NEXLINK_PROTOCOL_MAGIC 0x4E4C
#define NEXLINK_PROTOCOL_VERSION 1

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    uint16_t length;
} nexlink_protocol_header_t;

esp_err_t nexlink_protocol_encode(const nexlink_protocol_frame_t *frame, uint8_t *data, size_t *length)
{
    if (!frame || !data || !length || frame->length > NEXLINK_PROTOCOL_MAX_PAYLOAD ||
        *length < sizeof(nexlink_protocol_header_t) + frame->length) return ESP_ERR_INVALID_ARG;
    const nexlink_protocol_header_t header = { NEXLINK_PROTOCOL_MAGIC, NEXLINK_PROTOCOL_VERSION,
                                                (uint8_t)frame->type, frame->sequence, frame->length };
    memcpy(data, &header, sizeof(header));
    memcpy(data + sizeof(header), frame->payload, frame->length);
    *length = sizeof(header) + frame->length;
    return ESP_OK;
}

esp_err_t nexlink_protocol_decode(const uint8_t *data, size_t length, nexlink_protocol_frame_t *frame)
{
    if (!data || !frame || length < sizeof(nexlink_protocol_header_t)) return ESP_ERR_INVALID_ARG;
    nexlink_protocol_header_t header;
    memcpy(&header, data, sizeof(header));
    if (header.magic != NEXLINK_PROTOCOL_MAGIC || header.version != NEXLINK_PROTOCOL_VERSION ||
        header.type == 0 || header.type > NEXLINK_PROTOCOL_MSG_ERROR ||
        header.length > NEXLINK_PROTOCOL_MAX_PAYLOAD || length != sizeof(header) + header.length) return ESP_ERR_INVALID_SIZE;
    frame->type = (nexlink_protocol_message_type_t)header.type;
    frame->sequence = header.sequence;
    frame->length = header.length;
    memcpy(frame->payload, data + sizeof(header), header.length);
    return ESP_OK;
}
