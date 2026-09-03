$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$linkHeader = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_link\nexlink_link.h')
$linkSource = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_link\nexlink_link.c')
$bridge = Get-Content -Raw (Join-Path $root 'components\Middlewares\nexlink_dap_bridge\nexlink_dap_bridge.c')
$tx = Get-Content -Raw (Join-Path $root 'main\main_tx.c')
$rx = Get-Content -Raw (Join-Path $root 'main\main_rx.c')
foreach ($check in @(
    @($linkHeader, 'nexlink_link_send_payload'), @($linkHeader, 'nexlink_link_set_payload_callback'),
    @($bridge, 'NEXLINK_DAP_RETRY_COUNT 3'), @($bridge, 'NEXLINK_DAP_RESPONSE_TIMEOUT_MS 1000'),
    @($bridge, 's_cached_response_valid'),
    @($bridge, 'debug_probe_process_data'), @($bridge, 'debug_probe_submit_response'),
    @($tx, 'nexlink_dap_bridge_start_tx'), @($rx, 'nexlink_dap_bridge_start_rx')
)) { if ($check[0] -notmatch [regex]::Escape($check[1])) { throw "Missing reliable DAP token '$($check[1])'" } }
Write-Host 'PASS: wireless DAP forwarding and retry interfaces are present'
