# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $MesenPath,
    [Parameter(Mandatory)][string] $RomPath,
    [string] $OutputDirectory = 'out/mesen/baseline',
    [ValidateRange(2,100000)][int] $Frames = 1800,
    [ValidateRange(0,10)][int] $TraceVideoLastFrames = 0,
    [ValidateRange(0,120)][int] $TraceActorsLastFrames = 0,
    [ValidateRange(0,120)][int] $TraceLauncherLastFrames = 0,
    [ValidateRange(0,210)][int] $LauncherHoldFrames = 0
)
$ErrorActionPreference = 'Stop'
$acMesen = (Resolve-Path -LiteralPath $MesenPath).Path
$acRom = (Resolve-Path -LiteralPath $RomPath).Path
if ((Get-FileHash -LiteralPath $acMesen -Algorithm SHA256).Hash -ne 'D2EB03C2590C648BF329F127EBCFEFD70130E7690A9E2CCDBA8616FAEA1FE96B') { throw 'Expected pinned Mesen 2.1.1 executable' }
if ((Get-FileHash -LiteralPath $acRom -Algorithm SHA256).Hash -ne '1AF27C808BE34E0C2FE435C169F5169417D6362DEE0E928413581A1F6126557F') { throw 'Unsupported Alien Crush ROM' }
$acOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$acRuntime = Join-Path $acOutput 'runtime'
New-Item -ItemType Directory -Path $acRuntime -Force | Out-Null
$acComplete = Join-Path $acOutput 'complete.txt'
if (Test-Path -LiteralPath $acComplete) { Remove-Item -LiteralPath $acComplete }
foreach ($acName in @('Mesen.exe','MesenCore.dll','libHarfBuzzSharp.dll','libSkiaSharp.dll')) {
    Copy-Item -LiteralPath (Join-Path (Split-Path $acMesen) $acName) -Destination (Join-Path $acRuntime $acName) -Force
}
$acPalette = @(for ($acColor=0; $acColor -lt 512; $acColor++) {
    $acR = (36*(($acColor -shr 3) -band 7)) -shr 3
    $acG = (36*(($acColor -shr 6) -band 7)) -shr 2
    $acB = (36*($acColor -band 7)) -shr 3
    [uint32](0xFF000000L -bor ((($acR -shl 3) -bor ($acR -shr 2)) -shl 16) -bor ((($acG -shl 2) -bor ($acG -shr 4)) -shl 8) -bor ($acB -shl 3) -bor ($acB -shr 2))
})
$acSettings = @{
    Version='2.1.1'
    PcEngine=@{
        Port1=@{Type='PceTurboTap'}; Port1A=@{Type='PceController'}; Port1B=@{Type='PceController'}
        Port1C=@{Type='PceController'}; Port1D=@{Type='PceController'}; Port1E=@{Type='PceController'}
        RamPowerOnState='Random'; EnableRandomPowerOnState=$false
        UseHuC6280aAudio=$true; RemoveSpriteLimit=$false; DisableFrameSkipping=$true; Palette=$acPalette
    }
    Debug=@{ScriptWindow=@{AllowIoOsAccess=$true; AllowNetworkAccess=$false; ScriptTimeout=10}}
}
$acSettings | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $acRuntime 'settings.json') -Encoding UTF8
$acOldOutput=$env:AC_MESEN_OUTPUT
$acOldFrames=$env:AC_MESEN_FRAMES
$acOldTrace=$env:AC_MESEN_TRACE_VIDEO
$acOldActors=$env:AC_MESEN_TRACE_ACTORS
$acOldLauncher=$env:AC_MESEN_TRACE_LAUNCHER
$acOldHold=$env:AC_MESEN_LAUNCHER_HOLD
try {
    $env:AC_MESEN_OUTPUT=$acOutput.Replace('\','/')
    $env:AC_MESEN_FRAMES="$Frames"
    $env:AC_MESEN_TRACE_VIDEO="$TraceVideoLastFrames"
    $env:AC_MESEN_TRACE_ACTORS="$TraceActorsLastFrames"
    $env:AC_MESEN_TRACE_LAUNCHER="$TraceLauncherLastFrames"
    $env:AC_MESEN_LAUNCHER_HOLD="$LauncherHoldFrames"
    $acScript=Join-Path $PSScriptRoot 'mesen/capture.lua'
    $acProcess=Start-Process -FilePath (Join-Path $acRuntime 'Mesen.exe') -ArgumentList ('--testRunner --timeout=120 "{0}" "{1}"' -f $acScript,$acRom) -WindowStyle Hidden -PassThru
    if (-not $acProcess.WaitForExit(130000)) { $acProcess.Kill(); throw 'Mesen capture timed out' }
    if ($acProcess.ExitCode -ne 0) { throw "Mesen capture failed: $($acProcess.ExitCode)" }
    if (-not (Test-Path -LiteralPath (Join-Path $acOutput 'complete.txt'))) { throw 'Mesen did not finish capture' }
    Get-Content -LiteralPath (Join-Path $acOutput 'complete.txt')
} finally {
    $env:AC_MESEN_OUTPUT=$acOldOutput
    $env:AC_MESEN_FRAMES=$acOldFrames
    $env:AC_MESEN_TRACE_VIDEO=$acOldTrace
    $env:AC_MESEN_TRACE_ACTORS=$acOldActors
    $env:AC_MESEN_TRACE_LAUNCHER=$acOldLauncher
    $env:AC_MESEN_LAUNCHER_HOLD=$acOldHold
}
