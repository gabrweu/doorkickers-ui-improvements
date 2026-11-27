// The equipment grid (gui/equipment_picker.xml): replaces the Customization screen's one-column equipment list.
#pragma once

// GUI::ItemList::Init just ran on `list`. When that's the stock equipment list (#equipment_list_parent), the game has
// just filled it for a slot: show the grid in its place, or the stock list for slots the grid doesn't handle.
void Grid_OnListInit(void* list);
// Settings switch: off shows the stock list again (now and for every list built from then on).
void Grid_SetEnabled(bool on);
// The game reloaded its GUI (the loader's GUI_LOADED event): forgets every item; the next list attaches anew.
void Grid_OnGuiLoaded();
