#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define NEXLINK_PAIR_CHANNEL 6

typedef enum { NEXLINK_PAIR_ROLE_RX = 0, NEXLINK_PAIR_ROLE_TX } nexlink_pair_role_t;
typedef enum { NEXLINK_PAIR_STATE_SEARCHING = 0, NEXLINK_PAIR_STATE_SUCCEEDED, NEXLINK_PAIR_STATE_TIMED_OUT } nexlink_pair_state_t;
typedef struct { uint8_t peer_mac[6]; uint8_t channel; uint8_t lmk[16]; } nexlink_pair_record_t;
typedef void (*nexlink_pairing_state_callback_t)(nexlink_pair_state_t state, void *context);

esp_err_t nexlink_pairing_load(nexlink_pair_record_t *record, bool *paired);
esp_err_t nexlink_pairing_save(const nexlink_pair_record_t *record);
esp_err_t nexlink_pairing_clear(void);
esp_err_t nexlink_pairing_request(nexlink_pair_role_t role);
bool nexlink_pairing_mode_is_requested(nexlink_pair_role_t role);
esp_err_t nexlink_pairing_start(nexlink_pair_role_t role, nexlink_pairing_state_callback_t state_callback, void *context);
