#pragma once
#include "esp_err.h"
typedef struct bridge_target bridge_target_t;
esp_err_t nexlink_dap_bridge_start_tx(void);
esp_err_t nexlink_dap_bridge_start_rx(void);
void nexlink_dap_bridge_set_cdc_target(bridge_target_t *target);
