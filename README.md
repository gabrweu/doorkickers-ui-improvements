# doorkickers-ui-improvements

**Era's UI Improvements**: a *Door Kickers 2* mod that makes the trooper Customization screen faster to use. It doesn't replace any game files, so it works alongside other UI mods.

## Equipment picker
The stock equipment list shows one big row per item, so with weapon and gear mods enabled you see four items at a time and scroll a lot. This mod shows the same items as a compact grid of icons, with names, grouped under headers.

It works for every slot: primary and secondary weapons, their muzzle, scope and ammo, armor, helmet, night vision, utility pouches and support gear. It also covers the extra slots added by [Trooper Toolbox](https://steamcommunity.com/sharedfiles/filedetails/?id=3299377664) (Custom1–9, support gear 2/3).
- **Weapons** show two per row, grouped as RIFLES, MACHINE GUNS, MARKSMAN RIFLES, SHOTGUNS, PISTOLS, LAUNCHERS, and so on. Modded categories get their own group.
- **Everything else** shows as tiles, three per row, grouped by item type: GRENADES, CHARGES, TOOLS, ARMOR, HELMETS, NIGHT VISION, OPTICS, AMMO, GEAR, DISGUISES.

Long names wrap onto two lines, and icons are scaled to fit their tile.

Everything else works as in the stock list:
- Hovering an item shows its details and stats.
- Clicking equips it.
- Locked items are dimmed and show their star or battle-honor cost; clicking one opens the stock unlock dialog.
- The equipped item is framed in orange, and the grid opens scrolled to it.
- Items someone else **in the same squad** already has equipped get a light tint and a small tag; hovering one shows who ("USED BY …"), like the portrait picker. Other squads don't count.

Scroll with the mouse wheel or drag the scrollbar.

Inventory slots the trooper has nothing for are hidden. For example, Trooper Toolbox's clothing slots disappear for a class without any clothes, and support gear disappears for a class with none. A slot with something equipped always stays. Weapon attachment and night vision slots keep the game's own behaviour.

With Trooper Toolbox, its second page (the **More Gear and Cosmetics** button) is merged into the main one. Custom 7–9 go in the free spots around the figure: two over the shoulders and one under the belt. Support Gear 2 and 3 sit on the pouch row, after Utility Pouch 2, and close up when one of them is hidden. The boxes are the same size as the other clothing boxes, their icons are scaled to fit, and the button is gone. If you used the `tt_main_screen_gear` mod for this, disable it: it replaces Trooper Toolbox's screen file.

## Portrait picker
Choose any trooper portrait from a grid, instead of cycling through them one by one with the arrows. Click the grid button in the corner of the large portrait. A picker opens with every portrait the game knows, including portraits added by other mods. The filters are:
- **This unit**: the portraits the stock arrows cycle through.
- **All units**: every unit's portraits, grouped by unit. The unit's name stays pinned at the top while you scroll through its portraits.
- **Hide used**: hides portraits other troopers in your roster already wear.

**Random** (the orange button) gives the trooper a random portrait from the list that nobody wears; click again to re-roll. Between the buttons, a status line shows how many portraits are listed and how many are in use (or hidden). Unit headers show how many portraits each unit has.

Portraits worn by squadmates are dimmed, and those worn in your other squads are dimmed lightly. Hovering a portrait shows it large, with its file name, a CURRENT badge if it's the trooper's own, and the list of troopers who wear it.

The stock PREV/NEXT arrows on the portrait are fixed too. They step through the trooper's own pool in the same order as the picker, and no longer get stuck on 2–3 portraits when a trooper wears a portrait from outside its class (seen with mod classes that tag their own portraits).

Click a portrait to give it to the trooper. It's saved to your roster immediately.

## Operator tooltip
Hovering a trooper in a mission (or on the deploy screen) also shows their **Support Gear 2 and 3**: the lockpick, and the crowbar, halligan, feet of steel and so on. They appear in a row under the helmet and armor, with icons and names, and only when the trooper carries such gear. The injury and status strips move down to make room.

It replaces the old HUD Support Gear mod and doesn't replace `hud.xml`, so it works with the default HUD and with HUD mods like Deluke UI and Tacticool UI. Disable HUD Support Gear if you had it.

Known limit: the row hangs under the helmet/armor panel, so a trooper with neither helmet nor armor doesn't get it.

## Cursor
The Windows arrow is replaced by the mod's own cursor: a cream arrow with a dark outline and an orange tip. Its size follows the screen: 32 px at 1080p, 48 at 1440p, 64 at 4K. It shows in the menus, on the Customization screen and in missions, and adapts within a second when you change resolution or window mode. Outside the game window the normal Windows cursor is used.

## Esc and Back
Esc, Space, Back or a click outside closes the innermost thing that's open: first the portrait picker, then an open equipment list, and only then the Customization screen.

## Settings
Each part can be switched on or off from the mod loader's menu. On the main menu, open **Native mods** and pick **Era's UI Improvements**; the switches are under **SETTINGS**:
- the equipment grid
- hiding empty slots
- Trooper Toolbox's slots on one page
- Esc closing lists first
- the portrait picker
- the tooltip gear row
- the cursor

Changes apply immediately (there's also a "Turn everything on" button) and are saved in `%LOCALAPPDATA%\KillHouseGames\DoorKickers2\dk2ml\dk2_eras_ui_improvements\settings.ini`, which survives mod updates.

## Requirements
- [dk2ml](../doorkickers-modloader), the DK2 native mod loader (`dbghelp.dll` in the game folder). Without it the mod does nothing.
- Game version 1.12 (`gameVersion="112"`).
- Don't enable the old standalone **Portrait Selector** alongside this mod: it's included here, and both would hook the same game functions.

## Build and install (developers)
Requires VS 2022 Build Tools (C++ workload with CMake and Ninja) and the mod loader repo checked out next to this one. Otherwise, set `-DDK2ML_SDK_DIR=<folder with dk2ml.h>`.

```powershell
.\build.ps1 -SymTest   # builds build\dk2_eras_ui_improvements.dll and dry-runs it against the game's PDB
.\install.ps1          # copies the mod to %LOCALAPPDATA%\KillHouseGames\DoorKickers2\mods_upload\eras_ui_improvements
```

Then enable **Era's UI Improvements** in the in-game Mods menu. `tools\make_art.ps1` regenerates the thumbnail and the portrait button icons.

## License
MIT
