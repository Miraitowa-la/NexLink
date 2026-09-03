$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$pairHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_pairing\nexlink_pairing.h')
$pairSource = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_pairing\nexlink_pairing.c')
$transportHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\espnow_transport\espnow_transport.h')
$linkHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_link\nexlink_link.h')
$rxMain = Get-Content -Raw (Join-Path $root 'main\main_rx.c')
$txMain = Get-Content -Raw (Join-Path $root 'main\main_tx.c')
$keySource = Get-Content -Raw (Join-Path $root 'components\BSP\KEY\key.c')

$required = @(
    @($pairHeader, 'NEXLINK_PAIR_ROLE_TX'),
    @($pairHeader, 'nexlink_pairing_start'),
    @($pairHeader, 'nexlink_pairing_mode_is_requested'),
    @($pairSource, 'esp_fill_random'),
    @($pairSource, 'NEXLINK_PAIR_MSG_OFFER'),
    @($pairSource, 'NEXLINK_PAIR_MSG_ACCEPT'),
    @($transportHeader, 'espnow_transport_init_broadcast'),
    @($linkHeader, 'nexlink_link_init_encrypted'),
    @($rxMain, 'key_register_hold_callbacks'),
    @($txMain, 'key_register_hold_callbacks'),
    @($rxMain, 'nexlink_pairing_mode_is_requested'),
    @($txMain, 'nexlink_pairing_mode_is_requested'),
    @($keySource, '#define KEY_PAIR_HOLD_MS 2000'),
    @($keySource, '#define KEY_RESET_HOLD_MS 5000')
)

foreach ($check in $required) {
    if ($check[0] -notmatch [regex]::Escape($check[1])) {
        throw "Expected pairing flow token '$($check[1])' was not found."
    }
}

Write-Host 'PASS: pairing flow interfaces are present'
