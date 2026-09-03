$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$taskConfig = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_task_config\nexlink_task_config.h')
$middlewaresCmake = Get-Content -Raw (Join-Path $root 'components\Middlewares\CMakeLists.txt')
$dapCore = Get-Content -Raw (Join-Path $root 'components\Middlewares\debug_probe\cmsis_dap_core.c')
$targetUart = Get-Content -Raw (Join-Path $root 'components\Middlewares\transport_uart\transport_uart.c')
$usbDevice = Get-Content -Raw (Join-Path $root 'components\Middlewares\usb_device\usb_device.c')
$usbV1 = Get-Content -Raw (Join-Path $root 'components\Middlewares\usb_device\cmsis_dap_v1_hid.c')
$usbV2 = Get-Content -Raw (Join-Path $root 'components\Middlewares\usb_device\cmsis_dap_v2_bulk.c')
$link = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_link\nexlink_link.c')
$pairing = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_pairing\nexlink_pairing.c')

foreach ($check in @(
    @($taskConfig, '#define\s+NEXLINK_CPU_WIRELESS\s+0'),
    @($taskConfig, '#define\s+NEXLINK_CPU_TARGET\s+1'),
    @($middlewaresCmake, 'nexlink_task_config')
)) {
    if ($check[0] -notmatch $check[1]) {
        throw "Expected dual-core token '$($check[1])' was not found."
    }
}

foreach ($check in @(
    @($dapCore, 'cmsis_dap_task', 'NEXLINK_CPU_TARGET'),
    @($targetUart, 'transport_uart_task', 'NEXLINK_CPU_TARGET'),
    @($usbDevice, 'usb_device_task', 'NEXLINK_CPU_WIRELESS'),
    @($usbDevice, 'cdc_sender_task', 'NEXLINK_CPU_WIRELESS'),
    @($usbV1, 'cmsis_dap_v1_hid_task', 'NEXLINK_CPU_WIRELESS'),
    @($usbV2, 'usb_send_task', 'NEXLINK_CPU_WIRELESS'),
    @($link, 'nexlink_link_task', 'NEXLINK_CPU_WIRELESS'),
    @($pairing, 'pairing_task', 'NEXLINK_CPU_WIRELESS')
)) {
    if ($check[0] -notmatch [regex]::Escape($check[1]) -or
        $check[0] -notmatch [regex]::Escape($check[2])) {
        throw "Expected task/core pair '$($check[1])' / '$($check[2])' was not found."
    }
}

Write-Host 'PASS: dual-core task affinity configuration is present'
