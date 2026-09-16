#include <stdio.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bridge_target.h"
#include "debug_gpio.h"
#include "debug_probe.h"
#include "led.h"
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

void app_main(void)
{
    printf("NexLink DIRECT starting\n");

    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(debug_gpio_init(&debug_gpio_config));
    ESP_ERROR_CHECK(debug_probe_init());

    bridge_target_t *cdc_target = bridge_target_create(&cdc_target_config);
    ESP_ERROR_CHECK(cdc_target ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(usb_device_init_serial_number());
    ESP_ERROR_CHECK(usb_phy_init());
    ESP_ERROR_CHECK(usb_device_start(&usb_device_config, cdc_target));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
