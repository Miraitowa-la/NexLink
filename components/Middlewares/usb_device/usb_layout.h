/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// USB interface numbers
enum {
    ITF_NUM_CMSIS_DAP_V2_BULK = 0,
    ITF_NUM_CMSIS_DAP_V1_HID,
    ITF_NUM_CDC_CONTROL,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL,
};

// USB endpoint numbers
#define EPNUM_CMSIS_DAP_V2_BULK    3
#define EPNUM_CMSIS_DAP_V1_HID     1
#define EPNUM_CDC_DATA              2
#define EPNUM_CDC_NOTIFICATION      4

// USB string descriptor indices
#define USB_STRID_CMSIS_DAP_V2_BULK 4
#define USB_STRID_CMSIS_DAP_V1_HID  5
#define USB_STRID_CDC               6

