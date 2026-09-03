#include "nexlink_mode.h"

#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NEXLINK_MODE_NAMESPACE "nexlink"
#define NEXLINK_MODE_RX_KEY "rx_mode"
#define NEXLINK_MODE_TX_KEY "tx_mode"

static esp_err_t nexlink_mode_init_storage(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    }
    return result;
}

static uint8_t nexlink_mode_read(const char *key, uint8_t default_value)
{
    if (nexlink_mode_init_storage() != ESP_OK) {
        return default_value;
    }

    nvs_handle_t handle;
    if (nvs_open(NEXLINK_MODE_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return default_value;
    }

    uint8_t value = default_value;
    const esp_err_t result = nvs_get_u8(handle, key, &value);
    nvs_close(handle);
    return result == ESP_OK ? value : default_value;
}

static esp_err_t nexlink_mode_write(const char *key, uint8_t value)
{
    esp_err_t result = nexlink_mode_init_storage();
    if (result != ESP_OK) {
        return result;
    }

    nvs_handle_t handle;
    result = nvs_open(NEXLINK_MODE_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }

    result = nvs_set_u8(handle, key, value);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

nexlink_rx_mode_t nexlink_mode_get_rx(void)
{
    const uint8_t value = nexlink_mode_read(NEXLINK_MODE_RX_KEY, NEXLINK_RX_MODE_DIRECT);
    return value == NEXLINK_RX_MODE_WIRELESS ? NEXLINK_RX_MODE_WIRELESS : NEXLINK_RX_MODE_DIRECT;
}

nexlink_tx_mode_t nexlink_mode_get_tx(void)
{
    const uint8_t value = nexlink_mode_read(NEXLINK_MODE_TX_KEY, NEXLINK_TX_MODE_OFF);
    return value == NEXLINK_TX_MODE_ACTIVE ? NEXLINK_TX_MODE_ACTIVE : NEXLINK_TX_MODE_OFF;
}

esp_err_t nexlink_mode_set_rx_and_restart(nexlink_rx_mode_t mode)
{
    if (mode != NEXLINK_RX_MODE_DIRECT && mode != NEXLINK_RX_MODE_WIRELESS) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t result = nexlink_mode_write(NEXLINK_MODE_RX_KEY, (uint8_t)mode);
    if (result == ESP_OK) {
        esp_restart();
    }
    return result;
}

esp_err_t nexlink_mode_set_tx_and_restart(nexlink_tx_mode_t mode)
{
    if (mode != NEXLINK_TX_MODE_OFF && mode != NEXLINK_TX_MODE_ACTIVE) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t result = nexlink_mode_write(NEXLINK_MODE_TX_KEY, (uint8_t)mode);
    if (result == ESP_OK) {
        esp_restart();
    }
    return result;
}
