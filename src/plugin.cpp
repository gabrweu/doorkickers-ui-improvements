// Era's UI Improvements, for the trooper Customization screen:
//   - the equipment list as a dense, grouped icon grid (grid.cpp, entries.cpp)
//   - inventory slots the trooper has nothing for hidden (slots.cpp)
//   - Esc / Back closing an open equipment list or the portrait picker first (back.cpp, picker.cpp)
//   - a portrait picker, and the stock PREV/NEXT portrait arrows fixed (picker.cpp, portraits.cpp)
//   - the trooper hover tooltip shows Support Gear 2/3 (lockpick, crowbar, halligan, ...) with names (hudtip.cpp)
//   - Trooper Toolbox's second-page slots on the main page, its page button gone (ttmerge.cpp)
//   - our own mouse cursor, sized to the screen (cursor.cpp; no hooks)
//   - an on/off switch per feature on this mod's page of the loader's Native mods screen (settings.cpp)
//
// Hooks are safe hooks (dk2ml.h), and only on functions the freecam plugin doesn't hook: MinHook can't hook a
// function twice, so two plugins hooking the same one would make the second fail to load. Freecam hooks
// GUI::sAction::Execute, ImGui::Render, GameGUI::IsAnyMenuOpened and the camera. (This plugin replaces the separate
// Portrait Selector, which hooked the same Customize_* functions as here.)
#include <cstddef>

#include "back.h"
#include "cursor.h"
#include "dk2ml.h"
#include "game.h"
#include "grid.h"
#include "hudtip.h"
#include "picker.h"
#include "settings.h"
#include "slots.h"
#include "ttmerge.h"

namespace {

template <typename T>
T* Ptr(uint64_t reg)
{
    return reinterpret_cast<T*>(reg);
}

// void GameGUI::Customize_OnTrooperSelected(const Roster::Trooper*): the screen opened or another trooper was picked
int OnTrooperSelectedPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = r->rdx;
    return DK2ML_CALL_ORIGINAL;
}

void OnTrooperSelectedPost(DK2ML_Regs* r, void*)
{
    if (!Picker_IsApplying())
        Picker_OnTrooperSelected(Ptr<const void>(r->scratch[0]));
    TtMerge_Apply(); // before Slots_Refresh, which may hide merged slots the trooper has nothing for
    // after the game's own #inventory_panel refresh (which only shows/hides the attachment and NVG slots)
    Slots_Refresh();
    TtMerge_Arrange();
}

// bool GameGUI::Customize_OnEvent(unsigned eventId, const GUI::sEventParams*)
int CustomizeOnEventPre(DK2ML_Regs* r, void*)
{
    auto id = static_cast<int64_t>(static_cast<uint32_t>(r->rdx));
    r->scratch[0] = static_cast<uint64_t>(id);
    if (id == game::off.customizeBack) {
        // Esc / Space / the background / the profile's BACK: close the innermost thing that's open, not the screen
        if (Picker_IsOpen()) {
            Picker_Close();
            r->rax = 1;
            return DK2ML_SKIP_ORIGINAL;
        }
        if (g_settings.backKey && Back_CloseEquipmentList()) {
            r->rax = 1;
            return DK2ML_SKIP_ORIGINAL;
        }
    }
    if (id == game::off.customizeClosed)
        Picker_Close();
    return DK2ML_CALL_ORIGINAL;
}

void CustomizeOnEventPost(DK2ML_Regs* r, void*)
{
    if (static_cast<int64_t>(r->scratch[0]) == game::off.customizeOpened) {
        Picker_Attach();
        TtMerge_Apply(); // after TT's GUI_CUSTOMIZE_SHOW_DEFAULT from the screen's OnOpen
    }
}

// void GameGUI::Customize_OnChangePortrait(bool next): the PREV/NEXT arrows on the large portrait. Replaced, because
// the stock version gets stuck when a trooper wears a portrait from outside its class's pool (Picker_StepPortrait).
int OnChangePortraitPre(DK2ML_Regs* r, void*)
{
    bool next = (r->rdx & 0xff) != 0;
    return Picker_StepPortrait(next) ? DK2ML_SKIP_ORIGINAL : DK2ML_CALL_ORIGINAL;
}

// void GUI::ItemList::Init(): the stock equipment list was just filled (or another list laid out)
int ItemListInitPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = r->rcx;
    return DK2ML_CALL_ORIGINAL;
}

void ItemListInitPost(DK2ML_Regs* r, void*)
{
    Grid_OnListInit(Ptr<void>(r->scratch[0]));
}

// void GUI::Item::Show() / Hide(): runs for every item shown or hidden, so the callbacks bail out fast
int ItemShowPre(DK2ML_Regs* r, void*)
{
    auto* item = Ptr<const void>(r->rcx);
    if (!Slots_IsInternal() && TtMerge_SkipShow(item))
        return DK2ML_SKIP_ORIGINAL;
    TtMerge_OnShow(item);
    return Slots_OnShow(item) ? DK2ML_SKIP_ORIGINAL : DK2ML_CALL_ORIGINAL;
}

int ItemHidePre(DK2ML_Regs* r, void*)
{
    auto* item = Ptr<const void>(r->rcx);
    if (!Slots_IsInternal() && TtMerge_SkipHide(item))
        return DK2ML_SKIP_ORIGINAL; // TT hiding a merged slot (it's hidden with #inventory_panel instead)
    Slots_OnHide(item);
    return DK2ML_CALL_ORIGINAL;
}

// void UpdateHumanTooltip(GUI::Item* root, ..., const Equipment_Client* equipment (9th argument), ...)
int UpdateHumanTooltipPre(DK2ML_Regs* r, void*)
{
    r->scratch[0] = r->rcx;
    r->scratch[1] = r->stack[9];
    HudTip_Before(Ptr<void>(r->rcx));
    return DK2ML_CALL_ORIGINAL;
}

void UpdateHumanTooltipPost(DK2ML_Regs* r, void*)
{
    HudTip_After(Ptr<void>(r->scratch[0]), Ptr<const void>(r->scratch[1]));
}

template <typename T>
bool Hook(T target, DK2ML_PreFn pre, DK2ML_PostFn post = nullptr)
{
    void* t = reinterpret_cast<void*>(target);
    return game::api->CreateSafeHook(t, pre, post, nullptr) == DK2ML_OK && game::api->EnableHook(t) == DK2ML_OK;
}

} // namespace

DK2ML_EXPORT int DK2ML_PluginInit(const DK2ML_API* api, const DK2ML_PluginInfo* info)
{
    // v4: GetConfigDir (settings.ini). v5 adds AddOption (the Native mods screen); without it the switches can only
    // be edited in settings.ini
    if (api->apiVersion < 4 || api->structSize < offsetof(DK2ML_API, GetConfigDir) + sizeof(void*)) {
        api->Log("needs mod loader api v4 or newer (update dbghelp.dll from the mod loader release)");
        return 1;
    }
    game::api = api;
    if (!game::Resolve())
        return 2;
    Settings_Load();
    if (api->apiVersion >= 5 && api->structSize >= offsetof(DK2ML_API, AddOption) + sizeof(void*))
        Settings_Register(api);
    else
        api->Log("settings: mod loader api v%u has no options menu; edit settings.ini instead", api->apiVersion);
    using namespace game;
    if (!Hook(fn.customizeOnTrooperSelected, OnTrooperSelectedPre, OnTrooperSelectedPost) ||
        !Hook(fn.customizeOnEvent, CustomizeOnEventPre, CustomizeOnEventPost) ||
        !Hook(fn.customizeOnChangePortrait, OnChangePortraitPre) ||
        !Hook(fn.itemListInit, ItemListInitPre, ItemListInitPost) ||
        !Hook(fn.itemShow, ItemShowPre) ||
        !Hook(fn.itemHide, ItemHidePre) ||
        !Hook(fn.updateHumanTooltip, UpdateHumanTooltipPre, UpdateHumanTooltipPost))
        return 3;

    Cursor_Start(info ? info->modDir : nullptr);

    api->Log("ready");
    return 0;
}
