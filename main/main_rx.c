#include <stdio.h>
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bridge_target.h"
#include "debug_gpio.h"
#include "debug_probe.h"
#include "led.h"
#include "key.h"
#include "nexlink_link.h"
#include "nexlink_mode.h"
#include "nexlink_pairing.h"
#include "nexlink_dap_bridge.h"
#include "usb_device.h"
#include "usb_phy.h"

/*
 * NexLink 的 USB 身份。修改 VID/PID 后，Windows 会将其视作新的 USB 设备实例。
 * 0x303A/0x1002 仅适用于开发测试，发布时必须替换为项目拥有或获授权的 VID/PID。
 */
static const usb_device_config_t usb_device_config = {
    .vendor_id = 0x303A,
    .product_id = 0x1002,
    .manufacturer = "NexLink",
};

/*
 * SWD 与 JTAG 共用 TCK/SWCLK、TMS/SWDIO 两对信号；引脚编号按当前 NexLink
 * 硬件连接填写。目标板必须与 ESP32-S3 共地，且调试信号电平不得超过 3.3 V。
 */
static const debug_gpio_config_t debug_gpio_config = {
    .tdi = GPIO_NUM_10,
    .tdo = GPIO_NUM_12,
    .tck = GPIO_NUM_17,
    .tms = GPIO_NUM_18,
    .nrst = GPIO_NUM_13,
};

static const bridge_target_config_t cdc_target_config = {
    .uart = {
        .port = UART_NUM_1,
        .tx_pin = GPIO_NUM_47,
        .rx_pin = GPIO_NUM_45,
        .baud_rate = 115200,
    },
};

static void rx_link_state_changed(nexlink_link_state_t state, void *context)
{
    (void)context;
    ESP_ERROR_CHECK(led_set_state(state == NEXLINK_LINK_STATE_CONNECTED
                                      ? LED_STATE_BLINK_1HZ
                                      : LED_STATE_ON));
}

static void rx_start_direct(void)
{
    printf("NexLink RX DIRECT starting\n");
    ESP_ERROR_CHECK(led_set_state(LED_STATE_OFF));
    ESP_ERROR_CHECK(debug_gpio_init(&debug_gpio_config));
    ESP_ERROR_CHECK(debug_probe_init());

    bridge_target_t *cdc_target = bridge_target_create(&cdc_target_config);
    ESP_ERROR_CHECK(cdc_target ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(usb_device_init_serial_number());
    ESP_ERROR_CHECK(usb_phy_init());
    ESP_ERROR_CHECK(usb_device_start(&usb_device_config, cdc_target));
}

static void rx_start_wireless(void)
{
    printf("NexLink RX WIRELESS starting\n");
    ESP_ERROR_CHECK(led_set_state(LED_STATE_ON));
    ESP_ERROR_CHECK(debug_gpio_init(&debug_gpio_config));
    ESP_ERROR_CHECK(debug_probe_init());
    bridge_target_t *cdc_target = bridge_target_create(&cdc_target_config);
    ESP_ERROR_CHECK(cdc_target ? ESP_OK : ESP_ERR_NO_MEM);
    nexlink_dap_bridge_set_cdc_target(cdc_target);
    if (nexlink_pairing_mode_is_requested(NEXLINK_PAIR_ROLE_RX)) {
        printf("NexLink RX pairing: waiting for TX offer\n");
        ESP_ERROR_CHECK(nexlink_pairing_start(NEXLINK_PAIR_ROLE_RX, NULL, NULL));
        ESP_ERROR_CHECK(led_set_state(LED_STATE_BLINK_4HZ));
        return;
    }

    nexlink_pair_record_t record;
    bool paired = false;
    ESP_ERROR_CHECK(nexlink_pairing_load(&record, &paired));
    if (!paired) {
        printf("NexLink RX is not paired; hold KEY for 3 seconds to pair\n");
        return;
    }
    ESP_ERROR_CHECK(nexlink_link_init_encrypted(record.peer_mac, record.channel, record.lmk,
                                                 rx_link_state_changed, NULL));
    ESP_ERROR_CHECK(nexlink_dap_bridge_start_rx());
}

static void rx_key_click(void *context)
{
    (void)context;
    const nexlink_rx_mode_t current_mode = nexlink_mode_get_rx();
    const nexlink_rx_mode_t next_mode = current_mode == NEXLINK_RX_MODE_DIRECT
                                          ? NEXLINK_RX_MODE_WIRELESS
                                          : NEXLINK_RX_MODE_DIRECT;
    ESP_ERROR_CHECK(nexlink_mode_set_rx_and_restart(next_mode));
}

static void rx_key_pair(void *context)
{
    (void)context;
    ESP_ERROR_CHECK(nexlink_pairing_request(NEXLINK_PAIR_ROLE_RX));
    ESP_ERROR_CHECK(nexlink_mode_set_rx_and_restart(NEXLINK_RX_MODE_WIRELESS));
}

static void rx_key_reset_pairing(void *context)
{
    (void)context;
    ESP_ERROR_CHECK(nexlink_pairing_clear());
    ESP_ERROR_CHECK(nexlink_mode_set_rx_and_restart(NEXLINK_RX_MODE_DIRECT));
}

void app_main(void)
{
    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(key_init());
    key_register_click_callback(rx_key_click, NULL);
    key_register_hold_callbacks(rx_key_pair, NULL, rx_key_reset_pairing, NULL);

    if (nexlink_mode_get_rx() == NEXLINK_RX_MODE_WIRELESS) {
        rx_start_wireless();
    } else {
        rx_start_direct();
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
