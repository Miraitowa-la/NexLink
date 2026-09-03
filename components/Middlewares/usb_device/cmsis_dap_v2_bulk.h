/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#define CMSIS_DAP_V2_BULK_EPSIZE          64 /* full speed */
#define CMSIS_DAP_V2_BULK_IFACE_SUBCLASS  0x00
#define CMSIS_DAP_V2_BULK_IFACE_PROTOCOL  0x00

#define VENDOR_REQUEST_MICROSOFT 0x20  // Can be any value between 0x20 and 0xFF
#define MS_OS_20_DESC_LEN  0xB2

// Microsoft OS descriptor request indices. Both descriptor versions use the
// same vendor request advertised by the device.
#define MS_OS_10_STRING_INDEX          0xEE
#define MS_OS_10_COMPAT_ID_INDEX       0x0004
#define MS_OS_10_COMPAT_ID_DESC_LEN    0x28
#define MS_OS_20_DESCRIPTOR_INDEX      0x0007

void cmsis_dap_v2_bulk_start(void);


