# Builds the plugin (build\dk2_eras_ui_improvements.dll) in Release with MSVC x64.
# -SymTest then dry-runs its init against the game's PDB with the mod loader's symtest.exe (build the loader first);
# it must exit 0 ("plugin init returned 0" and no PROBLEM lines; 100 means a call the loader would refuse).
param(
    [string]$Config = 'Release',
    [switch]$SymTest,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\DoorKickers2'
)

$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x64 build tools not found' }

$src = $PSScriptRoot
$build = Join-Path $src 'build'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
# vcvars calls vswhere by name
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"

cmd /c "`"$vcvars`" >nul && cmake -S `"$src`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config && cmake --build `"$build`""
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
$dll = Join-Path $build 'dk2_eras_ui_improvements.dll'
$dll

if ($SymTest) {
    $symtestExe = Join-Path $src '..\doorkickers-modloader\build\symtest.exe'
    if (-not (Test-Path $symtestExe)) { throw "symtest.exe not found ($symtestExe); build the mod loader first" }
    & $symtestExe $GameDir $dll # no trailing backslash: it would escape the closing quote; symtest adds one
    if ($LASTEXITCODE -ne 0) { throw "symtest dry run failed (exit code $LASTEXITCODE)" }
}
