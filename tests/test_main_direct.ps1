$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root "main/main_direct.c"

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "main_direct.c does not exist"
}

$source = Get-Content -Raw -LiteralPath $sourcePath

foreach ($required in @(
    '#include "bridge_target.h"',
    '#include "debug_probe.h"',
    '#include "usb_device.h"',
    'debug_probe_init()',
    'bridge_target_create',
    'usb_device_start')) {
    if ($source -notmatch [regex]::Escape($required)) {
        throw "main_direct.c is missing required direct-mode element: $required"
    }
}

foreach ($forbidden in @('nexlink_link.h', 'nexlink_link_init', 'espnow_')) {
    if ($source -match [regex]::Escape($forbidden)) {
        throw "main_direct.c must not initialize wireless functionality: $forbidden"
    }
}

Write-Host "PASS: main_direct.c contains only the wired DAP+CDC application path"
