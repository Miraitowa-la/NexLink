#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "espnow_transport.h"

static QueueHandle_t s_receive_queue;
static uint8_t s_peer_mac[6];
static uint8_t s_local_mac[6];
static const uint8_t s_pmk[ESP_NOW_KEY_LEN] = "NexLink-PMK-2026";

static void espnow_receive_callback(const esp_now_recv_info_t *info, const uint8_t *data, int length)
{
    if (!info || !data || length <= 0 || length > ESPNOW_TRANSPORT_MAX_PAYLOAD) {
        return;
    }
    espnow_transport_packet_t packet = {
        .length = (size_t)length,
    };
    memcpy(packet.source_mac, info->src_addr, sizeof(packet.source_mac));
    memcpy(packet.data, data, packet.length);
    xQueueSend(s_receive_queue, &packet, 0);
}

esp_err_t espnow_transport_init(const uint8_t peer_mac[6], uint8_t channel)
{
    if (!peer_mac || channel == 0 || channel > 14) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), "espnow", "NVS erase failed");
        nvs_result = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(nvs_result, "espnow", "NVS init failed");
    const esp_err_t netif_result = esp_netif_init();
    if (netif_result != ESP_OK && netif_result != ESP_ERR_INVALID_STATE) {
        return netif_result;
    }
    const esp_err_t event_result = esp_event_loop_create_default();
    if (event_result != ESP_OK && event_result != ESP_ERR_INVALID_STATE) {
        return event_result;
    }
    const wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_config), "espnow", "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), "espnow", "wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), "espnow", "wifi start failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE), "espnow", "channel failed");
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_STA, s_local_mac), "espnow", "get local MAC failed");
    ESP_LOGI("espnow", "STA MAC %02X:%02X:%02X:%02X:%02X:%02X, channel %u",
             s_local_mac[0], s_local_mac[1], s_local_mac[2], s_local_mac[3], s_local_mac[4], s_local_mac[5], channel);
    ESP_RETURN_ON_ERROR(esp_now_init(), "espnow", "esp-now init failed");

    s_receive_queue = xQueueCreate(8, sizeof(espnow_transport_packet_t));
    if (!s_receive_queue) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(s_peer_mac, peer_mac, sizeof(s_peer_mac));
    const esp_now_peer_info_t peer = {
        .channel = channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    esp_now_peer_info_t peer_config = peer;
    memcpy(peer_config.peer_addr, s_peer_mac, sizeof(s_peer_mac));
    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer_config), "espnow", "peer add failed");
    return esp_now_register_recv_cb(espnow_receive_callback);
}

esp_err_t espnow_transport_init_broadcast(uint8_t channel)
{
    static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    return espnow_transport_init(broadcast_mac, channel);
}

esp_err_t espnow_transport_get_local_mac(uint8_t mac[6])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(mac, s_local_mac, sizeof(s_local_mac));
    return ESP_OK;
}

esp_err_t espnow_transport_init_encrypted(const uint8_t peer_mac[6], uint8_t channel, const uint8_t lmk[16])
{
    if (!lmk) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(espnow_transport_init(peer_mac, channel), "espnow", "base init failed");
    ESP_RETURN_ON_ERROR(esp_now_del_peer(peer_mac), "espnow", "remove plain peer failed");
    ESP_RETURN_ON_ERROR(esp_now_set_pmk(s_pmk), "espnow", "set PMK failed");
    esp_now_peer_info_t peer = { .channel = channel, .ifidx = WIFI_IF_STA, .encrypt = true };
    memcpy(peer.peer_addr, peer_mac, sizeof(peer.peer_addr));
    memcpy(peer.lmk, lmk, sizeof(peer.lmk));
    return esp_now_add_peer(&peer);
}

esp_err_t espnow_transport_send(const uint8_t *data, size_t length)
{
    if (!data || length == 0 || length > ESPNOW_TRANSPORT_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_now_send(s_peer_mac, data, length);
}

esp_err_t espnow_transport_receive(espnow_transport_packet_t *packet, uint32_t timeout_ms)
{
    if (!packet) {
        return ESP_ERR_INVALID_ARG;
    }
    return xQueueReceive(s_receive_queue, packet, pdMS_TO_TICKS(timeout_ms)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
