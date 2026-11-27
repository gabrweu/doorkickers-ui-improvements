// Esc and Space (the screen's OnKeyDown), a click on the background and the profile's BACK button all trigger
// GUI_GAME_CUSTOMIZE_BACK, which closes the whole Customization screen (GameGUI::Customize_OnEvent), even with an
// equipment list open. While one is open, the Customize_OnEvent hook skips the stock handling and the list's own
// BACK button (#customize_back_eqplist) is "clicked" instead: its XML closes the list the right way for whichever
// customization.xml is loaded (stock: uncheck the slots, hide the panel; Trooper Toolbox: GUI_CUSTOMIZE_SHOW_DEFAULT,
// which also brings its slots and background back).
#include "equipment/back.h"

#include "core/game.h"
#include "core/gui.h"

bool Back_CloseEquipmentList()
{
    void* gameGUI = game::GameGUI();
    void* screen = gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizationScreen.Offset()) : nullptr;
    void* panel = gui::Find(screen, "#equipment_list_panel");
    if (!screen || gui::IsHidden(screen) || !panel || gui::IsHidden(panel))
        return false; // no list open: BACK closes the screen as usual

    if (void* back = gui::Find(panel, "#customize_back_eqplist"))
        dk2ml::gui::Click(game::api, back);
    else
        gui::ShowIf(panel, false); // its OnClose shows the profile again
    return true;
}
