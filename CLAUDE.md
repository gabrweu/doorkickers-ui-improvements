# CLAUDE.md
  | `EqpFirearm_Template::suppressedSwitch` | 376 (optional) |

**Era's UI Improvements**: a *Door Kickers 2* mod for the trooper Customization screen and the HUD. Its features:
- the equipment list (one big row per item) becomes a dense, grouped icon grid, for every slot;
- slots the trooper has nothing for are hidden;
- Esc/Back closes an open list or picker before the screen;
- a portrait picker, with the stock PREV/NEXT portrait arrows fixed;
- the trooper hover tooltip shows Support Gear 2/3 (lockpick, crowbar, halligan) with names;
- our own mouse cursor, sized to the screen;
- an on/off switch for each of the above, on the mod's page of the mod loader's Native mods screen.

It's a native plugin (`dk2_eras_ui_improvements.dll`) for the dk2ml loader (`..\doorkickers-modloader`, see its CLAUDE.md) plus GUI XML files. The portrait picker was the separate Portrait Selector (`..\doorkickers-portrait-selector`, now superseded). Its notes are in "Portrait picker" below.

## Rules
- **Never run git.** The user commits and pushes manually (Fork). When work is done, say what changed and suggest a commit message.
- **Never upload or publish anything** (GitHub releases, Steam Workshop, the in-game Mods-menu upload).
- **Never add game-derived material to the repo:** PDB dumps, disassembly, or game assets. GUI XML references stock textures *by path* only. Our own art comes from `tools\make_art.ps1`. Disassembly goes to a scratch folder, never into the repo.
- **In-game testing is done by the user.** Never launch or kill the game.

## Layout
```
src/game.h/.cpp      every symbol / field / enum the plugin uses, resolved by name from DoorKickers2.pdb; logs key offsets
src/gui.h/.cpp       GUI helpers: Find, Children, Text, the game's AddChild/SetOrigin actions, Callback wiring
src/entries.cpp      reads the stock list's items back, groups them by item type, picks the wide or square layout
src/grid.cpp         attach next to the stock list, the virtualized grid, scrolling
src/slots.cpp        hides inventory slots the trooper has no options for
src/back.cpp         Esc / Back with an equipment list open closes the list, not the screen
src/ttmerge.cpp      Trooper Toolbox: its second page's slots (Custom7-9, SupportGear2/3) moved onto the main page
src/hudtip.cpp       the trooper hover tooltip's support gear row (gui/operator_tooltip.xml)
src/cursor.cpp       the mouse cursor (mod/cursor/eras_cursor.cur), set on the game window from a background thread
src/settings.cpp     the per-feature switches (settings.ini), registered as options on the loader's Native mods screen
src/portraits.cpp    reads/validates ObjectLibrary::m_humanPortraits, filters it, assigns a portrait to a trooper
src/picker.cpp       the portrait picker: attach to the Customization screen, the grid, apply the clicked portrait
src/plugin.cpp       DK2ML_PluginInit + the safe hooks
mod/                 the mod folder players get: mod.xml, mod_image.jpg, gui/equipment_picker.xml,
                     gui/portrait_selector.xml, textures/gui/dk2ml_ps_open_*.dds
tools/make_art.ps1   draws mod_image.jpg, the portrait button icons and the cursor
```

## Build, install, test
- `.\build.ps1` (VS 2022 Build Tools, CMake, Ninja). `DK2ML_SDK_DIR` defaults to `..\doorkickers-modloader\include`.
- **Always dry-run before asking the user to test:** `.\build.ps1 -SymTest` (needs `..\doorkickers-modloader\build\symtest.exe`). It must print `plugin init returned 0`. Check the logged offsets against the values below.
- `.\install.ps1` copies the mod to `%LOCALAPPDATA%\KillHouseGames\DoorKickers2\mods_upload\eras_ui_improvements`, clearing that folder first so files removed from `mod\` don't linger (the game loads every `gui\*.xml`). It refuses while the game runs. `-Uninstall` removes it.
  - It also deletes the old `mods_upload\equipment_picker` dev install.
  - It warns if a Portrait Selector install exists: that plugin hooks the same functions, so one of the two would fail to load.
- Logs: `<game>\dk2ml.log` (plugin) and `%LOCALAPPDATA%\KillHouseGames\DoorKickers2\log.txt` (GUI XML errors).

## How it works (game build gameVersion 112)
- **Hooks** (`plugin.cpp`, all safe hooks). Never hook a function the freecam plugin hooks (`GUI::sAction::Execute`, `ImGui::Render`, `GameGUI::IsAnyMenuOpened`, the camera and the renderer): MinHook can't hook a target twice, so the second plugin would fail to load.

  | Hook | Does |
  |---|---|
  | `GameGUI::Customize_OnTrooperSelected` (pre saves `rdx`; post) | Picker refresh, then `Slots_Refresh` |
  | `GameGUI::Customize_OnEvent` (`edx` = event) | Pre on BACK: close the picker, or the equipment list, and skip with `rax = 1`. On CLOSED: close the picker. Post on OPENED: attach the picker. |
  | `GameGUI::Customize_OnChangePortrait` (pre) | The fixed PREV/NEXT arrows |
  | `GUI::ItemList::Init` (post) | The equipment grid. `Init` runs for every `ItemList` in the game; we act only when `this` is `#equipment_list_parent` inside `GameGUI::m_pCustomizationScreen > #equipment_list_panel`. |
  | `GUI::Item::Show` (pre, may skip), `GUI::Item::Hide` (pre) | Keeping empty slots hidden |
  | `UpdateHumanTooltip` (pre saves `rcx` and `stack[9]`; post) | The hover tooltip's support gear row (`hudtip.cpp`) |
- **The stock list builder.** On `GUI_GAME_CUSTOMIZE_INVENTORY_SELECT` (151), `Customize_OnEvent` calls an unnamed routine (`0x140117BA0` in build 112) with the customized trooper, the clicked slot Checkbox's name, and `#equipment_list_panel`. It does five things:
  1. Deletes the old items.
  2. Collects the templates bound to the trooper's class for that slot and sorts them by `sortId`.
  3. Clones `#equipment_item_template` per item.
  4. Fills each item (`0x140115A20`): `#id` = template name, `#name`, `#image`, `#unlock` / `#unlock_stars` / `#unlock_bh_bgnd`, `#equipment_item_equipped`, `#muzzle_indicator`.
  5. Calls `ItemList::Init`, then `ItemList::ScrollTo(equipped)`.

  **EMPTY items** (the slot's "Empty" choice, from `empty.xml` binds) are filled with a null template (`0x140117EF0`): `#name` becomes `@menu_cust_item_empty`, `#id` becomes **empty**, and there's no image. Equipping that cell with its empty `#id` is how stock empties a slot. So `FromStockList` keeps such cells as EMPTY entries (the NONE group), and our cell gets the same empty `#id`. Don't skip cells with an empty `#id`.

  So in our post hook the list holds exactly the right items in the right order. `entries::FromStockList` reads them back and doesn't rebuild anything.
- **Hover and equip stay stock.** In `Customize_OnEvent`:
  - `ITEM_TRY_EQUIP` (150) calls `Customize_OnTryEquipItem(params->owner)`.
  - `HOVER_LIST_ITEM` (152) calls `Customize_OnHoverListItem(params->owner)`.

  The owner is the item whose XML `TriggerEvent` action fired; for a clone, that's the clone. Both functions go through helper `0x140117FB0`, which finds a child named **`#id`** in the item (falling back to its parent's children) and reads `StaticText::m_text` (+960) as the template name for `ObjectLibrary::GetEquipment`. **It doesn't null-check**, so every cell must have a `#id` StaticText. Our cells carry the stock item's actions and a hidden `#id`.
  - TryEquip handles the unlock cost and the `Customization_UnlockItem` pop-up. The pop-up stores the item and passes it to `Customize_OnUnlockItem` on YES.
  - Equipping hides `#equipment_list_panel` in code, which hides our grid with it.
  - `HOVER_LIST_ITEM_END` (153) doesn't use the owner.
  - We show `#equipment_details_panel` ourselves from a Callback on hover; the stock item does it from XML. That keeps our XML from naming items in `customization.xml`, which other UI mods replace.
- **Every slot.** The grid takes over every list the game builds: weapons, attachments (muzzle/scope/ammo), armor, helmets, NVG, utility pouches, support gear 1–3, and Trooper Toolbox's Custom1–9.
  - Nothing is keyed on the slot. Ammo templates have no `inventoryBinding`, and the slot Checkboxes' checked state around `INVENTORY_SELECT` / `GUI_CUSTOMIZE_UNCHECK_ALL` is murky.
  - Only empty or unreadable lists keep the stock list (`Deactivate` re-shows what we hid).
- **Grouping** (`entries.cpp`, `GroupOf`), by `EquipmentType`:
  - **Firearms** go by `category`. Stock categories come in unit flavours (`usrifle`, `opforlmg`, `militiapistol`), so they're merged by substring; unknown modded categories get their own group under the raw name in upper case. Only firearms have a category.
  - **Other types** use the `g_typeLabels` table: GRENADES, CHARGES, ARMOR, HELMETS, NIGHT VISION, AMMO, DISGUISES, TOOLS (lockpick, crowbar, hammer, bolt cutter, breaching kit, TEC torch). Its enum names are resolved from the PDB on first use; a missing one is logged and falls into OTHER.
  - **`SCOPE`** is OPTICS in a slot whose binding contains "Scope", else GEAR: Trooper Toolbox's Custom slots hold cosmetics as `<Scope>` items.
  - **`EMPTY`** is NONE, a group without a header row (`Headerless` in `grid.cpp`, also never sticky): the Empty cell sits alone at the top.
  - **Order:** NONE (the slot's Empty choice) always comes first; the other groups appear in stock (sortId) order, by first appearance, and each group's items stay in stock order.
- **Squadmates' gear** (`entries::MarkSquadUse`, setting `squad_usage`). When a list is built, every inventory slot of the other troopers in the customized trooper's **own squad** is read (`Roster::Trooper::inventory` names) through `portraits::Squadmates`, which shares the portrait picker's validated roster walk (`ReadRoster`). An entry whose template name a squadmate has in any slot gets `usedBy` (their names). Its cell shows `#dk2ml_ep_cell_used` (a light 16% tint + tag), and hovering it shows `#dk2ml_ep_usedby` beside the cell.
  - **The pop-up** (`ShowUsedBy`) is built top-down: the item's icon (`#dk2ml_ep_usedby_img`, not aligned, fitted into 200×64 at most scale 0.6; hidden without an icon), the title "USED BY N OPERATOR(S)", then the names (`usedBy` is a list of names). Its height is fitted to the names: `ChangeText` with `sizeX` and no `sizeY` leaves the wrapped height in `m_size.y`; the pop-up's `m_size.y` is set and the two named backgrounds are stretched in y (render scale). Then the children are re-placed with SetOrigin. Names that would make it taller than the grid become "+N MORE"; the title count stays complete. The names placeholder is ~700 characters (`ChangeText` caps at the XML length).
  - The pop-up goes on the side with more room and never over the hovered cell, which would end the hover; it's clamped vertically to the grid.
  - It hides on hover end (an `OnHoverEnd` Callback in both templates), scrolling, and deactivation.
  - Empty entries never count.
  - **Suppressed weapons.** A suppressor isn't an attachment: the muzzle list holds suppressed firearm twins (eg. `FRWD_CSAW_Sup`, `inventoryBinding="PrimaryWeaponMuzzle"`), and equipping one replaces the trooper's `PrimaryWeapon` name. `EqpFirearm_Template::suppressedSwitch` (+376, char*) links the twins. A squadmate with a muzzle-bound twin also counts for the plain weapon (`SuppressedTwinOf`), not the reverse. Ammo has separate `_Sup` variants with no link, so ammo counts stay split.
- **Two layouts** (`g_layouts` in `grid.cpp`), each with its own template and lazily cloned cell pool; the header pool is shared:
  - **Wide cells** (`#dk2ml_ep_cell_template`, 268×146, 2 columns, 6 rows, icon box 256×78, max icon scale 0.75). Used when more than half the list is firearms (`List::wide`): weapon and muzzle lists, whose icons are about 360×112.
  - **Square tiles** (`#dk2ml_ep_tile_template`, 176×148, 3 columns, 6 rows, icon box 164×80, max scale 0.8) for everything else. Those icons are 50–270 px and roughly square: armor 272×188, grenades 120×144, cosmetics 128×128.
  - Both templates have the same child names, so `FillCell` works with either. The tile template is optional: without it everything uses the wide layout.
  - Switching layout hides the other layout's cells.
- **Trooper Toolbox** (workshop 3299377664, which replaces `customization.xml`):
  - It keeps `#equipment_list_panel` / `#equipment_list_parent`. It nests `#id` inside `#sortid`, and both `gui::Find` and the game's helper are recursive.
  - Its extra slot Checkboxes (`Custom1..9`, `SupportGear2/3`) open the same list, so the same hook covers them.
  - Its list origin is `55 124` (see Attach).
- **Trooper Toolbox merge** (`ttmerge.cpp`, setting `tt_main_page`). TT's `#cosmetic_panel` (Custom7–9, SupportGear2/3, `#close_cosmetic_panel`) is a child of `#inventory_panel`, both `align="r"` with no origin, so it shares the main page's coordinate space. Nothing is copied or reparented:
  - **Geometry**, once per GUI on the first trooper select / OPENED with TT found:
    - the five Checkboxes get main-page origins (`kPlaces`, fitted on a 2000×1125 screenshot: `x = -544 + (left − 1022) × 1.28`, `y = 528 − (top − 88) × 1.285`);
    - they're resized to Custom3: its `m_size` (Checkbox and both `m_pButtons`), each of the 3 render objects per Button scaled to Custom3's drawn size (size × scale), and `#image` scale/origin and `#name` origin copied;
    - TT's `small_button_normal` texture has its own cream border, so no frame art is needed (the main page's frames are baked into TT's background);
    - the originals are saved and restored when the setting goes off.
  - **Layout:** Custom7 / Custom9 over the shoulders (mirrored), Custom8 under Custom6, SupportGear2/3 after UtilityPouch2 on the pouch row (the equipment row below would put their labels off the window). Places have a group: SupportGear2/3 share theirs, and `TtMerge_Arrange` (after `Slots_Refresh` and `Slots_SetEnabled`) gives the visible ones the places in order, so a slot hidden by slots.cpp leaves no gap.
  - **Icons** are fitted with `gui::FitImage` into the reference box's drawn size × 0.85 / 0.7, at most the reference icon's scale (0.6), after each trooper select and whenever `#inventory_panel` is shown again (`TtMerge_OnShow`: TT's `SHOW_DEFAULT` after a list closes).
  - **Visibility**, in the `Item::Show/Hide` hooks (`TtMerge_SkipShow/SkipHide`, checked after `Slots_IsInternal` so the empty-slot hiding still works): skip Show of `#cosmetic_panel_toggle` / `#close_cosmetic_panel` and Hide of `#cosmetic_panel` and the five slots. A slot click hides `#inventory_panel`, which hides them with it. Re-asserted on every trooper select (before `Slots_Refresh`).
  - The user's old XML mod `tt_main_screen_gear` (a side-column version, replacing `customization.xml`) must be disabled.
- **Empty slots** (`slots.cpp`). Every inventory slot Checkbox (`Inventory::GetNameForEnum(i)` looked up in the screen) the trooper has no options for is hidden. That includes Trooper Toolbox's Custom1–9 and SupportGear2/3, and stock slots too.
  - **Left to the game:** weapon scope/ammo/muzzle and HelmetNVG. The `#inventory_panel` refresh (`0x140117330`) `SetVisible`s those itself and only fills the others.
  - **When.** In the `Customize_OnTrooperSelected` post hook. That's after the game's `#inventory_panel` refresh, and only while the screen is shown (the function does nothing otherwise).
  - **Options = the game's own list rule** (`0x140113B90`, generic path):
    - the template's `inventoryBinding` equals the slot name (or is contained in it, for UTILITY_POUCH1..SUPPORT_GEAR3);
    - it's bound to the trooper (`0x140112B40`): `ObjectLibrary::IsEquipmentBoundTo` with the template's `name`, or a HashedString holding only the FNV-1a hash of `EquipmentType::GetNameForEnum(type)`, to the trooper's `className` or `unit`;
    - it has a `tooltip`.

    EMPTY items don't count. A slot whose equipped item (`Roster::Trooper::inventory[i]`) isn't EMPTY always stays.
  - **Templates** are `ObjectLibrary::m_equipmentTemplates`, a `std::unordered_map<unsigned, Equipment_Template*>`: list head at map+8, `node->next` at +0, the value at node+0x18, as the game walks it.
  - **Keeping them hidden.** Trooper Toolbox shows the slots again from XML: `GUI_CUSTOMIZE_SHOW_DEFAULT` runs on screen open, Back and cosmetics close; its cosmetics button shows Custom7–9.
    - `Checkbox` derives directly from `GUI::Item` (no Show/Hide override; `Button::Hide` calls `Item::Hide`), so every Show/Hide of a slot goes through `Item::Show` / `Item::Hide`.
    - The Show hook skips the original for a suppressed slot and records `wantShown`; the Hide hook clears it. When the slot gets options again (another trooper), it's shown only if something wanted it shown, so cosmetics-panel slots stay hidden on the main page.
    - State is dropped if `GuiRoot()` changes (GUI reload).
- **Esc / Back** (`back.cpp`). Esc and Space (the screen's `OnKeyDown`), the background click catcher and `#customize_back_profile` all trigger `GUI_GAME_CUSTOMIZE_BACK` (161), stock and Trooper Toolbox alike. `Customize_OnEvent` closes the whole screen on it.
  - **The fix.** The `Customize_OnEvent` pre hook closes the innermost thing that's open and skips the original:
    1. **The portrait picker**, if it's open.
    2. **Otherwise the equipment list**, if `#equipment_list_panel` is shown. The list's own `#customize_back_eqplist` gets its click actions run with `Item::ExecuteOnEvent(EVENT_CLICK)`. Its XML closes the list the right way for whichever `customization.xml` is loaded: stock unchecks and hides the panel; Trooper Toolbox runs `GUI_CUSTOMIZE_SHOW_DEFAULT`.
  - The unlock pop-up's Esc only hides the pop-up (no BACK), so it's unaffected.
- **Operator tooltip** (`hudtip.cpp`, `gui/operator_tooltip.xml`). A port of the user's earlier GUI-only "HUD Support Gear" mod, which replaced `hud.xml` (once for stock, once rebased on Deluke UI).
  - **The function.** `UpdateHumanTooltip(GUI::Item* root, bool, bool, bool, eHumanType, const sHumanId&, const StaticString<48>&, const StaticString<48>&, const Equipment_Client* equipment, Inventory::eType, …)` fills `HUD_Human_Tooltip`. It's a free function, called by `GameGUI::UpdateEntityTooltip` (mission hover) and `GameGUI::Deploy_UpdateTooltip`.
  - **Where the equipment is.** `equipment` is the 9th argument. The prologue sets `rbp = rsp(entry) − 136`, and the filler reads `0xd0(%rbp)` = entry `rsp + 72` = `stack[9]`. It's `Equipment_Client[Inventory::NUM_VALUES]` (88 bytes each, `pItem` = `const Equipment_Template*` at +0).
  - **Routing** (filler `0x1400E8DE0`):
    - UtilityPouch1/2 and SupportGear1 go to `#Inv_Utility0..3`;
    - primary, secondary and mounted gun go to `#InvWeapon0/1`;
    - scope/ammo are skipped;
    - every other slot is filled only if `FindChild(root, Inventory::GetNameForEnum(slot))` finds an item. The game hides it, sets `#Icon`, and shows it when filled. Stock `hud.xml` has no `SupportGear2/3` items.
  - **Layout rule.** Slot-named direct children of `#InvArmorHelmet` are lined up 160 px apart (half its width), so our cells sit in a plain wrapper (`#dk2ml_tt_gear`) that keeps its XML origin.
  - **Attach.** The pre hook moves the wrapper into the root's `#InvArmorHelmet` (AddChild) before the game fills the tooltip.
  - **Stacking.** At the end of `UpdateHumanTooltip`, visible sections are chained by writing each one's `m_parent` to the previous visible one: `#InvWeapon0/1`, `#InvArmorHelmet`, `#InvInjuryStatus`, buffs. Each sits at its fixed XML origin (`0 -120`, align `lb`) below its "parent". The sections are XML children of `#NameHeader`; their child lists stay as loaded, so `ContainerOf` finds them.
  - **The post hook:**
    1. Sets each shown cell's `#dk2ml_tt_name` from the template's tooltip (`Loca::GetTextForId`), fitted to two lines with `gui::SetTextFitted`, and only when it changes.
    2. Fits `#Icon` into the 88 px circle (max scale 0.45).
    3. Shows the wrapper, which holds the row's one full-width 320×150 background (so a single filled slot leaves no gap), only while a cell is filled.
    4. Moves the section whose `m_parent` is the panel down by the row's height (150) while the row shows, and back otherwise. Everything after it follows, because each section is placed relative to the previous one. That puts injury/fatigue and buffs under the row.
  - **Moving aligned items.** The SetOrigin action (`sAction::Execute`, `0x1400CC9DD`) writes its `"x y"` into **both** `m_localOrigin` (+120) and `m_alignOffset` (+156). For aligned items (the sections are `align="lb"`), the game itself moves them by adding the same delta to both fields (the `#InvArmorHelmet` row layout, `0x1400EA0DD`). `ShiftBy` does that and remembers the delta it applied, so nothing drifts or gets applied twice. Don't use SetOrigin on aligned items with a value read from `m_localOrigin`.
  - **Text colour.** Copied from the tooltip's own `#InvWeapon0 > #Name`: cream in stock, white in Deluke. A StaticText's colour is `m_textRenderObject.color` (+976+28 = 0x3EC), the same field the game reads from its `GoodGuy`/`BadGuy` colour holders.
  - **Other HUD mods.** Deluke UI and Tacticool UI have the same tooltip structure (only text colours differ), so one runtime-injected row covers them all.
  - **Known limit:** a trooper with neither helmet nor armor has `#InvArmorHelmet` hidden, so the row doesn't show.
- **Settings** (`settings.cpp`).
  - **Storage.** One `std::atomic<bool>` per feature (`g_settings`), saved as `key=0|1` in `GetConfigDir()` + `settings.ini` (`%LOCALAPPDATA%\…\dk2ml\dk2_eras_ui_improvements\`), all on by default. It's loaded right after `Resolve()`.
  - **Where they're edited: the mod loader's Native mods screen** (main menu → Native mods → Era's UI Improvements → SETTINGS), the same way Free Camera does it.
    - **Registration.** `Settings_Register` adds them with `DK2ML_API::AddOption` (API v5) during `DK2ML_PluginInit`; the loader builds the screen when the GUI loads. They are: HEADER "Customization screen" (6 BOOLs, incl. "Mark gear used by squadmates" and "Trooper Toolbox: all slots on one page"), "Missions" (1), "General" (1), plus a BUTTON "Turn everything on".
    - **The values are ours.** Each BOOL points at a plain `bool` in `g_switches`. The loader reads it when the screen opens; on a click it writes it and calls `onChange`. `OnChange` copies it into the atomic flag, applies the switch, and saves. `onChange` runs crash-contained: if it crashed, the loader would switch the plugin's hooks and options off. The loader copies label and tooltip strings; a leading `@` would be stripped.
    - **Loader versions.** The plugin needs API v4 (`GetConfigDir`). With only v4 there's no Native mods options section; that's logged, and `settings.ini` still works.
  - **Live switching.**
    - grid: `Grid_SetEnabled` / `Grid_OnListInit` deactivate;
    - slots: `Slots_SetEnabled(false)` restores every hidden slot as its own visibility would be;
    - Esc: the `Customize_OnEvent` hook checks `backKey`;
    - portraits: `Picker_SetEnabled` / `Picker_Attach` show or hide the button, and `Picker_StepPortrait` falls back to stock;
    - tooltip: `HudTip_Before` doesn't attach, `HudTip_After` hides the row and undoes the shift;
    - cursor: the thread puts the class cursor back to `IDC_ARROW`.
    - TT merge: `TtMerge_SetEnabled(false)` restores the saved geometry, hides the five slots and shows the toggle again; on, it merges at once if the screen is up.
- **Cursor** (`cursor.cpp`, no hooks).
  - **How the game shows it.** It registers its window class with `LoadCursorA(NULL, IDC_ARROW)` (`0x140237EB1`) and doesn't import `SetCursor` or draw a cursor itself, so Windows shows the class cursor through `DefWindowProc`'s `WM_SETCURSOR`, everywhere in the game.
  - **The swap.** `SetClassLongPtrW(hwnd, GCLP_HCURSOR, …)` replaces it. A `WM_SETCURSOR` is then posted so the new cursor shows without waiting for a mouse move.
  - **Why a thread.** The window doesn't exist while plugins initialize, and resolution changes need a different size. A background thread checks every 500 ms for this process's largest visible unowned top-level window, and reloads the cursor when the window or the size changes.
  - **Sizes** come from the client height: below 1300 → 32, below 1900 → 48, below 2600 → 64, else 96. They're images in one multi-size `.cur` (32-bit DIBs, hotspot at the tip) written by `tools/make_art.ps1`. The previous cursor we loaded is destroyed; the system arrow never is.
  - **Where it lives.** It's loaded from `DK2ML_PluginInfo::modDir` + `cursor\eras_cursor.cur`. Without a mod folder (mods_native) or the file, the Windows cursor stays.
- **Attach.** `#dk2ml_ep_root` is a top-level item in our XML. It's moved into the stock list's parent with the game's AddChild action, which also puts it last, so it's on top for drawing and hit-testing.
  - Its XML placement copies the stock list's (`align="l" origin="4 124"`, 548x944).
  - If a UI mod moved the list (Trooper Toolbox uses `origin="55 124"`) and the align flags match, the root gets the list's `m_localOrigin`.
  - When active, the stock list and its two scroll-arrow buttons are hidden. The buttons are found as the unnamed `stealFocus` Buttons among the list's siblings. Whatever was hidden is shown again when a slot falls back to the stock list.
- **Children.** `GUI::Item::m_children` (+72) is the head node of an intrusive circular `LinkedList<GUI::Item>` (`head` +0, `next` +8, `owner` +24). Every Item has a node as a base at +8. Walk from `head->next` until you're back at `head`, taking `node->owner`. That's the same walk `ItemList::Init` does.
- **The grid is virtualized** the way the portrait picker's is: rows are group headers or up to the layout's `cols` cells, and only one screen of slots is cloned per layout (`EnsureHeaders` / `EnsureCells`, once per GUI load).
  - Headers (34 px) are shorter than cell rows (146 / 148 px), so `ShowRows` places every visible row from the top and re-sets an origin only when it changed.
  - When the top row is mid-group, that group's header sticks to the top.
  - `MaxTop` is the smallest top row from which the rest fits on one screen.
  - **Scroll stops** (`src/scroll.h`, shared with the portrait picker). A top on a group's header looks exactly like a top on its first card row (the header sticks), so headers aren't stops: `scroll::Stop` moves a header top to the next row, and `scroll::Step` counts card rows only. Every wheel notch therefore moves the same number of card rows: 1 in the grid, 2 in the portrait picker. `MaxTop`, `ShowRows` and `ScrollTo` all normalize through `Stop`.
  - **"N MORE BELOW"** (`#dk2ml_ep_more`, `ShowMore`). Only whole rows are shown. `ShowRows` first picks the rows with `VisibleEnd(top, height)` (sticky header, row heights, the header/cell pools). If any rows are left over, it picks again with the bar's height (+ gap) reserved, and the bar shows pinned to the grid's bottom with the number of hidden items; a click scrolls one wheel notch. A trailing header whose cards didn't fit is dropped too, so a group never shows its header alone. At `MaxTop` everything fits, so there's no bar.
  - Columns are `floor((grid width + gap) / (cell width + gap))`, so the XML cell size decides the density.
- **Cell names.** The name label is top-anchored under the icon with `wordWrap` and a fixed `sizeX`, so it wraps downwards. Both cells leave room for exactly two lines of `paragraph_3` (NotoSans-Medium 20, about 28 px per line).
  - Don't use `fontAutoDownsize`: it shrinks long modded names (25–30 characters) until they can't be read.
  - **Measuring text.** `StaticText::ChangeText` lays the text out right away (`StaticText::UpdateRenderObject`, `0x1400DFBD0` in build 112), with word wrap at the hardcoded width.
    - For each axis without a hardcoded size (`m_hardcodedSize` +0x3B0/+0x3B4, from the XML `sizeX`/`sizeY`), it writes the laid-out text's size into `Item::m_size`.
    - Our labels have `sizeX` and no `sizeY`, so after `ChangeText` the label's `m_size.y` is the wrapped text's real height.
  - **`SetName`** uses that measurement:
    1. It measures one and two lines (`"X"` and `"X\nX"`) once per layout; both are logged.
    2. It sets the name and keeps it if it fits in two lines.
    3. Otherwise it drops words from the end (then characters) with "..." until it fits.

    The full name is in the details panel on hover.
- **Icon fitting.**
  - **What the game does.** `StaticImage::SetImageAndImplicitSize` writes the texture id, the texture's pixel size and `bNeedsUpdate` into the image's inline `RenderObject2D` (`m_renderObject`). It keeps `scale`, and the drawn size is size × scale. It also returns early when the texture is unchanged.
  - **What `FitIcon` does.** After every `setImage` it sets scale = min(XML scale, box / size), using the box from `#dk2ml_ep_cell_iconbox`'s `m_size`, then sets `bNeedsUpdate`. Icons are drawn as large as the box allows, never above the XML scale (0.75 weapons, 0.8 tiles). The XML scale is read from the cell template's icon, which is never re-skinned.
- **XML gotchas** (learned on the portrait picker): plain `Item`s can take the cursor, so containers inside a cell are StaticImages with a transparent texture. Templates have no tooltip, because clones would share and double-free it. Text placeholders must be as long as any text set later (`ChangeText` caps at the XML length).
- **Expected offsets** (the dry-run logs them):

  | Field | Offset |
  |---|---|
  | `GameGUI::m_pCustomizationScreen` | 88 |
  | `GUI::Item::m_hashedName` | 40 |
  | `m_parent` | 64 |
  | `m_children` | 72 |
  | `m_size` | 112 |
  | `m_localOrigin` | 120 |
  | `m_hidden` | 128 |
  | `m_alignFlags` | 152 |
  | `m_bStealFocus` | 864 |
  | `m_eventProperties` | 168 |
  | `LinkedList` head / next / owner | 0 / 8 / 24 |
  | `StaticText::m_text` | 960 |
  | `sAction` size | 80 (`pCallback` 72) |
  | `Equipment_Template` type | 32 |
  | `Equipment_Template` guiImage | 80 |
  | `Equipment_Template` category | 96 (HashedString) |
  | `Equipment_Template` inventoryBinding | 112 |
  | `StaticImage::m_renderObject` | 904 |
  | `RenderObject2D::scale` | 60 |
  | `RenderObject2D::size` | 68 |
  | `RenderObject2D::bNeedsUpdate` | 16 |
  | `GameGUI::m_pCustomizeSelection` | 584 |
  | `ObjectLibrary::m_equipmentTemplates` | 128 (a stale layout has 64) |
  | `Roster::Trooper` unit / className / inventory | 224 / 276 / 328 |
  | `StaticString<48>` size / `m_hash` | 52 / 48 |
  | `Equipment_Template` name / tooltip | 40 / 64 |
  | `StaticText::m_textRenderObject` + `sTextRenderObject::color` | 976 + 28 |
  | `Equipment_Client` size / `pItem` | 88 / 0 (a 32-bit layout has 80) |
  | `GUI::Item::m_alignOffset` | 156 |
  | `GUI::Checkbox::m_pButtons` (Button*[2]) | 912 |
  | `GUI::Button::m_renderObjects` (RenderObject2D[3]) | 936 |
  | `RenderObject2D` size | 192 |

  The PDB also has a 32-bit `Equipment_Template` layout (guiImage at 44). `game.cpp` refuses to start on non-8-aligned pointer fields.

## Portrait picker (`picker.cpp`, `portraits.cpp`, `gui/portrait_selector.xml`)
Brought over from the Portrait Selector mod; item names keep its `#dk2ml_ps_` prefix. Its hooks are in the table above.
- **Clicks.** Our XML uses `<Action type="Callback" target="...">`. `sAction::Execute` calls `pCallback(const sAction&)` when it's non-null. The plugin writes `pCallback` into our own actions only (`gui::SetCallback`). The XML loader requires a target that exists.
- **Cloning.** `Item::CloneInternal` is a deep copy: it clones the children and the actions. `CloneActions` copies `pCallback` and sets `owner` = clone, so a cell's callback gets its cell from `action.owner`.
- **Hit-testing** (`GUIManager::FindItemUnderCursor_Recursive`): hidden items are skipped, and children are tested last to first, before their parent. A `stealFocus` item under the cursor returns **itself**, and its children are never tested. So a `stealFocus` background must be an earlier sibling of the controls, never their parent, as in the stock screens.
- **Attach.** Customization_Screen captures input, so the picker must be its child. The open button goes on `#player_profile_panel > #portrait` (other screens have a `#portrait` too). Re-adding the picker with AddChild on open puts it last, which makes it topmost for drawing and hit-testing.
- **The list is virtualized.** The content is a list of rows (`BuildRows`), each either a group header or up to `cols` entries of one group. Only one screen of row slots exists.
  - **Building the slots.** The first time the picker opens (and again after a GUI reload), `cols × visibleRows` cells and one header per slot are cloned into `#dk2ml_ps_grid`, a plain Item. The counts are `floor(grid / (cell + gap))` per axis, so 10 × 11 at the current sizes. Each one is built the way the stock equipment list builds its items:
    1. `Button::CloneInternal(template)` (`Item::CloneInternal` for headers)
    2. `UpdatePointerReferences(clone, clone, root)`
    3. `SetName(clone, nullptr)`
    4. AddChild into the grid
    5. `SetOrigin`: relative to the grid center, +y up
  - **Scrolling** moves a window `[g_top, g_top + visibleRows)` over the rows and re-skins the slots: `StaticImage::SetImageAndImplicitSize`, the current-portrait and squadmate marks, and Show/Hide. A cell that already shows the right entry is skipped unless forced. A huge mod pool therefore costs one screen, not thousands of GUI items.
  - It scrolls in **whole rows**, because nothing clips a partly visible row.
  - Cells are never deleted. That matters because `ExecuteOnEvent` and `OnCursorUp` keep using an item after its action runs, so an item must never be deleted inside its own event. Scrolling inside a cell's own scroll event is safe for the same reason.
- **Input.**
  - Mouse wheel: Callbacks on `EVENT_SCROLL_DOWN`/`EVENT_SCROLL_UP` on the cells, headers, grid and scrollbar scroll 2 card rows per notch (`scroll::Step`, see the grid's scroll stops).
  - Scrollbar: `#dk2ml_ps_thumb` (a child of `#dk2ml_ps_track`) uses `EVENT_CURSOR_DOWN` and `EVENT_DRAG` Callbacks.
  - `Item::ExecuteOnEvent` stores `{owner, cursor}` in `sAction::eventParams` before running an action. The cursor is in the GUI's +y-up space, so the drag maps the cursor delta since the press onto rows. No screen mapping is needed.
  - Header text is set with `StaticText::ChangeText`. It copies the text but caps it at the item's length limit from the XML, so the XML placeholder must be at least as long as any label.
- **Grouping by unit** ("All units"). `sHumanPortrait::unit` is only the hash of the unit name, which is also its `StaticString<48>::m_hash`.
  - `ObjectLibrary::GetUnit(const StaticString<48>&)` compares only `m_hash`. A zeroed key with just the hash set therefore finds the `ArmyOrg::Unit`. That avoids reading `ObjectLibrary::m_units`, whose offset differs between the PDB's layouts.
  - Header text is `Loca::GetTextForId(unit.nameUI)`, upper-cased, falling back to `unit.name`. The PDB also has a 32-bit `ArmyOrg::Unit` layout (`nameUI` @52); `game.cpp` rejects a `nameUI` offset that isn't 8-aligned.
  - Group order: the trooper's unit, then other units by internal name, then OTHER PORTRAITS (hashes with no unit, e.g. a mod's private pool). Within a group, entries are sorted by file name in natural order (`NaturalLess`: case-insensitive, digit runs as numbers, so ranger2 comes before ranger10), for both filters.
  - Every group starts a new row, under its header (see the virtualized list above). `g_cellEntry` maps each visible cell to the entry it shows.
- **Applying a portrait** mirrors `Customize_OnChangePortrait`:
  1. Copy `tex` into `trooper.id.tex` and copy `gender`. Leave `texHash` alone; stock doesn't touch it.
  2. `Roster::Save()`.
  3. `Customize_OnTrooperSelected(trooper)` for the profile refresh (it builds the `_large` texture name itself).
  4. `Deploy_Update` with `m_deploySelection.roster.pTrooper` temporarily set to the trooper.
- **Hover pop-up** (`#dk2ml_ps_preview`): the cell template's `OnHover`/`OnHoverEnd` Callbacks (`EVENT_CURSOR_HOVER`/`_END`, owner = the cell) show and hide it. It shows the large portrait, the file name and "Used by NAME".
  - The pop-up is moved into the grid after the cells are cloned, so it draws above them and shares their coordinates. It's placed beside the hovered cell, never over it (covering the cell would end the hover). It re-renders when scrolling changes what the hovered cell shows.
  - **Large portrait:** `tex` with `_large` inserted before the extension, as the game builds it for the Customization screen.
  - **Layout:** the pop-up is 360×470 and top-anchored. "Used by" is a `sizeX` + `wordWrap` StaticText, the same attributes stock `campaign.xml` uses, so a long list wraps downwards. A CURRENT badge (`#dk2ml_ps_preview_current`) marks the trooper's own portrait.
  - **Missing `_large` fallback:** when there is no `_large` version, the pop-up shows the cell texture in `#dk2ml_ps_preview_img_small` (scale 3) instead.
    - Existence is checked with `FileManagerImpl::FileExists(*g_fileManager, path)`. It goes through `SearchFileInDataFolders`: game data, packed archives and mods.
    - The result is cached per path.
  - **Size:** `StaticImage::SetImageAndImplicitSize` replaces the `RenderObject2D` texture (+8) and `size` (+68) but keeps `scale` (+60), so the XML `scaleX`/`scaleY` (0.6) still apply.
- **Roster-wide usage** (`portraits::UsedInRoster`): walks `Roster::m_squads` (`List<Roster::Squad *>`), with each squad's inline trooper array validated as below.
  - Squadmates' portraits get the strong dim (`#dk2ml_ps_cell_used`); portraits worn only in other squads get the light one (`_used_other`).
  - The pop-up lists squadmates first, then "NAME (SQUAD)" from `Squad::name` (`StaticString<32>`).
  - Cells have no tooltips.
- **Buttons.** THIS UNIT and ALL UNITS sit at the top left, HIDE USED and RANDOM at the top right, flush with the grid edges (53 px in, using `lt`/`rt` aligns). RANDOM is styled orange as an action.
  - The status line between the two groups (`#dk2ml_ps_status_count` and `_used`) is set in `Refresh`: "N PORTRAITS", then "N IN USE", or "N HIDDEN" with HIDE USED on.
  - Headers read "TITLE (count)".
  - HIDE USED drops portraits anyone else wears, but keeps the trooper's own. It uses `portraits::Without`, which rebuilds the groups.
  - RANDOM applies a random unused entry from the current list, then refreshes; the picker stays open.
  - The title is "CHOOSE PORTRAIT: NAME", set with `ChangeText` on every refresh.
- **Sticky header.** When the top row is in the middle of a titled group, slot 0 shows that group's header and the rows shift down one slot.
  - `MaxTop` allows one extra row when headers exist, so the last row stays reachable.
  - `g_cellEntry` still maps every visible cell, so clicks and hover don't care.
  - `Roster::GetSquadForTrooper` returns the squad. `Squad::roster` is an inline `Roster::Trooper[]` with stride `sizeof(Roster::Trooper)` (1936), and `numInRoster` entries are filled in. The count is validated against `diedInCampaign` (the next field).
  - A trooper's name is `sHumanId::name.m_szString`.
  - Keep the cell template tooltip-less: `Item::SetTooltip` frees the item's string, so a clone sharing the template's buffer would free it twice.
- **Stock arrow bug** (from the disassembly of `Customize_OnChangePortrait`):
  1. The current index is searched only among portraits of the trooper's unit with class 0 or the trooper's class. If the trooper's `tex` isn't found, the index silently stays **0** (library entry 0, a stock Ranger portrait).
  2. The step accepts a candidate of the unit when the **current portrait's** class (not the trooper's) is 0 or matches.

  So a trooper wearing a portrait from outside its class pool falls back to index 0 (class 0) and gets "any portrait of the unit". NEXT then always lands on the unit's first portrait in load order and PREV on its last: stuck on 2–3 portraits. The Blue Vector intel submod hits this (OGA 2 troopers ending up with OGA 1 `cia*` portraits).
  - **Our replacement:** steps through `ForTrooper(trooper, ThisUnit)`, the same pool and natural order as the picker, with wrap-around. An out-of-pool portrait goes to the pool's first (NEXT) or last (PREV) entry. It applies with the picker's `Apply`.
  - It falls back to the stock code only when there's no trooper or the pool is empty.
- **Portrait pool.** `ObjectLibrary::m_humanPortraits` (`List<sHumanPortrait>`) is merged from every `human_identities` XML, including mods'. The "this unit" filter matches stock: unit hash equal, and class hash equal, or 0 only when no portrait of that unit names the trooper's class (stock rule from `human_identities.xml`: once a class has class-tagged portraits, only those are used for it). Gender 0 is male, 1 is female; mods use other values for their own pools.
- **Expected offsets** (from the disassembly; the PDB has several layouts of `ObjectLibrary` / `GameGUI`, and the dry-run logs what was picked):

  | Field | Offset |
  |---|---|
  | `m_pCustomizeSelection` | 584 |
  | `m_pCustomizationScreen` | 88 |
  | deploy pTrooper | 512+24+0 |
  | `m_humanPortraits` | 496 (not 288) |
  | `sHumanId::tex` | 52 |
  | gender | 184 |
  | unit hash | 224+48 |
  | class hash | 276+48 |
  | `sHumanPortrait` size | 152 |
  | `sAction` size / `pCallback` | 80 / 72 |
  | `sItemEventProperties` size | 48 |

  `portraits.cpp` validates the pool at runtime and disables the picker if it looks wrong.
