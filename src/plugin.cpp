// Era's UI Improvements, for the trooper Customization screen:
//   - the equipment list as a dense, grouped icon grid (grid.cpp, entries.cpp)
//   - inventory slots the trooper has nothing for hidden (slots.cpp), and the Concealment bar for classes without
//     concealment (concealment.cpp)
//   - Esc / Back closing an open equipment list or the portrait picker first (back.cpp, picker.cpp)
//   - a portrait picker, and the stock PREV/NEXT portrait arrows fixed (picker.cpp, portraits.cpp)
//   - the trooper hover tooltip shows Support Gear 2/3 (lockpick, crowbar, halligan, ...) with names (hudtip.cpp)
//   - in missions, the action wheel's target and path pages on one wheel (wheel.cpp)
//   - Trooper Toolbox's second-page slots on the main page, its page button gone (ttmerge.cpp)
//   - a loadouts panel: per operator and per squad, 4 saved loadouts each for guns, gear and looks (loadouts/)
//   - our own mouse cursor, sized to the screen (cursor.cpp; no hooks: the FRAME and WINDOW_RESIZED events)
//   - on the campaign selection screen, which squads completed each campaign (cards.cpp; from the Item::Show hook)
//   - an on/off switch per feature on this mod's page of the loader's Native mods screen (settings.cpp)
//
// Hooks are safe hooks (dk2ml.h). Several plugins may safe-hook the same function (the loader keeps a chain), so
// sharing one with another mod is fine. GUI reloads, the frame tick and window resizes come from the loader's events,
// lookups, actions and our XML Callback actions from its GUI kit.
// (This plugin replaces the separate Portrait Selector, which hooks the same Customize_* functions as here: don't
// run both.)
#include "campaigns/cards.h"
#include "core/game.h"
#include "core/settings.h"
#include "equipment/back.h"
#include "equipment/grid.h"
#include "hud/cursor.h"
#include "hud/hudtip.h"
#include "hud/wheel.h"
#include "loadouts/store.h"
#include "loadouts/panel.h"
#include "portraits/picker.h"
#include "slots/concealment.h"
#include "slots/slots.h"
#include "slots/ttmerge.h"

// The loader reads this from the DLL without running it. It gives the name on the Native mods screen and in the
// Workshop prompt, and the plugin API version this needs.
DK2ML_PLUGIN_MANIFEST(1,"Era's UI Improvements", PLUGIN_VERSION, "Gabriel Bunselmeyer", "", 112);

namespace {

// a pointer kept in scratch[] from a pre for its post
template <typename T>
T* Kept(uint64_t slot)
{
    return reinterpret_cast<T*>(slot);
}

// void GameGUI::Customize_OnTrooperSelected(const Roster::Trooper*): the screen opened or another trooper was picked
int OnTrooperSelectedPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = reinterpret_cast<uint64_t>(dk2ml::Arg<const void*>(r, 1));
    return DK2ML_CALL_ORIGINAL;
}

void OnTrooperSelectedPost(DK2ML_Regs* r, void*)
{
    if (!Picker_IsApplying())
        Picker_OnTrooperSelected(Kept<const void>(r->scratch[0]));
    TtMerge_Apply(); // before Slots_Refresh, which may hide merged slots the trooper has nothing for
    // after the game's own #inventory_panel refresh (which only shows/hides the attachment and NVG slots)
    Slots_Refresh();
    TtMerge_Arrange();
    Concealment_Refresh();
    Loadouts_Refresh();
}

// bool GameGUI::Customize_OnEvent(unsigned eventId, const GUI::sEventParams*)
int CustomizeOnEventPre(DK2ML_Regs* r, void*)
{
    auto id = static_cast<int64_t>(dk2ml::Arg<uint32_t>(r, 1));
    r->scratch[0] = static_cast<uint64_t>(id);
    if (id == game::GUI_GAME_CUSTOMIZE_BACK.Get()) {
        // Esc / Space / the background / the profile's BACK: close the innermost thing that's open, not the screen
        if (Picker_IsOpen()) {
            Picker_Close();
            dk2ml::SetResult(r, true);
            return DK2ML_SKIP_ORIGINAL;
        }
        if (g_settings.backKey && Back_CloseEquipmentList()) {
            dk2ml::SetResult(r, true);
            return DK2ML_SKIP_ORIGINAL;
        }
    }
    if (id == game::GUI_GAME_CUSTOMIZE_CLOSED.Get())
        Picker_Close();
    return DK2ML_CALL_ORIGINAL;
}

void CustomizeOnEventPost(DK2ML_Regs* r, void*)
{
    Loadouts_OnCustomizeEvent(); // an equip, Paste loadout, a list closing: the panel's marks and visibility
    if (static_cast<int64_t>(r->scratch[0]) == game::GUI_GAME_CUSTOMIZE_OPENED.Get()) {
        Picker_Attach();
        TtMerge_Apply(); // after TT's GUI_CUSTOMIZE_SHOW_DEFAULT from the screen's OnOpen
        Loadouts_Attach();
    }
}

// void GameGUI::Customize_OnChangePortrait(bool next): the PREV/NEXT arrows on the large portrait. Replaced, because
// the stock version gets stuck when a trooper wears a portrait from outside its class's pool (Picker_StepPortrait).
int OnChangePortraitPre(DK2ML_Regs* r, void*)
{
    return Picker_StepPortrait(dk2ml::Arg<bool>(r, 1)) ? DK2ML_SKIP_ORIGINAL : DK2ML_CALL_ORIGINAL;
}

// void GUI::ItemList::Init(): the stock equipment list was just filled (or another list laid out)
int ItemListInitPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = reinterpret_cast<uint64_t>(dk2ml::Arg<void*>(r, 0));
    return DK2ML_CALL_ORIGINAL;
}

void ItemListInitPost(DK2ML_Regs* r, void*)
{
    Grid_OnListInit(Kept<void>(r->scratch[0]));
}

// void GUI::Item::Show() / Hide(): runs for every item shown or hidden, so the callbacks bail out fast
int ItemShowPre(DK2ML_Regs* r, void*)
{
    auto* item = dk2ml::Arg<const void*>(r, 0);
    Campaigns_OnShow(item); // the campaign selection screen opening
    Loadouts_OnShow(item);  // an equipment list opening
    if (!Slots_IsInternal() && TtMerge_SkipShow(item))
        return DK2ML_SKIP_ORIGINAL;
    TtMerge_OnShow(item);
    return Slots_OnShow(item) ? DK2ML_SKIP_ORIGINAL : DK2ML_CALL_ORIGINAL;
}

int ItemHidePre(DK2ML_Regs* r, void*)
{
    auto* item = dk2ml::Arg<const void*>(r, 0);
    if (!Slots_IsInternal() && TtMerge_SkipHide(item))
        return DK2ML_SKIP_ORIGINAL; // TT hiding a merged slot (it's hidden with #inventory_panel instead)
    Slots_OnHide(item);
    Loadouts_OnHide(item);
    return DK2ML_CALL_ORIGINAL;
}

// void UpdateHumanTooltip(GUI::Item* root, ..., const Equipment_Client* equipment (9th argument), ...)
int UpdateHumanTooltipPre(DK2ML_Regs* r, void*)
{
    void* root = dk2ml::Arg<void*>(r, 0);
    r->scratch[0] = reinterpret_cast<uint64_t>(root);
    r->scratch[1] = reinterpret_cast<uint64_t>(dk2ml::Arg<const void*>(r, 8));
    HudTip_Before(root);
    return DK2ML_CALL_ORIGINAL;
}

void UpdateHumanTooltipPost(DK2ML_Regs* r, void*)
{
    HudTip_After(Kept<void>(r->scratch[0]), Kept<const void>(r->scratch[1]));
}

// void GameGUI::OpenWaypointMenu(const ActionWP_Client* wp, eOpenWaypointsType type): the mission action wheel
int OpenWaypointMenuPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = reinterpret_cast<uint64_t>(dk2ml::Arg<const void*>(r, 1));
    r->scratch[1] = static_cast<uint64_t>(dk2ml::Arg<int>(r, 2));
    return DK2ML_CALL_ORIGINAL;
}

void OpenWaypointMenuPost(DK2ML_Regs* r, void*)
{
    Wheel_OnOpened(Kept<const void>(r->scratch[0]), static_cast<int>(r->scratch[1]));
}

// bool GameGUI::WaypointMenu_OnEvent(int eventId, const GUI::sEventParams*)
int WaypointMenuOnEventPre(DK2ML_Regs* r, void*)
{
    Wheel_OnEvent(dk2ml::Arg<int>(r, 1), dk2ml::Arg<const void*>(r, 2));
    return DK2ML_CALL_ORIGINAL;
}

// The game rebuilt its GUI: every item a module found before is gone, and its address may be reused, so each module
// forgets its items and finds the new ones the next time it needs them.
void OnGuiLoaded(const DK2ML_Event*, void*)
{
    Picker_OnGuiLoaded();
    Grid_OnGuiLoaded();
    Slots_OnGuiLoaded();
    Concealment_OnGuiLoaded();
    TtMerge_OnGuiLoaded();
    HudTip_OnGuiLoaded();
    Campaigns_OnGuiLoaded();
    Loadouts_OnGuiLoaded();
    Wheel_OnGuiLoaded();
}

void OnFrame(const DK2ML_Event*, void*)
{
    Cursor_Tick();
    Picker_Tick();
    Loadouts_Tick();
}

void OnWindowResized(const DK2ML_Event*, void*)
{
    Cursor_OnResized();
}

} // namespace

DK2ML_EXPORT int DK2ML_PluginInit(const DK2ML_API* api, const DK2ML_PluginInfo* info)
{
    // the loader only ever appends to its function table, so a shorter one lacks functions this plugin calls
    if (api->structSize < sizeof(DK2ML_API)) {
        api->Log("needs a newer Native Mod Loader (update dk2ml.dll and dbghelp.dll from the mod loader release)");
        return 1;
    }
    game::api = api;
    if (!game::Resolve())
        return 2;
    Settings_Load();
    loadouts::Load();
    Settings_Register(api);

    using namespace game;
    bool ok = dk2ml::On(api, DK2ML_EVENT_GUI_LOADED, OnGuiLoaded) && dk2ml::On(api, DK2ML_EVENT_FRAME, OnFrame) &&
              dk2ml::On(api, DK2ML_EVENT_WINDOW_RESIZED, OnWindowResized) &&
              dk2ml::Hook(api, GameGUI_Customize_OnTrooperSelected, OnTrooperSelectedPre, OnTrooperSelectedPost) &&
              dk2ml::Hook(api, GameGUI_Customize_OnEvent, CustomizeOnEventPre, CustomizeOnEventPost) &&
              dk2ml::Hook(api, GameGUI_Customize_OnChangePortrait, OnChangePortraitPre) &&
              dk2ml::Hook(api, GUI_ItemList_Init, ItemListInitPre, ItemListInitPost) &&
              dk2ml::Hook(api, GUI_Item_Show, ItemShowPre) &&
              dk2ml::Hook(api, GUI_Item_Hide, ItemHidePre) &&
              dk2ml::Hook(api, UpdateHumanTooltip, UpdateHumanTooltipPre, UpdateHumanTooltipPost);
    if (!ok)
        return 3;
    // optional: without it (logged in Resolve) the action wheel keeps its two pages
    if (WheelUsable() && !(dk2ml::Hook(api, GameGUI_OpenWaypointMenu, OpenWaypointMenuPre, OpenWaypointMenuPost) &&
                           dk2ml::Hook(api, GameGUI_WaypointMenu_OnEvent, WaypointMenuOnEventPre)))
        return 3;

    Cursor_Start(info ? info->modDir : nullptr);

    api->Log("ready");
    return 0;
}
