# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only inspection of ac-tools work-ram.bin captures (one 8 KiB block/frame).
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $RamPath,
    [ValidateRange(1,2147483647)][int] $FirstFrame=1,
    [ValidateRange(1,2147483647)][int] $LastFrame=2147483647
)
$ErrorActionPreference='Stop'
$acFile=Get-Item -LiteralPath $RamPath
if($acFile.Length -eq 0 -or $acFile.Length%8192 -ne 0 -or $acFile.Length -gt 64MB) {
    throw 'Expected a nonempty capture of complete 8192-byte RAM frames, at most 64 MiB'
}
$acFrameCount=[int]($acFile.Length/8192)
if($FirstFrame -gt $acFrameCount -or $LastFrame -lt $FirstFrame) {throw 'Invalid capture frame range'}
$acRam=[IO.File]::ReadAllBytes($acFile.FullName)
function Read-Word([int]$Offset,[int]$Low,[int]$High) {
    return [int]$acRam[$Offset+$Low]+256*[int]$acRam[$Offset+$High]
}
function Read-SignedWord([int]$Offset,[int]$Low,[int]$High) {
    $acValue=Read-Word $Offset $Low $High
    if($acValue -ge 32768) {$acValue-=65536}
    return $acValue
}
# Actor type 06 dispatches to 01:4E23. Do not assume its slot or that its
# presence alone authorizes launch input: stored/scene state still matters.
& {
    for($acFrame=$FirstFrame;$acFrame -le [Math]::Min($LastFrame,$acFrameCount);$acFrame++) {
        $acOffset=($acFrame-1)*8192
        for($acSlot=4;$acSlot -lt 64;$acSlot++) {
            if($acRam[$acOffset+0x1500+$acSlot] -ne 6) {continue}
            [pscustomobject][ordered]@{
                frame=$acFrame; slot=$acSlot
                table=$acRam[$acOffset+0x79]; mode=$acRam[$acOffset+0x67]
                scene53=$acRam[$acOffset+0x53]; scene54=$acRam[$acOffset+0x54]
                pad22=$acRam[$acOffset+0x22]; ball_type=$acRam[$acOffset+0x1500]
                flags=$acRam[$acOffset+0x1540+$acSlot]
                phase=$acRam[$acOffset+0x1840+$acSlot]
                timer=$acRam[$acOffset+0x1880+$acSlot]
                contact=$acRam[$acOffset+0x1ac0+$acSlot]
                travel_raw=(Read-Word ($acOffset+$acSlot) 0x1c00 0x1c40)
                travel_step_raw=(Read-SignedWord ($acOffset+$acSlot) 0x1b80 0x1bc0)
                velocity_y_raw=(Read-SignedWord ($acOffset+$acSlot) 0x1780 0x17c0)
                release_factor_raw=(Read-Word $acOffset 0xa6 0xa7)
                ball_x=(Read-Word $acOffset 0x15c0 0x1600)
                ball_y=(Read-Word $acOffset 0x1680 0x16c0)
                ball_velocity_y_raw=(Read-SignedWord $acOffset 0x1780 0x17c0)
            }
        }
    }
} | ConvertTo-Csv -NoTypeInformation
