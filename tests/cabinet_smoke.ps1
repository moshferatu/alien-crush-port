# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$Rom,
      [Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference='Stop'
$env:SDL_VIDEODRIVER='dummy'
$env:SDL_AUDIODRIVER='dummy'
$directory=Join-Path $OutputRoot ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$config=Join-Path $directory 'cabinet-windows.txt'
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Initial cabinet smoke run failed' }
$lines=[IO.File]::ReadAllLines($config)
if($lines.Count -ne 4 -or $lines[0] -ne 'ACWINDOWS 1') { throw 'Missing cabinet placements' }
foreach($role in 'playfield','backglass','score') {
    if(-not ($lines | Where-Object { $_.StartsWith("$role ") })) { throw "Missing $role window" }
}
# A disconnected monitor and off-screen geometry must recover to a usable display.
$lines[1]='playfield "Disconnected test monitor" 99000 -99000 540 720 0 90'
[IO.File]::WriteAllLines($config,$lines)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Cabinet restore smoke run failed' }
$restored=[IO.File]::ReadAllLines($config)
if($restored[1] -match 'Disconnected|99000' -or -not $restored[1].EndsWith(' 90')) {
    throw 'Missing-monitor recovery or rotation persistence failed'
}
# Invalid configuration must fail clearly and remain available for correction.
$invalid="ACWINDOWS 999`n"
[IO.File]::WriteAllText($config,$invalid)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid cabinet configuration was accepted' }
if([IO.File]::ReadAllText($config) -ne $invalid) { throw 'Invalid configuration was overwritten' }
Write-Output 'PASS: three windows, layout restore, missing-monitor recovery, rotation and invalid-config preservation'
exit 0
