$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root "components/BSP/LED/led.h"
$sourcePath = Join-Path $root "components/BSP/LED/led.c"

$header = Get-Content -Raw -LiteralPath $headerPath
$source = Get-Content -Raw -LiteralPath $sourcePath

foreach ($required in @('LED_STATE_OFF', 'LED_STATE_ON', 'LED_STATE_BLINK_1HZ', 'led_set_state')) {
    if ($header -notmatch [regex]::Escape($required)) {
        throw "led.h is missing: $required"
    }
}

foreach ($required in @('xTaskCreate', 'vTaskDelay', 'LED_STATE_BLINK_1HZ')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "led.c is missing state-controller element: $required"
    }
}

Write-Host "PASS: LED uses a centralized off/on/blink state controller"
