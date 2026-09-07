# SPDX-License-Identifier: GPL-3.0-or-later
# HuC6280 opcode table adapted from Devil's Crush tools/Disassemble-Rom.py.
[CmdletBinding()]
param(
    [string] $RomPath='Alien Crush.pce',
    [Parameter(Mandatory)][string] $Bank,
    [Parameter(Mandatory)][string] $Pc,
    [ValidateRange(1,65536)][int] $Count=64
)
$ErrorActionPreference='Stop'
$acBytes=[System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $RomPath).Path)
if($acBytes.Length -ne 262144 -or (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne '1AF27C808BE34E0C2FE435C169F5169417D6362DEE0E928413581A1F6126557F') { throw 'Only the supported Alien Crush ROM is mapped' }
$acBank=[Convert]::ToInt32($Bank,16); $acPc=[Convert]::ToInt32($Pc,16)
if($acBank -lt 0 -or $acBank -ge 128 -or $acPc -lt 0 -or $acPc -gt 65535) {throw 'Expected a cartridge bank (00..7F) and 16-bit PC in hex'}
$acOps=@{}
Import-Csv (Join-Path $PSScriptRoot 'huc6280-opcodes.csv') | ForEach-Object { $acOps[[Convert]::ToInt32($_.opcode,16)]=$_ }
$acSizes=@{imp=1;acc=1;imm=2;zp=2;zpx=2;zpy=2;izp=2;izx=2;izy=2;abs=3;abx=3;aby=3;ind=3;iax=3;rel=2;zprel=3;imm_zp=3;imm_zpx=3;imm_abs=4;imm_abx=4;blk=7}
for($acInstruction=0;$acInstruction -lt $Count;$acInstruction++) {
    $acOffset=($acBank -band 31)*8192+($acPc -band 8191)
    $acOpcode=[int]$acBytes[$acOffset]; $acEntry=$acOps[$acOpcode]
    $acLength=1; $acMnemonic='.db'; $acOperand='$'+$acOpcode.ToString('X2')
    if($acEntry) {
        $acLength=$acSizes[$acEntry.mode]; $acMnemonic=$acEntry.mnemonic
        if(($acPc -band 8191)+$acLength -gt 8192) {throw 'Instruction crosses an MPR boundary; supply the next mapped bank explicitly'}
        $acRaw=@($acBytes[$acOffset..($acOffset+$acLength-1)])
        $acByte=if($acLength -ge 2){[int]$acRaw[1]}else{0}
        $acWord=if($acLength -ge 3){$acByte+256*[int]$acRaw[2]}else{0}
        $acHexByte='$'+$acByte.ToString('X2'); $acHexWord='$'+$acWord.ToString('X4')
        $acOperand=switch($acEntry.mode) {
            imp {''} acc {'A'} imm {'#'+$acHexByte} zp {$acHexByte} zpx {$acHexByte+',X'} zpy {$acHexByte+',Y'}
            izp {'('+$acHexByte+')'} izx {'('+$acHexByte+',X)'} izy {'('+$acHexByte+'),Y'}
            abs {$acHexWord} abx {$acHexWord+',X'} aby {$acHexWord+',Y'} ind {'('+$acHexWord+')'} iax {'('+$acHexWord+',X)'}
            rel { $acDisplacement=if($acByte -ge 128){$acByte-256}else{$acByte}; '$'+(($acPc+2+$acDisplacement) -band 65535).ToString('X4') }
            zprel { $acDisplacement=[int]$acRaw[2]; if($acDisplacement -ge 128){$acDisplacement-=256}; $acHexByte+',$'+(($acPc+3+$acDisplacement) -band 65535).ToString('X4') }
            imm_zp {'#'+$acHexByte+',$'+([int]$acRaw[2]).ToString('X2')}
            imm_zpx {'#'+$acHexByte+',$'+([int]$acRaw[2]).ToString('X2')+',X'}
            imm_abs {'#'+$acHexByte+',$'+([int]$acRaw[2]+256*[int]$acRaw[3]).ToString('X4')}
            imm_abx {'#'+$acHexByte+',$'+([int]$acRaw[2]+256*[int]$acRaw[3]).ToString('X4')+',X'}
            blk { ($(@(1,3,5) | ForEach-Object {'$'+([int]$acRaw[$_]+256*[int]$acRaw[$_+1]).ToString('X4')}) -join ',') }
        }
    }
    '{0:X2}:{1:X4}  {2,-20} {3,-5} {4}' -f $acBank,$acPc,([System.BitConverter]::ToString($acBytes,$acOffset,$acLength).Replace('-',' ')),$acMnemonic,$acOperand
    $acPc=($acPc+$acLength) -band 65535
    if(($acPc -band 8191) -eq 0) {break}
}
