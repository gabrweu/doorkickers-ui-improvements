# doorkickers-ui-improvements

**Era's UI Improvements**: a *Door Kickers 2* mod that makes the trooper Customization screen faster to use. It doesn't replace any game files, so it works alongside other UI mods.

## Equipment picker
The stock equipment list shows one big row per item, so with weapon and gear mods enabled you see four items at a time and scroll a lot. This mod shows the same items as a compact grid of icons, with names, grouped under headers.

It works for every slot: primary and secondary weapons, their muzzle, scope and ammo, armor, helmet, night vision, utility pouches and support gear. It also covers the extra slots added by [Trooper Toolbox](https://steamcommunity.com/sharedfiles/filedetails/?id=3299377664) (Custom1–9, support gear 2/3).
- **Weapons** show two per row with names, or three or four per row as icons only (a setting; hover an icon for its name and details), grouped as BLUFOR RIFLES, REDFOR RIFLES, SMGS, MACHINE GUNS, MARKSMAN RIFLES, SHOTGUNS, PISTOLS, LAUNCHERS, and so on. Modded categories get their own group. The game files SMGs as rifles, so pistol-calibre rifles (MP7, MP5, Uzi...) are split into SMGS by their caliber, and the remaining rifles into BLUFOR and REDFOR: Eastern-bloc rounds (7.62x39, 5.45x39, 9x39, 7.62x54R, 12.7x55), or a name starting with "AK", make a rifle Redfor. Machine guns and marksman rifles that a mod files as rifles are spotted by their stats (100+ round belts, flat high accuracy) and go under MACHINE GUNS / MARKSMAN RIFLES.
- **Everything else** shows as tiles, three per row, grouped by item type: GRENADES, CHARGES, TOOLS, ARMOR, HELMETS, NIGHT VISION, OPTICS, AMMO, GEAR, DISGUISES.

Within each group, guns are sorted by caliber, smallest first (5.56x45 before 7.62x39 before 7.62x51, 9x19 before .45 ACP, shotguns last), then alphabetically within each caliber (numbers in numeric order, so G3 comes before G36). Other gear keeps the game's order. Click the **−** at the right end of a group's header to collapse it (e.g. hide all rifles) and **+** to bring it back; collapsed groups stay collapsed for that slot until you close the game.

Long names wrap onto two lines, and icons are scaled to fit their tile.

Everything else works as in the stock list:
- Hovering an item shows its details and stats.
- Clicking equips it.
- Locked items are dimmed and show their star or battle-honor cost; clicking one opens the stock unlock dialog.
- The equipped item is framed in orange, and the grid opens scrolled to it.
- Items someone else **in the same squad** already has equipped get a light tint and a small tag; hovering one shows who ("USED BY …"), like the portrait picker. Other squads don't count.

Scroll with the mouse wheel or drag the scrollbar.

Inventory slots the trooper has nothing for are hidden. For example, Trooper Toolbox's clothing slots disappear for a class without any clothes, and support gear disappears for a class with none. A slot with something equipped always stays. Weapon attachment and night vision slots keep the game's own behaviour.

The **Concealment** bar in the equipment stats is hidden for operators whose class can't conceal (everyone but the CIA's undercover and black ops), where it always reads OVERT.

With Trooper Toolbox, its second page (the **More Gear and Cosmetics** button) is merged into the main one. Custom 7–9 go in the free spots around the figure: two over the shoulders and one under the belt. Support Gear 2 and 3 sit on the pouch row, after Utility Pouch 2, and close up when one of them is hidden. The boxes are the same size as the other clothing boxes, their icons are scaled to fit, and the button is gone. Trooper Toolbox's credits, which were on that second page, are behind a small **CREDITS** button in its place: hover it to read them. Trooper Toolbox's background has its box frames painted into it; the mod shows that background with the frames removed and draws a frame around each box itself, so a hidden slot no longer leaves an empty frame behind. If you used the `tt_main_screen_gear` mod for this, disable it: it replaces Trooper Toolbox's screen file.

## Loadouts
A panel left of the Customization screen saves and loads whole loadouts: every slot at once, from the weapons with their scope, ammo and muzzle and the mounted gun, through the utility pouches and support gear 1–3, to the helmet, night vision, armor and Trooper Toolbox's Custom 1–9.

**My loadouts** are the operator's own: five of them. **Team loadouts** belong to the squad: five more, and every squadmate sees and can load them on their own screen. Each row shows its weapons (eg. "HK416D 10.5 Suppressed / Glock 19"), or its name if you named it, and an orange mark shows the loadout the operator has on right now. Hover a row to see everything in it: the weapons with their names and attachments, then the pouches, support gear, helmet, armor and cosmetics as icons.

Click a row to equip that loadout. **SAVE** stores everything the operator has equipped now in that row (it overwrites what was there). Items a loadout can't give this operator (another class's gear from a team loadout, or a mod that was removed) are left as they are, and the line at the bottom of the panel says how many; a weapon and its attachments change together or not at all. The panel hides while an equipment list is open.

Each slot on the Customization screen has a small **padlock** in its top-right corner. Click it to lock that slot for this operator: loading loadouts (and UNDO) then leave it, with its scope/ammo/muzzle or night vision, as it is, and the status line says how many locked slots were kept. You can still change a locked slot by hand. Click the padlock again to unlock.

**UNDO** (bottom right) puts back the operator's gear from before the last change, whether that was a loaded loadout or a single item picked from the list (or Paste loadout). Click it again to go further back, up to 20 steps per operator; the history lasts until you close the game.

The **X** on a row erases that loadout (its items and name); your operator's gear stays as it is.

Click the **pencil** on a row to name that loadout (eg. "Stealth"): type the name, then press Enter or click elsewhere. An empty name removes it. A named loadout shows its name next to its number instead of its weapons, and saving over a named loadout keeps its name. Anyone in the squad can rename a team loadout.

Loadouts are remembered by squad name and operator name, so renaming either starts afresh.

## Portrait picker
Choose any trooper portrait from a grid, instead of cycling through them one by one with the arrows. Click the portraits button at the bottom left of the large portrait, next to the info "i" (which moves down there from the top left). A picker opens with every portrait the game knows, including portraits added by other mods. The filters are:
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

## One-page action wheel
Right-clicking an operator (or one of their waypoints) near a door, window, enemy or hostage normally gives a wheel with two pages: the target's actions (kick, open, lockpick, charges, flashbang into the room, arrest...) and the path's actions (reload, switch weapon, sprint, crouch, grenades, delete path, go-codes), with a button to flip between them. Now both are on one wheel: the path actions stay where they were, and the target's actions sit on a second, outer ring around them, general actions at the top and equipment at the bottom. There's no page button any more. Near the screen edges the wheel is moved so the outer ring stays on screen.

Grenades and launchers are on both of the game's pages, and do different things: on the door's page they're used on the door (a flashbang goes into the room), on the path page they're thrown at a spot you pick. The merged wheel keeps both: the door's version sits on the outer ring straight outside the other one and has a small door-opening badge with an orange arrow.

Both rings are drawn by the mod as a single dotted line, a little darker than the game's, so the buttons stand out.

Without a target nearby the wheel is the stock one.

## Campaign cards
On the campaign selection screen, each campaign that one of your squads has finished gets a **COMPLETED** badge on its card, with the campaign's medal and how many times it was finished ("COMPLETED x2"). Hover the card to see which squads finished it, most recent first, with a count for squads that finished it more than once. It reads the game's own campaign history (the same runs the HISTORY page lists), so it covers mod campaigns too.

Squads are listed by the name they had when the run ended, as in the campaign history.

The Windows arrow is replaced by the mod's own cursor: a cream arrow with a dark outline and an orange tip. Its size follows the screen: 32 px at 1080p, 48 at 1440p, 64 at 4K. It shows in the menus, on the Customization screen and in missions, and adapts within a second when you change resolution or window mode. Outside the game window the normal Windows cursor is used.

## Esc and Back
Esc, Space, Back or a click outside closes the innermost thing that's open: first the portrait picker, then an open equipment list, and only then the Customization screen.

## Settings
Each part can be switched on or off from the mod loader's menu. On the main menu, open **Native mods** and pick **Era's UI Improvements**; the switches are under **SETTINGS**:
- the equipment grid
- weapons per row: 2 (with names), 3 or 4 (icons only)
- sorting guns (by caliber, then name)
- hiding empty slots
- hiding the concealment bar for operators who can't conceal
- Trooper Toolbox's slots on one page
- the loadouts panel
- Esc closing lists first
- the portrait picker
- the tooltip gear row
- the one-page action wheel
- the campaign cards
- the cursor

Changes apply immediately (there's also a "Turn everything on" button) and are saved in `%LOCALAPPDATA%\KillHouseGames\DoorKickers2\dk2ml\dk2_eras_ui_improvements\settings.ini`, which survives mod updates.

## Requirements
- [dk2ml](../doorkickers-modloader), the Door Kickers 2 Native Mod Loader (`dk2ml.dll` and `dbghelp.dll` in the game folder). Without it the mod does nothing.
- Game version 1.12 (`gameVersion="112"`).

## Build and install (developers)
Requires VS 2022 Build Tools (C++ workload with CMake and Ninja). The loader's headers are in `dk2ml\`; the dry run uses the mod loader repo checked out next to this one (its `build\symtest.exe`).

```powershell
.\build.ps1 -SymTest   # builds build\dk2_eras_ui_improvements.dll and dry-runs it against the game's PDB
.\install.ps1          # copies the mod to %LOCALAPPDATA%\KillHouseGames\DoorKickers2\mods_upload\eras_ui_improvements
```

Then enable **Era's UI Improvements** in the in-game Mods menu. `tools\make_art.ps1` regenerates the thumbnail and the portrait button icons.

## Credits
The Trooper Toolbox background (`mod/textures/gui/dk2ml_tt_bg.tga`) is [Trooper Toolbox](https://steamcommunity.com/sharedfiles/filedetails/?id=3299377664)'s art by Sly, used with permission, with its box frames removed.

## License
MIT, except the Trooper Toolbox background above, which stays Sly's.
