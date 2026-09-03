$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root "main/main_rx.c"
$source = Get-Content -Raw -LiteralPath $sourcePath

foreach ($required in @(
    'key_init()',
    'nexlink_mode_get_rx()',
    'nexlink_mode_set_rx_and_restart',
    'rx_start_direct',
    'rx_start_wireless',
    'usb_device_start',
    'nexlink_link_init',
    'LED_STATE_OFF',
    'LED_STATE_ON')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "main_rx.c is missing RX mode element: $required"
    }
}

foreach ($forbidden in @('LED_TOGGLE', 'NEXLINK_LINK_MSG_LED_TOGGLE')) {
    if ($source -match [regex]::Escape($forbidden)) {
        throw "main_rx.c still contains obsolete LED demo element: $forbidden"
    }
}

Write-Host "PASS: RX selects isolated DIRECT and WIRELESS initialization paths"
