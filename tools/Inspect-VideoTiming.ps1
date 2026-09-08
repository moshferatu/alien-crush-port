# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([Parameter(Mandatory)][string] $TraceDirectory)
$ErrorActionPreference='Stop'
$acDirectory=(Resolve-Path -LiteralPath $TraceDirectory).Path
$acManifest=@{}
foreach($acLine in Get-Content -LiteralPath (Join-Path $acDirectory 'video-trace-manifest.txt')) {
    $acParts=$acLine -split '=',2
    if($acParts.Length -ne 2 -or $acManifest.ContainsKey($acParts[0])) { throw 'Invalid video trace manifest' }
    $acManifest[$acParts[0]]=$acParts[1]
}
if($acManifest.schema -ne 'video-bus-timing-v1' -or $acManifest.backend -ne 'Mesen-2.1.1' -or
   $acManifest.rom_sha256 -ne '1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f') {
    throw 'Unsupported video timing trace'
}
function Read-Decimal([string] $Text,[long] $Maximum) {
    if($Text -notmatch '^[0-9]+$') { throw "Invalid decimal trace value: $Text" }
    $acNumber=[long]::Parse($Text,[Globalization.CultureInfo]::InvariantCulture)
    if($acNumber -gt $Maximum) { throw 'Decimal trace value exceeds bounds' }
    return $acNumber
}
function Read-Hex([string] $Text,[int] $Maximum) {
    if($Text -notmatch '^[0-9A-Fa-f]+$') { throw "Invalid hex trace value: $Text" }
    $acNumber=[Convert]::ToInt32($Text,16)
    if($acNumber -lt 0 -or $acNumber -gt $Maximum) { throw 'Hex trace value exceeds bounds' }
    return $acNumber
}
$acFirst=Read-Decimal $acManifest.first_frame 100000
$acLast=Read-Decimal $acManifest.last_frame 100000
if($acFirst -lt 1 -or $acLast -lt $acFirst -or $acLast-$acFirst -ge 10) { throw 'Invalid trace frame window' }
$acPath=Join-Path $acDirectory 'video-writes.csv'
if((Get-Item -LiteralPath $acPath).Length -gt 128MB) { throw 'Video trace exceeds 128 MiB limit' }
$acRows=@(Import-Csv -LiteralPath $acPath)
if($acRows.Count -eq 0 -or $acRows.Count -ne (Read-Decimal $acManifest.writes 1000000)) { throw 'Empty or incomplete video trace' }
$acClock=-1L
$acFrame=$acFirst
$acEvents=@(foreach($acRow in $acRows) {
    $acNextFrame=Read-Decimal $acRow.frame $acLast
    $acNextClock=Read-Decimal $acRow.clock ([long]::MaxValue)
    if($acNextFrame -lt $acFrame -or $acNextClock -lt $acClock) { throw 'Video trace is not chronological' }
    $acFrame=$acNextFrame; $acClock=$acNextClock
    $acScanline=Read-Decimal $acRow.scanline 262
    $acHclock=Read-Decimal $acRow.hclock 1364
    $acRcr=Read-Decimal $acRow.rcr 1023
    $acPort=Read-Hex $acRow.port 1029
    $acRegister=Read-Hex $acRow.register 31
    $acValue=Read-Hex $acRow.value 255
    $acMawr=Read-Hex $acRow.mawr 65535
    $acLatch=Read-Hex $acRow.vram_data 65535
    $acPalette=Read-Hex $acRow.palette_address 511
    $acColor=Read-Hex $acRow.palette_color 511
    $acPc=Read-Hex $acRow.pc 65535
    $acMpr=Read-Hex $acRow.mpr 255
    $acKind=$null; $acAddress=0; $acRequested=0
    if($acRow.device -eq 'VDC' -and $acPort -le 3) {
        if($acPort -eq 3 -and $acRegister -eq 2) {
            $acKind='VRAM-request'; $acAddress=$acMawr; $acRequested=($acValue -shl 8) -bor ($acLatch -band 255)
        } elseif($acPort -eq 2 -and $acRegister -eq 5) {
            $acKind='control-low'; $acAddress=5; $acRequested=$acValue
        }
    } elseif($acRow.device -eq 'VCE' -and $acPort -ge 1024 -and $acPort -le 1029) {
        if($acPort -eq 1028 -or $acPort -eq 1029) {
            $acKind='palette-byte'; $acAddress=$acPalette
            $acRequested=if($acPort -eq 1028){($acColor -band 256) -bor $acValue}else{($acColor -band 255) -bor (($acValue -band 1) -shl 8)}
        }
    } else { throw 'Unexpected video device/port' }
    if($null -ne $acKind) {
        [pscustomobject]@{frame=$acFrame;clock=$acClock;scanline=$acScanline;hclock=$acHclock;rcr=$acRcr;
            pc=('{0:X4}' -f $acPc);mpr=('{0:X2}' -f $acMpr);kind=$acKind;
            address=('{0:X4}' -f $acAddress);requested_value=('{0:X4}' -f $acRequested)}
    }
})
if($acEvents.Count -eq 0) { throw 'Trace contains no supported video data requests' }
$acOutput=Join-Path $acDirectory 'video-events.csv'
$acEvents | Export-Csv -LiteralPath $acOutput -NoTypeInformation -Encoding UTF8
"Validated $($acRows.Count) chronological bus writes; decoded $($acEvents.Count) data requests into $acOutput"
'Coordinates are raw Mesen values. VRAM requests can commit later; this report does not assign native render timing.'
