# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $RomPath,
    [Parameter(Mandatory)][string] $VideoDirectory
)
$ErrorActionPreference = 'Stop'
$acRom = (Resolve-Path -LiteralPath $RomPath).Path
$acDirectory = (Resolve-Path -LiteralPath $VideoDirectory).Path
if ((Get-FileHash -LiteralPath $acRom -Algorithm SHA256).Hash -ne '1AF27C808BE34E0C2FE435C169F5169417D6362DEE0E928413581A1F6126557F') { throw 'Unsupported Alien Crush ROM' }
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Collections.Generic;
public static class AlienVideoRomMatches {
    // Search at every ROM byte: loaders need not align ROM data to tile boundaries.
    // Skip constant blocks; they provide no useful source-location evidence.
    public static string[] Find(byte[] rom, byte[] video) {
        var rows=new List<string>(); rows.Add("vram_byte_offset,rom_byte_offset,matching_bytes");
        for(int at=0;at+32<=video.Length;at+=32) {
            bool varied=false;
            for(int i=1;i<32;i++) if(video[at+i]!=video[at]) {varied=true;break;}
            if(!varied)continue;
            int best=0,bestAt=0;
            for(int source=0;source+32<=rom.Length;source++) {
                if(rom[source]!=video[at] || rom[source+1]!=video[at+1] || rom[source+2]!=video[at+2] || rom[source+3]!=video[at+3])continue;
                int length=4,limit=Math.Min(rom.Length-source,video.Length-at);
                while(length<limit && rom[source+length]==video[at+length])length++;
                if(length>=32 && length>best) {best=length;bestAt=source;}
            }
            if(best>=32) {
                rows.Add(String.Format("0x{0:X4},0x{1:X5},{2}",at,bestAt,best));
                at+=((best/32)-1)*32;
            }
        }
        return rows.ToArray();
    }
}
'@
$acVideo=[IO.File]::ReadAllBytes((Join-Path $acDirectory 'vram.bin'))
if ($acVideo.Length -ne 65536) { throw 'Expected 65536-byte video RAM snapshot' }
$acRows=[AlienVideoRomMatches]::Find([IO.File]::ReadAllBytes($acRom),$acVideo)
$acRows | Set-Content -LiteralPath (Join-Path $acDirectory 'rom-matches.csv') -Encoding ASCII
"Found $($acRows.Length-1) exact runs. Repeated patterns may have multiple sources; a match is a candidate, not proof of the loader path."
