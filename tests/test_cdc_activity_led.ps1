$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$ledHeader = Get-Content -Raw -LiteralPath (Join-Path $root "components/BSP/LED/led.h")
$ledSource = Get-Content -Raw -LiteralPath (Join-Path $root "components/BSP/LED/led.c")
$usbSource = Get-Content -Raw -LiteralPath (Join-Path $root "components/Middlewares/usb_device/usb_device.c")
$bridgeSource = Get-Content -Raw -LiteralPath (Join-Path $root "components/Middlewares/nexlink_dap_bridge/nexlink_dap_bridge.c")

foreach ($required in @(
    'LED_CDC_RX_GPIO_PIN GPIO_NUM_9',
    'LED_CDC_TX_GPIO_PIN GPIO_NUM_46',
    'led_cdc_rx_activity',
    'led_cdc_tx_activity')) {
    if ($ledHeader -notmatch [regex]::Escape($required)) {
        throw "led.h is missing: $required"
    }
}

foreach ($required in @(
    'cdc_led_task',
    'LED_CDC_ACTIVITY_HOLD_MS',
    'LED_CDC_RX_GPIO_PIN',
    'LED_CDC_TX_GPIO_PIN')) {
    if ($ledSource -notmatch [regex]::Escape($required)) {
        throw "led.c is missing CDC activity support: $required"
    }
}

foreach ($check in @(
    @($usbSource, 'led_cdc_rx_activity'),
    @($usbSource, 'led_cdc_tx_activity'),
    @($bridgeSource, 'NEXLINK_PROTOCOL_MSG_CDC_DATA_TX'),
    @($bridgeSource, 'NEXLINK_PROTOCOL_MSG_CDC_DATA_RX'),
    @($bridgeSource, 'led_cdc_rx_activity'),
    @($bridgeSource, 'led_cdc_tx_activity')
)) {
    if ($check[0] -notmatch [regex]::Escape($check[1])) {
        throw "Missing CDC activity LED token '$($check[1])'"
    }
}

Write-Host "PASS: CDC RX/TX activity LEDs are wired to GPIO9/GPIO46"
