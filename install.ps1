# Developer install: dry-runs the plugin with the mod loader's symtest.exe (if found), then copies the mod (mod\) and the
# freshly built plugin into mods_upload\eras_ui_improvements, the folder the in-game Mods menu lists local mods from.
# The dk2ml loader itself (dk2ml.dll + dbghelp.dll, 1.0 or newer) comes from the mod loader repo.
# Also removes the dev install of Equipment Picker (this mod's old name), and warns about HUD Support Gear (its hud.xml
# adds the same tooltip items).
# Uninstall: .\install.ps1 -Uninstall
param(
    [switch]$Uninstall,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\DoorKickers2',
    [string]$SymTest = (Join-Path $PSScriptRoot '..\doorkickers-modloader\build\symtest.exe')
)

$ErrorActionPreference = 'Stop'
$root = Join-Path $env:LOCALAPPDATA 'KillHouseGames\DoorKickers2'
$modDir = Join-Path $root 'mods_upload\eras_ui_improvements'

if (Get-Process DoorKickers2 -ErrorAction SilentlyContinue) { throw 'Door Kickers 2 is running; close it first (the plugin DLL is locked)' }

if ($Uninstall) {
    if (Test-Path $modDir) { Remove-Item $modDir -Recurse -Force }
    Write-Host "Era's UI Improvements removed."
    return
}

$plugin = Join-Path $PSScriptRoot 'build\dk2_eras_ui_improvements.dll'
if (-not (Test-Path $plugin)) { throw 'Build first: .\build.ps1' }

# the dry run: its init against the real PDB, nothing hooked (exit 0 = clean; 100 = a call the loader would refuse)
if (Test-Path $SymTest) {
    $out = & $SymTest $GameDir $plugin
    if ($LASTEXITCODE -ne 0) { $out | Write-Host; throw "symtest dry run failed (exit code $LASTEXITCODE)" }
    Write-Host 'symtest: dry run clean'
} else {
    Write-Warning "symtest.exe not found ($SymTest): skipping the dry run"
}

$old = Join-Path $root 'mods_upload\equipment_picker'
if (Test-Path $old) {
    Remove-Item $old -Recurse -Force
    Write-Host "Removed the old Equipment Picker dev install ($old)."
}
foreach ($dir in 'mods_upload\hud_support_gear', 'mods_upload\hud_support_gear_deluke') {
    $p = Join-Path $root $dir
    if (Test-Path $p) {
        Write-Warning "HUD Support Gear is installed ($p). Its support gear row is part of this mod now (without replacing hud.xml): disable it in the Mods menu."
    }
}

# start clean, so files removed from mod\ (eg. an old gui\*.xml, which the game would still load) don't linger
if (Test-Path $modDir) { Remove-Item $modDir -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $modDir 'native') | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'mod\*') $modDir -Recurse -Force
Copy-Item $plugin (Join-Path $modDir 'native') -Force

Write-Host "Installed to $modDir. Enable `"Era's UI Improvements`" in the in-game Mods menu (needs the dk2ml loader)."
