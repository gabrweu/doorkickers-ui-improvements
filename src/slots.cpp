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
#include "slots.h"

#include <cstring>
#include <vector>

#include "game.h"
#include "gui.h"
#include "settings.h"
#include "ttmerge.h"

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

std::vector<Slot> g_slots;
const void* g_root = nullptr; // the GUI the slot pointers belong to
bool g_internal = false;      // our own Show/Hide calls

Slot* Find(const void* item)
{
    for (Slot& s : g_slots)
        if (s.item == item)
            return &s;
    return nullptr;
}

// After a GUI reload the old items are gone (and their addresses may be reused): forget them.
bool CheckRoot()
{
    void* root = game::GuiRoot();
    if (root != g_root) {
        g_slots.clear();
        g_root = root;
    }
    return !g_slots.empty();
}

void SetShown(const void* item, bool show)
{
    g_internal = true;
    gui::ShowIf(const_cast<void*>(item), show);
    g_internal = false;
}

// FNV-1a, the game's HashedString hash
unsigned Hash(const char* s)
{
    unsigned h = 0x811c9dc5;
    for (; s && *s; s++)
        h = (h ^ static_cast<unsigned char>(*s)) * 0x1000193;
    return h;
}

// The trooper may use the template (0x140112B40): bound by name or by type, to its class or its unit.
bool Available(const void* library, const void* tmpl, const void* trooper)
{
    const void* name = &game::Field<char>(tmpl, game::off.eqpName);
    const void* cls = &game::Field<char>(trooper, game::off.trooperClass);
    const void* unit = &game::Field<char>(trooper, game::off.trooperUnit);
    if (game::fn.isEquipmentBoundTo(library, name, cls) || game::fn.isEquipmentBoundTo(library, name, unit))
        return true;
    // a HashedString with only the type name's hash, as the game builds it
    char typeKey[64] = {};
    const char* typeName = game::fn.equipmentTypeName(game::Field<int>(tmpl, game::off.eqpType));
    game::Field<unsigned>(typeKey, game::off.hashedStringHash) = typeName && *typeName ? Hash(typeName) : 0;
    return game::fn.isEquipmentBoundTo(library, typeKey, cls) || game::fn.isEquipmentBoundTo(library, typeKey, unit);
}

bool GameManaged(int64_t slot)
{
    for (int64_t managed : game::off.invGameManaged)
        if (slot == managed)
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
    bool partial = c.type >= game::off.invUtility1 && c.type <= game::off.invSupport3;
    return partial ? strstr(c.name, binding) != nullptr : strcmp(c.name, binding) == 0;
}

} // namespace

void Slots_Refresh()
{
    if (!g_settings.emptySlots)
        return;
    void* gameGUI = game::GameGUI();
    const void* library = game::ObjectLibrary();
    void* screen = gameGUI ? game::Field<void*>(gameGUI, game::off.customizationScreen) : nullptr;
    const void* trooper = gameGUI ? game::Field<void*>(gameGUI, game::off.customizeSelection) : nullptr;
    if (!screen || gui::IsHidden(screen) || !trooper || !library)
        return; // Customize_OnTrooperSelected does nothing while the screen is hidden; neither do we
    CheckRoot();

    // the slots on this screen, each with options already if the trooper has something (not EMPTY) in it
    std::vector<Candidate> slots;
    for (int64_t type = 1; type < game::off.invCount; type++) {
        if (GameManaged(type))
            continue;
        const char* name = game::fn.inventoryName(static_cast<int>(type));
        void* item = name && *name ? gui::Find(screen, name) : nullptr;
        if (!item)
            continue; // eg. no Custom slots without Trooper Toolbox
        const char* equipped = &game::Field<char>(trooper, game::off.trooperInventory + static_cast<int32_t>(type * game::off.staticStringSize));
        bool has = false;
        if (game::Field<unsigned>(equipped, game::off.staticStringHash) != 0) {
            const void* tmpl = game::fn.getEquipment(library, equipped + game::off.staticStringText);
            has = tmpl && game::Field<int>(tmpl, game::off.eqpType) != game::off.eqpEmpty;
        }
        slots.push_back({type, name, item, has});
    }

    // one pass over every template, checking the trooper's binds only for templates that would fill a slot still
    // without options
    const char* map = &game::Field<char>(library, game::off.libraryTemplates);
    const void* head = game::Field<void*>(map, kMapListHead);
    const void* node = head ? game::Field<void*>(head, kNodeNext) : nullptr;
    for (int guard = 0; node && node != head && guard < 1000000; guard++, node = game::Field<void*>(node, kNodeNext)) {
        const void* tmpl = game::Field<void*>(node, kNodeValue);
        if (!tmpl || game::Field<int>(tmpl, game::off.eqpType) == game::off.eqpEmpty || !game::Field<const char*>(tmpl, game::off.eqpTooltip))
            continue;
        const char* binding = game::Field<const char*>(tmpl, game::off.eqpBinding);
        if (!binding || !*binding)
            continue;
        bool wanted = false;
        for (const Candidate& c : slots)
            wanted |= !c.hasOptions && Matches(c, binding);
        if (!wanted || !Available(library, tmpl, trooper))
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
    if (game::GuiRoot() == g_root)
        for (const Slot& s : g_slots)
            if (s.suppressed && s.wantShown)
                SetShown(s.item, true);
    g_slots.clear();
    TtMerge_Arrange();
}

bool Slots_IsInternal()
{
    return g_internal;
}
