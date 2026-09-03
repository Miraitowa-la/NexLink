$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$txPath = Join-Path $root "main/main_tx.c"
$linkHeaderPath = Join-Path $root "components/Middlewares/nexlink_link/nexlink_link.h"
$linkSourcePath = Join-Path $root "components/Middlewares/nexlink_link/nexlink_link.c"
$tx = Get-Content -Raw -LiteralPath $txPath
$linkHeader = Get-Content -Raw -LiteralPath $linkHeaderPath
$linkSource = Get-Content -Raw -LiteralPath $linkSourcePath

foreach ($required in @(
    'nexlink_mode_get_tx()',
    'nexlink_mode_set_tx_and_restart',
    'NEXLINK_TX_MODE_OFF',
    'NEXLINK_TX_MODE_ACTIVE',
    'LED_STATE_OFF',
    'LED_STATE_ON',
    'nexlink_link_init')) {
    if ($tx -notmatch [regex]::Escape($required)) {
        throw "main_tx.c is missing TX mode element: $required"
    }
}

foreach ($source in @($tx, $linkHeader, $linkSource)) {
    foreach ($forbidden in @('nexlink_link_send_led_toggle', 'NEXLINK_LINK_MSG_LED_TOGGLE', 'LED_TOGGLE')) {
        if ($source -match [regex]::Escape($forbidden)) {
            throw "TX/link source still contains obsolete LED demo element: $forbidden"
        }
    }
}

Write-Host "PASS: TX OFF/ACTIVE modes do not contain the LED demo protocol"
