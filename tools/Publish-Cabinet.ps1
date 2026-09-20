# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Build and publish a portable cabinet folder for Syncthing.
.DESCRIPTION
Uses the static windows-cabinet Release preset. Defaults to the sibling
alien-crush-cabinet folder. Updates packaged files without deleting local files
or touching portable-data. Run from any working directory.
.EXAMPLE
.\tools\Publish-Cabinet.ps1
.EXAMPLE
.\tools\Publish-Cabinet.ps1 -Destination 'E:\Syncthing\Alien Crush' -SkipBuild
#>
[CmdletBinding()]
param(
    [string] $Destination,
    [string] $RomPath,
    [switch] $SkipBuild,
    [string] $CMakePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
if (-not $Destination) {
    $Destination = Join-Path (Split-Path -Parent $repositoryRoot) 'alien-crush-cabinet'
}
if (-not $RomPath) { $RomPath = Join-Path $repositoryRoot 'Alien Crush.pce' }
$destinationFull = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination).TrimEnd('\', '/')
$workspaceFull = [IO.Path]::GetFullPath($repositoryRoot).TrimEnd('\', '/')
# A sibling is safe, but a workspace child, workspace ancestor or drive root
# is not a dedicated publish directory. No destination tree is ever removed.
if ($destinationFull -eq $workspaceFull -or
    $destinationFull.StartsWith($workspaceFull + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $workspaceFull.StartsWith($destinationFull + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $destinationFull -eq [IO.Path]::GetPathRoot($destinationFull).TrimEnd('\', '/')) {
    throw "Choose a dedicated folder outside the workspace: $destinationFull"
}
$romFull = (Resolve-Path -LiteralPath $RomPath).ProviderPath
if ((Get-Item -LiteralPath $romFull).Length -ne 262144 -or
    (Get-FileHash -LiteralPath $romFull -Algorithm SHA256).Hash -ne
    '1AF27C808BE34E0C2FE435C169F5169417D6362DEE0E928413581A1F6126557F') {
    throw 'The ROM must be the supported Alien Crush revision (see README.md).'
}
if (-not $CMakePath) {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { $CMakePath = $command.Source }
    else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $found = @(& $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
            if ($found.Count) { $CMakePath = $found[0] }
        }
    }
}
if (-not $CMakePath) { throw 'CMake not found. Install the Visual Studio CMake tools or supply -CMakePath.' }
$CMakePath = (Get-Command $CMakePath -ErrorAction Stop).Source
$buildRoot = Join-Path $repositoryRoot 'out/build/windows-cabinet'
$stage = $null
Push-Location -LiteralPath $repositoryRoot
try {
    if (-not $SkipBuild) {
        & $CMakePath --preset windows-cabinet
        if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)." }
        & $CMakePath --build --preset windows-cabinet --target alien-crush --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
    }
    $cache = Join-Path $buildRoot 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache) -or
        -not (Select-String -LiteralPath $cache -Pattern '^CMAKE_MSVC_RUNTIME_LIBRARY:.*=MultiThreaded$' -Quiet) -or
        -not (Select-String -LiteralPath $cache -Pattern '^SDL_STATIC:BOOL=ON$' -Quiet) -or
        -not (Select-String -LiteralPath $cache -Pattern '^SDL_SHARED:BOOL=OFF$' -Quiet)) {
        throw 'A static windows-cabinet build is required. Run without -SkipBuild.'
    }
    # Finish the build/install in a fresh local staging directory before updating
    # the folder watched by Syncthing. Only Runtime-component files are packaged.
    $stage = Join-Path $buildRoot ('publish-' + [Guid]::NewGuid().ToString('N'))
    & $CMakePath --install $buildRoot --config Release --component Runtime --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw "Install failed ($LASTEXITCODE)." }
    if (-not (Test-Path -LiteralPath (Join-Path $stage 'alien-crush.exe'))) {
        throw 'Install did not produce alien-crush.exe.'
    }
    # Normalize the ROM name to the host default even when the input is renamed.
    Copy-Item -LiteralPath $romFull -Destination (Join-Path $stage 'Alien Crush.pce')
    $utf8 = New-Object Text.UTF8Encoding $false
    [IO.File]::WriteAllText((Join-Path $stage 'Launch-Cabinet.cmd'), "@echo off`r`n`"%~dp0alien-crush.exe`" --cabinet %*`r`nexit /b %errorlevel%`r`n", $utf8)
    $patterns = @'
// Machine-local layout, input/plunger bindings, artwork, high scores and quick states.
/portable-data
/logs
/captures
/traces
*.state
*.acrec
'@
    [IO.File]::WriteAllText((Join-Path $stage 'syncthing-ignore.txt'), $patterns + "`r`n", $utf8)
    $instructions = @'
Alien Crush cabinet package

Run Launch-Cabinet.cmd, or alien-crush.exe --cabinet.
The ROM is included under its default name. No extra runtime DLLs are needed.
Close the game before applying an update on the cabinet.

Syncthing setup (do this on BOTH computers before the first sync):
1. Select this package folder as the Syncthing folder root.
2. In Edit Folder > Ignore Patterns add these lines:
   /portable-data
   /logs
   /captures
   /traces
   *.state
   *.acrec
   Alternatively, after syncthing-ignore.txt exists locally, use:
   #include syncthing-ignore.txt
3. Start syncing, then run the game on the cabinet.

.stignore is local to each device; Syncthing does NOT transfer it.
The publish script configures it on the workstation. Configure the cabinet too.
Reference: https://docs.syncthing.net/users/ignoring.html

Layout, button/plunger bindings, custom backglass.bmp, high scores and quick states live in
portable-data beside the executable. They stay local to each computer.
High scores and initials automatically save to portable-data/alien-crush.hiscore.
Publishing updates packaged files and preserves this directory and other local
files. If needed, copy cabinet-specific bindings/artwork manually once.
See README.md for controls, monitor setup and configuration file formats.
Window placement, raw buttons, physical nudge and plunger settings share portable-data/cabinet.ini.
PinOne accelerometer nudging works automatically in cabinet mode (axes 0/1).
Optional input.nudge.* settings adjust sensitivity; see README.md.
Older separate configuration files are imported on startup and retained as backups.
Normal exit writes the combined settings. Edit plunger values in this main INI.
'@
    [IO.File]::WriteAllText((Join-Path $stage 'CABINET-SETUP.txt'), $instructions + "`r`n", $utf8)
    New-Item -ItemType Directory -Path $destinationFull -Force | Out-Null
    # Make the included file available before creating/updating .stignore.
    Copy-Item -LiteralPath (Join-Path $stage 'syncthing-ignore.txt') -Destination $destinationFull -Force
    $ignorePath = Join-Path $destinationFull '.stignore'
    $include = '#include syncthing-ignore.txt'
    $existing = if (Test-Path -LiteralPath $ignorePath) { [IO.File]::ReadAllText($ignorePath) } else { '' }
    if ($existing -notmatch '(?m)^#include syncthing-ignore\.txt\s*$') {
        $attributes = if (Test-Path -LiteralPath $ignorePath) { (Get-Item -LiteralPath $ignorePath -Force).Attributes } else { [IO.FileAttributes]::Hidden }
        if (Test-Path -LiteralPath $ignorePath) { (Get-Item -LiteralPath $ignorePath -Force).Attributes = [IO.FileAttributes]::Normal }
        try { [IO.File]::WriteAllText($ignorePath, $include + "`r`n" + $existing, $utf8) }
        finally { (Get-Item -LiteralPath $ignorePath -Force).Attributes = $attributes }
    }
    # Copy only package files, never mirror or delete the destination contents.
    Get-ChildItem -LiteralPath $stage -File | Where-Object Name -ne 'syncthing-ignore.txt' |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $destinationFull -Force }
    Write-Host "Published $destinationFull" -ForegroundColor Green
    Write-Host 'Run Launch-Cabinet.cmd. See CABINET-SETUP.txt for Syncthing setup on both machines.'
} finally {
    Pop-Location
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        $stageFull = (Resolve-Path -LiteralPath $stage).ProviderPath
        $allowedRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\', '/') + '\publish-'
        if (-not $stageFull.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean unexpected staging path: $stageFull"
        }
        Remove-Item -LiteralPath $stageFull -Recurse -Force
    }
}
