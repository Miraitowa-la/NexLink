$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$bsp = Get-Content "$root/components/BSP/CMakeLists.txt" -Raw
$middlewares = Get-Content "$root/components/Middlewares/CMakeLists.txt" -Raw
$transport = Get-Content "$root/components/Middlewares/espnow_transport/espnow_transport.c" -Raw
$tx = Get-Content "$root/main/main_tx.c" -Raw
$rx = Get-Content "$root/main/main_rx.c" -Raw

function Require-Match([string] $content, [string] $pattern, [string] $message) {
    if ($content -notmatch $pattern) { throw $message }
}

Require-Match $bsp 'KEY' 'BSP must include the KEY driver.'
Require-Match $middlewares 'espnow_transport' 'Middlewares must include ESP-NOW transport.'
Require-Match $middlewares 'nexlink_link' 'Middlewares must include NexLink link protocol.'
Require-Match $middlewares 'esp_wifi' 'Middlewares must depend on esp_wifi.'
Require-Match $middlewares 'nvs_flash' 'Middlewares must depend on nvs_flash before Wi-Fi starts.'
Require-Match $transport 'nvs_flash_init' 'ESP-NOW transport must initialize NVS before Wi-Fi.'
Require-Match $tx 'key_register_click_callback' 'TX must map a key click to its mode switch.'
Require-Match $tx 'NEXLINK_TX_MODE_ACTIVE' 'TX must define its active wireless mode.'
Require-Match $rx 'NEXLINK_RX_MODE_WIRELESS' 'RX must define its wireless mode.'
Require-Match (Get-Content "$root/components/Middlewares/nexlink_link/nexlink_link.h" -Raw) 'nexlink_link_init' 'Link module must expose transport initialization.'

foreach ($content in @(
    $tx,
    $rx,
    (Get-Content "$root/components/Middlewares/nexlink_link/nexlink_link.h" -Raw),
    (Get-Content "$root/components/Middlewares/nexlink_link/nexlink_link.c" -Raw))) {
    foreach ($obsolete in @('nexlink_link_send_led_toggle', 'NEXLINK_LINK_MSG_LED_TOGGLE', 'LED_TOGGLE')) {
        if ($content -match [regex]::Escape($obsolete)) {
            throw "Obsolete LED demo protocol element remains: $obsolete"
        }
    }
}

Write-Host 'PASS: ESP-NOW P0 mode integration is present without the LED demo protocol.'
