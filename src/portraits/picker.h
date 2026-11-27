// The portrait picker: a grid of portraits (gui/portrait_selector.xml) on the trooper Customization screen.
#pragma once

// Puts our GUI items into the Customization screen (idempotent; cheap enough to call on every screen event).
void Picker_Attach();
// The game reloaded its GUI (the loader's GUI_LOADED event): drops every cached item and cell.
void Picker_OnGuiLoaded();
// The customized trooper changed (Customize_OnTrooperSelected ran).
void Picker_OnTrooperSelected(const void* trooper);
// Every frame (the loader's FRAME event): keeps the portrait's (i) buttons moved down beside our button (or back, with
// the switch off). Returns at once until the screen's portrait has been found.
void Picker_Tick();
bool Picker_IsOpen();
void Picker_Close();
// True while we call Customize_OnTrooperSelected ourselves to refresh the screen after applying a portrait.
bool Picker_IsApplying();
// The stock PREV/NEXT portrait arrows, done right: steps through the trooper's own pool (the picker's "This unit"
// list), and a portrait from outside it goes to the pool's first (next) or last (prev) one. Returns false if there's
// nothing to do it with (no trooper, empty pool), in which case the stock code should run.
bool Picker_StepPortrait(bool next);
// Settings switch: off hides the picker's button (and the picker) and leaves the PREV/NEXT arrows to the stock code.
void Picker_SetEnabled(bool on);
