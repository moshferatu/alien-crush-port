# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $RomPath,
    [string] $ToolsPath = 'out/build/windows-vs2022/Release/alien-crush-tools.exe',
    [string] $OutputDirectory = 'out/captures/verified-baseline'
)
$ErrorActionPreference = 'Stop'
$acGolden = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../tests/reference-baseline.json') | ConvertFrom-Json
& $ToolsPath --rom $RomPath --verify-replay --frames $acGolden.frames --capture $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Reference replay verification failed' }
$acActual = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'census.csv') -Algorithm SHA256).Hash
if ($acActual -ne $acGolden.census_sha256) { throw "Pristine baseline mismatch: $acActual. Investigate the first differing frame; do not regenerate the golden automatically." }
"PASS: exact frozen pristine baseline ($($acGolden.frames) frames)"
