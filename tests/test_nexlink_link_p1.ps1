$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -LiteralPath (Join-Path $root "components/Middlewares/nexlink_link/nexlink_link.h")
$source = Get-Content -Raw -LiteralPath (Join-Path $root "components/Middlewares/nexlink_link/nexlink_link.c")
$tx = Get-Content -Raw -LiteralPath (Join-Path $root "main/main_tx.c")
$rx = Get-Content -Raw -LiteralPath (Join-Path $root "main/main_rx.c")

foreach ($required in @(
    'NEXLINK_LINK_STATE_WAITING',
    'NEXLINK_LINK_STATE_CONNECTED',
    'nexlink_link_state_callback_t',
    'nexlink_link_init')) {
    if ($header -notmatch [regex]::Escape($required)) {
        throw "nexlink_link.h is missing P1 API: $required"
    }
}

foreach ($required in @(
    'NEXLINK_LINK_MSG_HELLO',
    'NEXLINK_LINK_MSG_HELLO_ACK',
    'NEXLINK_LINK_MSG_HEARTBEAT',
    'NEXLINK_LINK_HEARTBEAT_MS 500',
    'NEXLINK_LINK_TIMEOUT_MS 3000',
    'espnow_transport_receive',
    'espnow_transport_send',
    'memcmp(packet->source_mac',
    'xTaskCreate')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "nexlink_link.c is missing P1 behavior: $required"
    }
}

foreach ($main in @($tx, $rx)) {
    foreach ($required in @('NEXLINK_LINK_STATE_CONNECTED', 'LED_STATE_BLINK_1HZ', 'LED_STATE_ON')) {
        if ($main -notmatch [regex]::Escape($required)) {
            throw "TX/RX must map link state to LED state: $required"
        }
    }
}

Write-Host "PASS: P1 link handshake, heartbeat, timeout, and LED mapping are present"
