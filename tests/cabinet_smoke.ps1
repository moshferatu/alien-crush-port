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
$config=Join-Path $directory 'cabinet.ini'
$recording=Join-Path $directory 'session.acrec'
function Set-Fields([hashtable]$fields) {
    $lines=@([IO.File]::ReadAllLines($config) | Where-Object { -not $fields.ContainsKey(($_ -split '=',2)[0]) })
    foreach($key in $fields.Keys) { $lines+="$key=$($fields[$key])" }
    [IO.File]::WriteAllLines($config,$lines)
}
function Get-Field([string]$key) {
    $line=[IO.File]::ReadAllLines($config) | Where-Object { $_.StartsWith("$key=") }
    if($line) { return ($line -split '=',2)[1] }
}
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --record $recording
if($LASTEXITCODE -ne 0) { throw 'Initial cabinet smoke run failed' }
$lines=[IO.File]::ReadAllLines($config)
if($lines[0] -ne 'version=1') { throw 'cabinet.ini is not in the current format' }
foreach($role in 'playfield','backglass','dmd') {
    if([int](Get-Field "$role.width") -le 0 -or -not (Get-Field "$role.display")) { throw "Missing $role window" }
}
foreach($oldFile in 'cabinet-windows.txt','cabinet-inputs.txt','cabinet-plunger.txt') {
    if(Test-Path (Join-Path $directory $oldFile)) { throw 'Fresh cabinets must use only cabinet.ini' }
}
if((Get-Field 'input.plunger.axis') -ne '-1' -or $null -eq (Get-Field 'input.buttons.device') -or (Get-Field 'input.one') -ne 'Return') {
    throw 'Main cabinet.ini must include key, button and plunger settings'
}
# A disconnected monitor and off-screen geometry must recover to a usable display.
Set-Fields @{'playfield.display'='Disconnected test monitor'; 'playfield.x'='99000'; 'playfield.y'='-99000'; 'playfield.artwork_rotation'='90'}
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Cabinet restore smoke run failed' }
if((Get-Field 'playfield.display') -match 'Disconnected' -or (Get-Field 'playfield.x') -eq '99000' -or (Get-Field 'playfield.artwork_rotation') -ne '90') {
    throw 'Missing-monitor recovery or rotation persistence failed'
}
$restored=[IO.File]::ReadAllLines($config)
# Invalid configuration must fail clearly and remain available for correction.
$invalid="version=1`nplayfield.artwork_rotation=45`n"
[IO.File]::WriteAllText($config,$invalid)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid cabinet configuration was accepted' }
if([IO.File]::ReadAllText($config) -ne $invalid) { throw 'Invalid configuration was overwritten' }
# The older format and its separate files are read, left alone, and their
# settings carried into the current cabinet.ini on exit.
$legacy="# Alien Crush cabinet settings`ninput.plunger.axis=-1`nwindow.playfield=`"Test`" 0 0 540 720 0 90`nwindow.backglass=`"Test`" 0 0 640 360 0 0`nwindow.score=`"Test`" 0 0 640 200 0 0`n"
[IO.File]::WriteAllText($config,$legacy)
$inputConfig=Join-Path $directory 'cabinet-inputs.txt'
$validInput="ACINPUTS1`n`"Absent test cabinet board`"`n0 1 2 3 4 -1 -1 -1`n"
[IO.File]::WriteAllText($inputConfig,$validInput)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Absent configured controller must allow startup' }
if([IO.File]::ReadAllText($inputConfig) -ne $validInput) { throw 'Input settings were overwritten' }
if((Get-Field 'version') -ne '1' -or (Get-Field 'input.buttons.device') -ne 'Absent test cabinet board' -or
    (Get-Field 'input.buttons.nudge') -ne '2' -or (Get-Field 'input.buttons.launch') -ne '-1' -or (Get-Field 'playfield.artwork_rotation') -ne '90') {
    throw 'Legacy buttons and windows were not migrated into cabinet.ini'
}
$invalidInput="ACINPUTS1`n`"Test board`"`n256 1 2 3 4 -1 -1 -1`n"
[IO.File]::WriteAllText($inputConfig,$invalidInput)
[IO.File]::WriteAllText($config,$legacy)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -eq 0) { throw 'Invalid input settings were accepted' }
if([IO.File]::ReadAllText($inputConfig) -ne $invalidInput) { throw 'Invalid input settings were overwritten' }
if([IO.File]::ReadAllText($config) -ne $legacy) { throw 'An unreadable older configuration was overwritten' }
[IO.File]::WriteAllText($inputConfig,$validInput)
$plungerConfig=Join-Path $directory 'cabinet-plunger.txt'
$validPlunger="ACPLUNGER1`n`"Absent test cabinet board`"`n2 1000 -1000 8 25`n"
[IO.File]::WriteAllText($plungerConfig,$validPlunger)
[IO.File]::WriteAllText($config,($legacy -replace "input.plunger.axis=-1`n",''))
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Absent configured plunger must allow startup' }
if([IO.File]::ReadAllText($plungerConfig) -ne $validPlunger) { throw 'Plunger settings were overwritten' }
if((Get-Field 'input.plunger.drawn') -ne '-1000') { throw 'Legacy plunger calibration was not migrated into cabinet.ini' }
# Once migrated, changes to obsolete files cannot override the shared settings.
[IO.File]::WriteAllText($inputConfig,'obsolete invalid buttons')
[IO.File]::WriteAllText($plungerConfig,'obsolete invalid plunger')
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory
if($LASTEXITCODE -ne 0) { throw 'Shared cabinet.ini did not take precedence over old files' }
# A replay reads no hardware settings, even ones that cannot be read.
[IO.File]::WriteAllText($config,$legacy)
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --replay $recording
if($LASTEXITCODE -ne 0) { throw 'Replay must ignore hardware configuration' }
[IO.File]::WriteAllLines($config,$restored)
# Optional local artwork is read in both live and replay cabinet sessions.
$artPath=Join-Path $directory 'backglass.bmp'
$art=New-Object byte[] 62
$art[0]=66; $art[1]=77; $art[2]=62; $art[10]=54; $art[14]=40
$art[18]=2; $art[22]=1; $art[26]=1; $art[28]=24
$art[56]=255; $art[58]=255
[IO.File]::WriteAllBytes($artPath,$art)
Set-Fields @{'backglass.artwork_rotation'='90'}
& $Executable --rom $Rom --cabinet --frames 3 --data-dir $directory --replay $recording
if($LASTEXITCODE -ne 0) { throw 'Cabinet artwork replay smoke failed' }
if((Get-Field 'backglass.artwork_rotation') -ne '90') { throw 'Backglass rotation was not preserved' }
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
Write-Output 'PASS: three windows, layout recovery, format migration, local artwork, input/plunger preservation and replay hardware isolation'
exit 0
