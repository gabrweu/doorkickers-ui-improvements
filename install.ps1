# Developer install: copies the mod (mod\) and the freshly built plugin into mods_upload\eras_ui_improvements, the
# folder the in-game Mods menu lists local mods from. The dk2ml loader itself (dbghelp.dll) comes from the mod loader repo.
# Also removes the dev install of Equipment Picker (this mod's old name), and warns about the separate Portrait
# Selector (it hooks the same game functions, so with both enabled one of the plugins fails to load) and HUD Support
# Gear (its hud.xml adds the same tooltip items).
# Uninstall: .\install.ps1 -Uninstall
param([switch]$Uninstall)

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

$old = Join-Path $root 'mods_upload\equipment_picker'
if (Test-Path $old) {
    Remove-Item $old -Recurse -Force
    Write-Host "Removed the old Equipment Picker dev install ($old)."
}
foreach ($dir in 'mods_upload\portrait_selector', 'mods\portrait_selector') {
    $p = Join-Path $root $dir
    if (Test-Path $p) {
        Write-Warning "Portrait Selector is installed ($p). It's part of this mod now and hooks the same functions: disable it in the Mods menu or uninstall it."
    }
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
