#pragma once

#include "nexlink_protocol.h"
#include "usb_device.h"

esp_err_t nexlink_tx_frontend_init(const usb_device_config_t *usb_config);
esp_err_t nexlink_tx_frontend_take_request(nexlink_protocol_frame_t *frame, uint32_t timeout_ms);
esp_err_t nexlink_tx_frontend_submit_cdc_data(const uint8_t *data, size_t length);
