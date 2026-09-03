$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$protocol = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_protocol\nexlink_protocol.h')
$bridge = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_dap_bridge\nexlink_dap_bridge.c')
$frontend = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_tx_frontend\nexlink_tx_frontend.c')
$usb = Get-Content -Raw (Join-Path $root 'components\Middlewares\usb_device\usb_device.h')
$rxMain = Get-Content -Raw (Join-Path $root 'main\main_rx.c')
foreach ($check in @(
 @($protocol,'NEXLINK_PROTOCOL_MSG_CDC_FLOW'), @($bridge,'NEXLINK_CDC_WINDOW 4'),
 @($bridge,'bridge_target_write'), @($bridge,'bridge_target_set_line_format'),
 @($bridge,'NEXLINK_PROTOCOL_MSG_CDC_DATA_RX'), @($frontend,'nexlink_tx_frontend_submit_cdc_data'),
 @($usb,'usb_device_frontend_write')
)) { if ($check[0] -notmatch [regex]::Escape($check[1])) { throw "Missing CDC flow token '$($check[1])'" } }
$wirelessStart = $rxMain.IndexOf('static void rx_start_wireless')
$wirelessEnd = $rxMain.IndexOf('static void rx_key_click')
$wirelessBody = $rxMain.Substring($wirelessStart, $wirelessEnd - $wirelessStart)
if ($wirelessBody -notmatch [regex]::Escape('nexlink_dap_bridge_set_cdc_target(cdc_target)')) {
    throw 'RX wireless initialization must provide its UART target to the wireless bridge.'
}
Write-Host 'PASS: wireless CDC forwarding and flow-control interfaces are present'
