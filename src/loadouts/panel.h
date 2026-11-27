// Loadouts panel, left of the Customization screen (gui/loadout_panel.xml): the operator's own saved loadouts and the
// squad's (team profiles), 5 each, every one the whole loadout. A row loads a profile, its SAVE button stores
// everything equipped, its pencil renames it.
#pragma once

// GUI_GAME_CUSTOMIZE_OPENED: moves the panel into the screen (once per GUI) and refreshes it.
void Loadouts_Attach();
// Customize_OnTrooperSelected ran: the panel for the (new) trooper.
void Loadouts_Refresh();
// Customize_OnEvent ran (an equip, Paste loadout, a list opening or closing...): refreshes the "equipped" marks and
// whether the panel shows.
void Loadouts_OnCustomizeEvent();
// GUI::Item::Show / Hide are about to run on item: the panel hides while an equipment list is open.
void Loadouts_OnShow(const void* item);
void Loadouts_OnHide(const void* item);
// The loader's FRAME event: while a loadout is being renamed, takes the name once its box loses focus (Enter, Esc or
// a click elsewhere). Returns at once otherwise.
void Loadouts_Tick();
// Settings switch: shows or hides the panel.
void Loadouts_SetEnabled(bool on);
// The game reloaded its GUI (the loader's GUI_LOADED event): the panel is looked up again on the next Attach.
void Loadouts_OnGuiLoaded();
