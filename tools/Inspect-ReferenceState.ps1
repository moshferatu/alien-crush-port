# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $PayloadPath,
    [string] $OutputDirectory = 'out/state-inspection'
)
$ErrorActionPreference = 'Stop'
# This is the pinned Mednafen named-field serialization format, not game state.
# Work on a raw checkpoint.payload emitted by alien-crush-tools --verify-replay.
$acInput = (Resolve-Path -LiteralPath $PayloadPath).Path
if ((Get-Item -LiteralPath $acInput).Length -gt 64MB) { throw 'State exceeds size limit' }
$acBytes = [IO.File]::ReadAllBytes($acInput)
if ($acBytes.Length -lt 32 -or [Text.Encoding]::ASCII.GetString($acBytes,0,8) -ne 'MDFNSVST') { throw 'Expected a raw reference payload, not an .acrec or .state container' }
$acEnd = [BitConverter]::ToUInt32($acBytes,20)
if ($acEnd -lt 32 -or $acEnd -gt $acBytes.Length) { throw 'Truncated reference state' }
$acOutput = [IO.Path]::GetFullPath($OutputDirectory)
$acFields = [Collections.Generic.List[object]]::new()
$acNames = [Collections.Generic.HashSet[string]]::new()
$acPos = 32
while ($acPos -lt $acEnd) {
    if ($acEnd-$acPos -lt 36) { throw 'Truncated section header' }
    $acSection = [Text.Encoding]::ASCII.GetString($acBytes,$acPos,32).TrimEnd([char]0)
    $acSize = [BitConverter]::ToUInt32($acBytes,$acPos+32)
    $acPos += 36
    if ($acSize -gt $acEnd-$acPos) { throw 'Truncated section' }
    $acSectionEnd = $acPos+$acSize
    while ($acPos -lt $acSectionEnd) {
        $acNameSize = [int]$acBytes[$acPos++]
        if ($acNameSize+4 -gt $acSectionEnd-$acPos) { throw 'Truncated field header' }
        $acName = [Text.Encoding]::ASCII.GetString($acBytes,$acPos,$acNameSize)
        $acPos += $acNameSize
        $acSize = [BitConverter]::ToUInt32($acBytes,$acPos)
        $acPos += 4
        if ($acSize -gt $acSectionEnd-$acPos) { throw 'Truncated field data' }
        $acFile = ($acSection+'--'+$acName -replace '[^A-Za-z0-9_-]','_')+'.bin'
        if (-not $acNames.Add($acFile)) { throw "Duplicate output field: $acFile" }
        $acValue = $null
        if ($acSize -eq 1) { $acValue = [uint64]$acBytes[$acPos] }
        elseif ($acSize -eq 2) { $acValue = [uint64][BitConverter]::ToUInt16($acBytes,$acPos) }
        elseif ($acSize -eq 4) { $acValue = [uint64][BitConverter]::ToUInt32($acBytes,$acPos) }
        $acFields.Add([pscustomobject]@{Section=$acSection;Field=$acName;Offset=$acPos;Bytes=$acSize;UnsignedValue=$acValue;File=$acFile})
        $acPos += $acSize
    }
}
# Validate the whole structure before creating outputs.
New-Item -ItemType Directory -Path $acOutput -Force | Out-Null
foreach ($acField in $acFields) {
    $acData = [byte[]]::new($acField.Bytes)
    [Array]::Copy($acBytes,$acField.Offset,$acData,0,$acField.Bytes)
    [IO.File]::WriteAllBytes((Join-Path $acOutput $acField.File),$acData)
}
$acFields | Export-Csv -LiteralPath (Join-Path $acOutput 'fields.csv') -NoTypeInformation
[pscustomobject]@{PayloadSha256=(Get-FileHash -LiteralPath $acInput -Algorithm SHA256).Hash; SerializedBytes=$acEnd; Fields=$acFields.Count} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $acOutput 'manifest.json') -Encoding UTF8
"Extracted $($acFields.Count) named diagnostic fields to $acOutput"
