// Hides the Customization screen's inventory slots (PrimaryWeapon .. Custom9) that the trooper has nothing for.
#pragma once

// The customized trooper was (re)selected (GameGUI::Customize_OnTrooperSelected ran): work out again which slots have
// options for it, hide those that don't and bring back those that do.
void Slots_Refresh();
// GUI::Item::Show is about to run on item. True: skip it (a slot we keep hidden; it's remembered that something
// wanted it shown, for when the slot gets options again).
bool Slots_OnShow(const void* item);
// GUI::Item::Hide is about to run on item.
void Slots_OnHide(const void* item);
// Settings switch: off brings back every slot we hid (as its own visibility would be) and stops hiding.
void Slots_SetEnabled(bool on);
// The game reloaded its GUI (the loader's GUI_LOADED event): the slots are looked up again on the next refresh.
void Slots_OnGuiLoaded();
// True while slots.cpp itself is showing or hiding a slot (those calls aren't filtered by anyone else).
bool Slots_IsInternal();
