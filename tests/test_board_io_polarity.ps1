$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$keyHeader = Get-Content -Raw -LiteralPath (Join-Path $root "components/BSP/KEY/key.h")
$ledSource = Get-Content -Raw -LiteralPath (Join-Path $root "components/BSP/LED/led.c")

if ($keyHeader -notmatch 'KEY_GPIO_PIN\s+GPIO_NUM_38') {
    throw "KEY_GPIO_PIN must be GPIO38"
}

foreach ($required in @('#define LED_ON_LEVEL 1', '#define LED_OFF_LEVEL 0')) {
    if ($ledSource -notmatch [regex]::Escape($required)) {
        throw "LED active-high polarity definition is missing: $required"
    }
}

if ($ledSource -notmatch 'gpio_set_level\(LED_GPIO_PIN, LED_OFF_LEVEL\)') {
    throw "LED off state must drive the active-high LED low"
}

if ($ledSource -notmatch 'gpio_set_level\(LED_GPIO_PIN, LED_ON_LEVEL\)') {
    throw "LED on state must drive the active-high LED high"
}

Write-Host "PASS: GPIO38 key and active-high LED polarity are configured"
