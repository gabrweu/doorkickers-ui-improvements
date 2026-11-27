// Inventory slots without options. Trooper Toolbox adds slots (Custom1..9, SupportGear2/3) that every trooper sees,
// whether or not its class has anything for them; stock slots can be empty for a class too (eg. support gear).
//
// Which items a slot offers is the game's rule from building the equipment list (0x140113B90 in build 112, its
// generic path): a template whose inventoryBinding is the slot's name (contained in it, for UtilityPouch1/2 and
// SupportGear1-3), that's bound to the trooper (0x140112B40: ObjectLibrary::IsEquipmentBoundTo with the template's
// name or its type's name, to the trooper's class or unit), and that has a tooltip. The EMPTY item doesn't count as
// an option. A slot with an item equipped always stays, so nothing gets stranded.
//
// The game itself only shows/hides the weapon attachment slots and HelmetNVG (the #inventory_panel refresh,
// 0x140117330); those are left to it. Other slots it never shows or hides, but Trooper Toolbox does from XML
// (GUI_CUSTOMIZE_SHOW_DEFAULT shows them all again whenever the screen opens or the equipment list closes, its
// cosmetics button shows Custom7-9). So a slot we hide is kept hidden by skipping GUI::Item::Show on it, and we note
// whether anything wanted it shown (or hidden again since), to restore that when the slot gets options again.
#include "slots/slots.h"

#include <cstring>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"
#include "slots/ttmerge.h"

namespace {

// MSVC's std::unordered_map: its std::list of nodes has its head at +8; a node is {next, prev, pair<key, value>}, the
// value (Equipment_Template*) at +0x18. The game walks ObjectLibrary::m_equipmentTemplates the same way.
constexpr int32_t kMapListHead = 8;
constexpr int32_t kNodeNext = 0;
constexpr int32_t kNodeValue = 0x18;

struct Slot {
    const void* item;
    bool suppressed; // we keep it hidden
    bool wantShown;  // what its visibility would be without us (the last Show/Hide anyone else asked for)
};

std::vector<Slot> g_slots;   // items of the current GUI: emptied when the game reloads it (Slots_OnGuiLoaded)
bool g_internal = false;      // our own Show/Hide calls

Slot* Find(const void* item)
{
    for (Slot& s : g_slots)
        if (s.item == item)
            return &s;
    return nullptr;
}

// Whether we know the slots of the current GUI. A GUI reload empties the list (Slots_OnGuiLoaded): the old items are
// gone, and their addresses may be reused, so comparing pointers couldn't tell.
bool CheckRoot()
{
    return !g_slots.empty();
}

void SetShown(const void* item, bool show)
{
    g_internal = true;
    gui::ShowIf(const_cast<void*>(item), show);
    g_internal = false;
}

bool GameManaged(int64_t slot)
{
    for (const dk2ml::Enum& managed : game::Inventory_GameManaged)
        if (slot == managed.Get())
            return true;
    return false;
}

struct Candidate {
    int64_t type;     // Inventory::eType
    const char* name; // the slot Checkbox's name
    const void* item;
    bool hasOptions;
};

bool Matches(const Candidate& c, const char* binding)
{
    bool partial = c.type >= game::Inventory_UTILITY_POUCH1.Get() && c.type <= game::Inventory_SUPPORT_GEAR3.Get();
    return partial ? strstr(c.name, binding) != nullptr : strcmp(c.name, binding) == 0;
}

} // namespace

void Slots_Refresh()
{
    if (!g_settings.emptySlots)
        return;
    void* gameGUI = game::GameGUI();
    const void* library = game::ObjectLibrary();
    void* screen = gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizationScreen.Offset()) : nullptr;
    const void* trooper = gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizeSelection.Offset()) : nullptr;
    if (!screen || gui::IsHidden(screen) || !trooper || !library)
        return; // Customize_OnTrooperSelected does nothing while the screen is hidden; neither do we
    CheckRoot();

    // the slots on this screen, each with options already if the trooper has something (not EMPTY) in it
    std::vector<Candidate> slots;
    for (int64_t type = 1; type < game::Inventory_NUM_VALUES.Get(); type++) {
        if (GameManaged(type))
            continue;
        const char* name = game::Inventory_GetNameForEnum(static_cast<int>(type));
        void* item = name && *name ? gui::Find(screen, name) : nullptr;
        if (!item)
            continue; // eg. no Custom slots without Trooper Toolbox
        const char* equipped = &dk2ml::At<char>(trooper, game::Roster_Trooper_inventory.Offset() + static_cast<int32_t>(type * game::sizeof_StaticString48.Get()));
        bool has = false;
        if (dk2ml::At<unsigned>(equipped, game::StaticString48_m_hash.Offset()) != 0) {
            const void* tmpl = game::ObjectLibrary_GetEquipment(library, equipped + game::StaticString48_m_szString.Offset());
            has = tmpl && dk2ml::At<int>(tmpl, game::Equipment_Template_type.Offset()) != game::EquipmentType_EMPTY.Get();
        }
        slots.push_back({type, name, item, has});
    }

    // one pass over every template, checking the trooper's binds only for templates that would fill a slot still
    // without options
    const char* map = &dk2ml::At<char>(library, game::ObjectLibrary_m_equipmentTemplates.Offset());
    const void* head = dk2ml::At<void*>(map, kMapListHead);
    const void* node = head ? dk2ml::At<void*>(head, kNodeNext) : nullptr;
    for (int guard = 0; node && node != head && guard < 1000000; guard++, node = dk2ml::At<void*>(node, kNodeNext)) {
        const void* tmpl = dk2ml::At<void*>(node, kNodeValue);
        if (!tmpl || dk2ml::At<int>(tmpl, game::Equipment_Template_type.Offset()) == game::EquipmentType_EMPTY.Get() || !dk2ml::At<const char*>(tmpl, game::Equipment_Template_tooltip.Offset()))
            continue;
        const char* binding = dk2ml::At<const char*>(tmpl, game::Equipment_Template_inventoryBinding.Offset());
        if (!binding || !*binding)
            continue;
        bool wanted = false;
        for (const Candidate& c : slots)
            wanted |= !c.hasOptions && Matches(c, binding);
        if (!wanted || !game::BoundToTrooper(library, tmpl, trooper))
            continue;
        for (Candidate& c : slots)
            c.hasOptions |= Matches(c, binding);
    }

    int hidden = 0;
    for (const Candidate& c : slots) {
        Slot* s = Find(c.item);
        if (!c.hasOptions) {
            hidden++;
            if (!s) {
                g_slots.push_back({c.item, false, false});
                s = &g_slots.back();
            }
            if (!s->suppressed) {
                s->suppressed = true;
                s->wantShown = !gui::IsHidden(c.item);
                SetShown(c.item, false);
            }
        } else if (s && s->suppressed) {
            s->suppressed = false;
            if (s->wantShown)
                SetShown(c.item, true);
        }
    }
    game::api->Log("slots: %d of %d hidden (no options for this trooper)", hidden, static_cast<int>(slots.size()));
}

bool Slots_OnShow(const void* item)
{
    if (g_internal || !CheckRoot())
        return false;
    Slot* s = Find(item);
    if (!s || !s->suppressed)
        return false;
    s->wantShown = true;
    return true;
}

void Slots_OnHide(const void* item)
{
    if (g_internal || !CheckRoot())
        return;
    if (Slot* s = Find(item); s && s->suppressed)
        s->wantShown = false;
}

void Slots_SetEnabled(bool on)
{
    if (on)
        return; // the next trooper select hides what needs hiding
    for (const Slot& s : g_slots) // all of the current GUI (a reload empties the list)
        if (s.suppressed && s.wantShown)
            SetShown(s.item, true);
    g_slots.clear();
    TtMerge_Arrange();
}

void Slots_OnGuiLoaded()
{
    g_slots.clear();
}

bool Slots_IsInternal()
{
    return g_internal;
}
