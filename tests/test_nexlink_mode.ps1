$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root "components/Middlewares/nexlink_mode/nexlink_mode.h"
$sourcePath = Join-Path $root "components/Middlewares/nexlink_mode/nexlink_mode.c"

if (-not (Test-Path -LiteralPath $headerPath)) {
    throw "nexlink_mode.h does not exist"
}

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "nexlink_mode.c does not exist"
}

$header = Get-Content -Raw -LiteralPath $headerPath
$source = Get-Content -Raw -LiteralPath $sourcePath

foreach ($required in @(
    'NEXLINK_RX_MODE_DIRECT',
    'NEXLINK_RX_MODE_WIRELESS',
    'NEXLINK_TX_MODE_OFF',
    'NEXLINK_TX_MODE_ACTIVE',
    'nexlink_mode_get_rx',
    'nexlink_mode_get_tx',
    'nexlink_mode_set_rx_and_restart',
    'nexlink_mode_set_tx_and_restart')) {
    if ($header -notmatch [regex]::Escape($required)) {
        throw "nexlink_mode.h is missing: $required"
    }
}

foreach ($required in @(
    'nvs_flash_init',
    'nvs_open',
    'nvs_get_u8',
    'nvs_set_u8',
    'nvs_commit',
    'esp_restart')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "nexlink_mode.c is missing: $required"
    }
}

Write-Host "PASS: nexlink_mode persists RX/TX modes and restarts after changes"
