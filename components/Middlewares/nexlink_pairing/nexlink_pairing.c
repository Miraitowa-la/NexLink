#include "nexlink_pairing.h"

#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "espnow_transport.h"
#include "nexlink_task_config.h"

#define NEXLINK_PAIR_NAMESPACE "nexlink_pair"
#define NEXLINK_PAIR_RECORD_KEY "record"
#define NEXLINK_PAIR_REQUEST_KEY "request"
#define NEXLINK_PAIR_MAGIC 0x4E50
#define NEXLINK_PAIR_VERSION 1
#define NEXLINK_PAIR_TIMEOUT_MS 30000
#define NEXLINK_PAIR_OFFER_PERIOD_MS 500

typedef enum { NEXLINK_PAIR_MSG_OFFER = 1, NEXLINK_PAIR_MSG_ACCEPT = 2 } nexlink_pair_message_type_t;
typedef struct __attribute__((packed)) { uint16_t magic; uint8_t version; uint8_t type; uint32_t nonce; uint8_t lmk[16]; } nexlink_pair_offer_t;
typedef struct __attribute__((packed)) { uint16_t magic; uint8_t version; uint8_t type; uint32_t nonce; } nexlink_pair_accept_t;

static nexlink_pair_role_t s_role;
static nexlink_pairing_state_callback_t s_state_callback;
static void *s_state_context;
static nexlink_pair_offer_t s_offer;

static esp_err_t pairing_open(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), "nexlink_pair", "NVS erase failed");
        result = nvs_flash_init();
    }
    return result == ESP_OK ? nvs_open(NEXLINK_PAIR_NAMESPACE, mode, handle) : result;
}

static esp_err_t write_request(uint8_t value)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(pairing_open(NVS_READWRITE, &handle), "nexlink_pair", "NVS open failed");
    esp_err_t result = value ? nvs_set_u8(handle, NEXLINK_PAIR_REQUEST_KEY, value)
                             : nvs_erase_key(handle, NEXLINK_PAIR_REQUEST_KEY);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

static void notify(nexlink_pair_state_t state)
{
    if (s_state_callback) s_state_callback(state, s_state_context);
}

static void finish_pairing(const nexlink_pair_record_t *record, nexlink_pair_state_t state)
{
    if (record) ESP_ERROR_CHECK(nexlink_pairing_save(record));
    ESP_ERROR_CHECK(write_request(0));
    notify(state);
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

esp_err_t nexlink_pairing_load(nexlink_pair_record_t *record, bool *paired)
{
    if (!record || !paired) return ESP_ERR_INVALID_ARG;
    *paired = false;
    nvs_handle_t handle;
    const esp_err_t opened = pairing_open(NVS_READONLY, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    ESP_RETURN_ON_ERROR(opened, "nexlink_pair", "NVS open failed");
    size_t length = sizeof(*record);
    const esp_err_t result = nvs_get_blob(handle, NEXLINK_PAIR_RECORD_KEY, record, &length);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK || length != sizeof(*record)) return result != ESP_OK ? result : ESP_ERR_INVALID_SIZE;
    *paired = record->channel >= 1 && record->channel <= 14;
    return *paired ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t nexlink_pairing_save(const nexlink_pair_record_t *record)
{
    if (!record || record->channel == 0 || record->channel > 14) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(pairing_open(NVS_READWRITE, &handle), "nexlink_pair", "NVS open failed");
    esp_err_t result = nvs_set_blob(handle, NEXLINK_PAIR_RECORD_KEY, record, sizeof(*record));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

esp_err_t nexlink_pairing_clear(void)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(pairing_open(NVS_READWRITE, &handle), "nexlink_pair", "NVS open failed");
    esp_err_t result = nvs_erase_key(handle, NEXLINK_PAIR_RECORD_KEY);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_erase_key(handle, NEXLINK_PAIR_REQUEST_KEY);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

esp_err_t nexlink_pairing_request(nexlink_pair_role_t role)
{
    if (role != NEXLINK_PAIR_ROLE_RX && role != NEXLINK_PAIR_ROLE_TX) return ESP_ERR_INVALID_ARG;
    return write_request((uint8_t)role + 1U);
}

bool nexlink_pairing_mode_is_requested(nexlink_pair_role_t role)
{
    nvs_handle_t handle;
    if (pairing_open(NVS_READONLY, &handle) != ESP_OK) return false;
    uint8_t value = 0;
    const esp_err_t result = nvs_get_u8(handle, NEXLINK_PAIR_REQUEST_KEY, &value);
    nvs_close(handle);
    return result == ESP_OK && value == (uint8_t)role + 1U;
}

static bool is_offer(const espnow_transport_packet_t *packet)
{
    nexlink_pair_offer_t offer;
    if (packet->length != sizeof(offer)) return false;
    memcpy(&offer, packet->data, sizeof(offer));
    if (offer.magic != NEXLINK_PAIR_MAGIC || offer.version != NEXLINK_PAIR_VERSION || offer.type != NEXLINK_PAIR_MSG_OFFER) return false;
    s_offer = offer;
    return true;
}

static bool is_accept(const espnow_transport_packet_t *packet)
{
    nexlink_pair_accept_t accept;
    if (packet->length != sizeof(accept)) return false;
    memcpy(&accept, packet->data, sizeof(accept));
    return accept.magic == NEXLINK_PAIR_MAGIC && accept.version == NEXLINK_PAIR_VERSION &&
           accept.type == NEXLINK_PAIR_MSG_ACCEPT && accept.nonce == s_offer.nonce;
}

static void send_accept(void)
{
    const nexlink_pair_accept_t accept = { .magic = NEXLINK_PAIR_MAGIC, .version = NEXLINK_PAIR_VERSION,
                                            .type = NEXLINK_PAIR_MSG_ACCEPT, .nonce = s_offer.nonce };
    for (int index = 0; index < 3; ++index) {
        (void)espnow_transport_send((const uint8_t *)&accept, sizeof(accept));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void pairing_task(void *argument)
{
    (void)argument;
    const TickType_t start = xTaskGetTickCount();
    TickType_t last_offer = start - pdMS_TO_TICKS(NEXLINK_PAIR_OFFER_PERIOD_MS);
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        if (now - start >= pdMS_TO_TICKS(NEXLINK_PAIR_TIMEOUT_MS)) {
            ESP_LOGW("nexlink_pair", "pairing timed out");
            finish_pairing(NULL, NEXLINK_PAIR_STATE_TIMED_OUT);
        }
        if (s_role == NEXLINK_PAIR_ROLE_TX && now - last_offer >= pdMS_TO_TICKS(NEXLINK_PAIR_OFFER_PERIOD_MS)) {
            (void)espnow_transport_send((const uint8_t *)&s_offer, sizeof(s_offer));
            last_offer = now;
        }
        espnow_transport_packet_t packet;
        if (espnow_transport_receive(&packet, 100) != ESP_OK) continue;
        if (s_role == NEXLINK_PAIR_ROLE_RX && is_offer(&packet)) {
            nexlink_pair_record_t record = { .channel = NEXLINK_PAIR_CHANNEL };
            memcpy(record.peer_mac, packet.source_mac, sizeof(record.peer_mac));
            memcpy(record.lmk, s_offer.lmk, sizeof(record.lmk));
            send_accept();
            finish_pairing(&record, NEXLINK_PAIR_STATE_SUCCEEDED);
        }
        if (s_role == NEXLINK_PAIR_ROLE_TX && is_accept(&packet)) {
            nexlink_pair_record_t record = { .channel = NEXLINK_PAIR_CHANNEL };
            memcpy(record.peer_mac, packet.source_mac, sizeof(record.peer_mac));
            memcpy(record.lmk, s_offer.lmk, sizeof(record.lmk));
            finish_pairing(&record, NEXLINK_PAIR_STATE_SUCCEEDED);
        }
    }
}

esp_err_t nexlink_pairing_start(nexlink_pair_role_t role, nexlink_pairing_state_callback_t state_callback, void *context)
{
    if (role != NEXLINK_PAIR_ROLE_RX && role != NEXLINK_PAIR_ROLE_TX) return ESP_ERR_INVALID_ARG;
    s_role = role; s_state_callback = state_callback; s_state_context = context;
    memset(&s_offer, 0, sizeof(s_offer));
    if (role == NEXLINK_PAIR_ROLE_TX) {
        s_offer.magic = NEXLINK_PAIR_MAGIC; s_offer.version = NEXLINK_PAIR_VERSION; s_offer.type = NEXLINK_PAIR_MSG_OFFER;
        s_offer.nonce = esp_random();
        esp_fill_random(s_offer.lmk, sizeof(s_offer.lmk));
    }
    ESP_RETURN_ON_ERROR(espnow_transport_init_broadcast(NEXLINK_PAIR_CHANNEL), "nexlink_pair", "broadcast init failed");
    notify(NEXLINK_PAIR_STATE_SEARCHING);
    return xTaskCreatePinnedToCore(pairing_task, "nexlink_pair", 4096, NULL, 6, NULL,
                                   NEXLINK_CPU_WIRELESS) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
