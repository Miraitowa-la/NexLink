#include <stdio.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "key.h"
#include "nexlink_link.h"
#include "nexlink_mode.h"
#include "nexlink_pairing.h"
#include "nexlink_dap_bridge.h"
#include "nexlink_tx_frontend.h"
#include "usb_phy.h"

static const usb_device_config_t usb_device_config = {
    .vendor_id = 0x303A,
    .product_id = 0x1002,
    .manufacturer = "NexLink",
};

static void tx_link_state_changed(nexlink_link_state_t state, void *context)
{
    (void)context;
    ESP_ERROR_CHECK(led_set_state(state == NEXLINK_LINK_STATE_CONNECTED
                                      ? LED_STATE_BLINK_1HZ
                                      : LED_STATE_ON));
}

static void tx_key_click(void *context)
{
    (void)context;
    const nexlink_tx_mode_t current_mode = nexlink_mode_get_tx();
    const nexlink_tx_mode_t next_mode = current_mode == NEXLINK_TX_MODE_OFF
                                          ? NEXLINK_TX_MODE_ACTIVE
                                          : NEXLINK_TX_MODE_OFF;
    ESP_ERROR_CHECK(nexlink_mode_set_tx_and_restart(next_mode));
}

static void tx_key_pair(void *context)
{
    (void)context;
    ESP_ERROR_CHECK(nexlink_pairing_request(NEXLINK_PAIR_ROLE_TX));
    ESP_ERROR_CHECK(nexlink_mode_set_tx_and_restart(NEXLINK_TX_MODE_ACTIVE));
}

static void tx_key_reset_pairing(void *context)
{
    (void)context;
    ESP_ERROR_CHECK(nexlink_pairing_clear());
    ESP_ERROR_CHECK(nexlink_mode_set_tx_and_restart(NEXLINK_TX_MODE_OFF));
}

void app_main(void)
{
    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(key_init());
    key_register_click_callback(tx_key_click, NULL);
    key_register_hold_callbacks(tx_key_pair, NULL, tx_key_reset_pairing, NULL);

    if (nexlink_mode_get_tx() == NEXLINK_TX_MODE_ACTIVE) {
        printf("NexLink TX ACTIVE starting\n");
        ESP_ERROR_CHECK(led_set_state(LED_STATE_ON));
        ESP_ERROR_CHECK(usb_phy_init());
        ESP_ERROR_CHECK(nexlink_tx_frontend_init(&usb_device_config));
        if (nexlink_pairing_mode_is_requested(NEXLINK_PAIR_ROLE_TX)) {
            printf("NexLink TX pairing: broadcasting offer\n");
            ESP_ERROR_CHECK(nexlink_pairing_start(NEXLINK_PAIR_ROLE_TX, NULL, NULL));
            ESP_ERROR_CHECK(led_set_state(LED_STATE_BLINK_4HZ));
        } else {
            nexlink_pair_record_t record;
            bool paired = false;
            ESP_ERROR_CHECK(nexlink_pairing_load(&record, &paired));
            if (paired) {
                ESP_ERROR_CHECK(nexlink_link_init_encrypted(record.peer_mac, record.channel, record.lmk,
                                                             tx_link_state_changed, NULL));
                ESP_ERROR_CHECK(nexlink_dap_bridge_start_tx());
            } else {
                printf("NexLink TX is not paired; hold KEY for 3 seconds to pair\n");
            }
        }
    } else {
        printf("NexLink TX OFF starting\n");
        ESP_ERROR_CHECK(led_set_state(LED_STATE_OFF));
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
