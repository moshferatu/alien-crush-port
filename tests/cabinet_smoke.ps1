# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$Rom,
      [Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference='Stop'
$usage=& $Executable --help
if($LASTEXITCODE -ne 0 -or ($usage -match '--stitched|V toggles')) { throw 'Host help still advertises optional stitching' }
$env:SDL_VIDEODRIVER='dummy'
$env:SDL_AUDIODRIVER='dummy'
$directory=Join-Path $OutputRoot ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$config=Join-Path $directory 'cabinet-windows.txt'
$recording=Join-Path $directory 'session.acrec'
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --record $recording
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
# Raw input settings are user-authored and must survive both accepted and rejected runs.
[IO.File]::WriteAllLines($config,$restored)
$inputConfig=Join-Path $directory 'cabinet-inputs.txt'
$validInput="ACINPUTS1`n`"Absent test cabinet board`"`n0 1 2 3 4 -1 -1 -1`n"
[IO.File]::WriteAllText($inputConfig,$validInput)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Absent configured controller must allow startup' }
if([IO.File]::ReadAllText($inputConfig) -ne $validInput) { throw 'Input settings were overwritten' }
$invalidInput="ACINPUTS1`n`"Test board`"`n256 1 2 3 4 -1 -1 -1`n"
[IO.File]::WriteAllText($inputConfig,$invalidInput)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid input settings were accepted' }
if([IO.File]::ReadAllText($inputConfig) -ne $invalidInput) { throw 'Invalid input settings were overwritten' }
[IO.File]::WriteAllText($inputConfig,$validInput)
$plungerConfig=Join-Path $directory 'cabinet-plunger.txt'
if(Test-Path -LiteralPath $plungerConfig) { throw 'Missing plunger config must not be created automatically' }
$validPlunger="ACPLUNGER1`n`"Absent test cabinet board`"`n2 1000 -1000 8 25`n"
[IO.File]::WriteAllText($plungerConfig,$validPlunger)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Absent configured plunger must allow startup' }
if([IO.File]::ReadAllText($plungerConfig) -ne $validPlunger) { throw 'Plunger settings were overwritten' }
$invalidPlunger="ACPLUNGER1`n`"Test board`"`n2 0 0 8 25`n"
[IO.File]::WriteAllText($plungerConfig,$invalidPlunger)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid plunger settings were accepted' }
if([IO.File]::ReadAllText($plungerConfig) -ne $invalidPlunger) { throw 'Invalid plunger settings were overwritten' }
[IO.File]::WriteAllText($inputConfig,$invalidInput)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --replay $recording
if($LASTEXITCODE -ne 0) { throw 'Replay must ignore hardware configuration' }
# Optional local artwork is read in both live and replay cabinet sessions.
$artPath=Join-Path $directory 'backglass.bmp'
$art=New-Object byte[] 62
$art[0]=66; $art[1]=77; $art[2]=62; $art[10]=54; $art[14]=40
$art[18]=2; $art[22]=1; $art[26]=1; $art[28]=24
$art[56]=255; $art[58]=255
[IO.File]::WriteAllBytes($artPath,$art)
$placements=[IO.File]::ReadAllLines($config)
$placements[2]=$placements[2] -replace ' 0$', ' 90'
[IO.File]::WriteAllLines($config,$placements)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --replay $recording
if($LASTEXITCODE -ne 0) { throw 'Cabinet artwork replay smoke failed' }
if(-not ([IO.File]::ReadAllLines($config)[2]).EndsWith(' 90')) { throw 'Backglass rotation was not preserved' }
[IO.File]::WriteAllText($inputConfig,$validInput)
[IO.File]::WriteAllText($plungerConfig,$validPlunger)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Cabinet artwork live smoke failed' }
if([Convert]::ToBase64String([IO.File]::ReadAllBytes($artPath)) -ne [Convert]::ToBase64String($art)) {
    throw 'Local artwork was modified'
}
[IO.File]::WriteAllText($artPath,'invalid artwork')
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid cabinet artwork was accepted' }
if([IO.File]::ReadAllText($artPath) -ne 'invalid artwork') { throw 'Invalid artwork was modified' }
& $Executable --rom $Rom --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Desktop mode must ignore cabinet artwork' }
Write-Output 'PASS: three windows, layout recovery, local artwork, input/plunger preservation and replay hardware isolation'
exit 0
