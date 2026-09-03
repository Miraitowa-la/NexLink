$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$protocolHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_protocol\nexlink_protocol.h')
$protocolSource = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_protocol\nexlink_protocol.c')
$probeHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\debug_probe\include\debug_probe.h')
$usbHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\usb_device\usb_device.h')
$txMain = Get-Content -Raw (Join-Path $root 'main\main_tx.c')
$rxMain = Get-Content -Raw (Join-Path $root 'main\main_rx.c')

foreach ($check in @(
    @($protocolHeader, 'NEXLINK_PROTOCOL_MSG_DAP_REQUEST'),
    @($protocolHeader, 'NEXLINK_PROTOCOL_MSG_DAP_RESPONSE'),
    @($protocolHeader, 'NEXLINK_PROTOCOL_MSG_CDC_DATA_TX'),
    @($protocolHeader, 'nexlink_protocol_encode'),
    @($protocolHeader, 'nexlink_protocol_decode'),
    @($protocolSource, 'NEXLINK_PROTOCOL_MAGIC'),
    @($probeHeader, 'debug_probe_frontend_init'),
    @($probeHeader, 'debug_probe_submit_response'),
    @($usbHeader, 'usb_device_start_frontend'),
    @($txMain, 'nexlink_tx_frontend_init'),
    @($rxMain, 'debug_gpio_init'),
    @($rxMain, 'debug_probe_init'),
    @($rxMain, 'bridge_target_create')
)) {
    if ($check[0] -notmatch [regex]::Escape($check[1])) {
        throw "Expected P3 token '$($check[1])' was not found."
    }
}

Write-Host 'PASS: P3 protocol, TX USB frontend, and RX target executor skeleton are present'
