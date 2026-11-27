// Esc / Back with an equipment list open: close the list, not the whole Customization screen.
#pragma once

// GUI_GAME_CUSTOMIZE_BACK is about to be handled. If an equipment list is open, closes it (the way its own BACK
// button does) and returns true: the event is handled, the screen stays open.
bool Back_CloseEquipmentList();
