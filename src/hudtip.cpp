// The trooper hover tooltip. UpdateHumanTooltip fills HUD_Human_Tooltip (in missions, from
// GameGUI::UpdateEntityTooltip, and on the deploy screen) from the trooper's equipment. Its equipment filler
// (0x1400E8DE0 in build 112) puts weapons into #InvWeapon0/1 and the utility pouches and Support Gear 1 into the
// #Inv_Utility circles; every other slot is shown only if the tooltip has an item named after it
// (FindChild(root, Inventory::GetNameForEnum(slot))): the game hides that item, sets its #Icon and shows it when the
// slot is filled. Stock hud.xml has no SupportGear2/SupportGear3 items, so lockpicks, crowbars and halligans never
// show. gui/operator_tooltip.xml has them (a port of the HUD Support Gear mod, which replaced hud.xml), and this moves
// them into whichever hud.xml is loaded (stock, Deluke UI, Tacticool UI: same tooltip structure) before the game
// fills the tooltip.
//
// Layout: the row hangs below #InvArmorHelmet in a plain wrapper (the game lines up slot-named direct children of that
// panel 160 px apart, which only fits two). At the end of UpdateHumanTooltip the visible sections are chained by
// writing each one's m_parent to the previous visible one, and each sits at its fixed XML origin below its "parent",
// so the section after the panel (m_parent == #InvArmorHelmet) is moved down by the row's height while the row shows.
#include "hudtip.h"

#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "game.h"
#include "gui.h"
#include "settings.h"

namespace {

constexpr const char* kRow = "#dk2ml_tt_gear";
constexpr const char* kName = "#dk2ml_tt_name";
constexpr const char* kPanel = "#InvArmorHelmet";
constexpr const char* kSlots[] = {"SupportGear2", "SupportGear3"}; // Inventory::GetNameForEnum names
constexpr float kIconBox = 80;     // fits the 88 px circle
constexpr float kIconMaxScale = 0.45f;

struct Cell {
    int64_t slot = -1; // Inventory::eType
    void* item = nullptr;
    void* label = nullptr;
    void* icon = nullptr;
    const void* shownTemplate = nullptr; // what the label shows (set only when it changes)
};

// A section we moved down, and by how much (0 once moved back).
struct Shift {
    void* item;
    float applied;
};

const void* g_root = nullptr; // GuiRoot() our pointers belong to
void* g_row = nullptr;
float g_rowHeight = 0;
Cell g_cells[std::size(kSlots)];
gui::TextFit g_fit;
std::vector<Shift> g_shifts;
void* g_sections = nullptr; // the item whose children are the tooltip's sections (#NameHeader in stock hud.xml)
void* g_sectionsPanel = nullptr;
unsigned g_color = 0; // the tooltip's own text color, copied to our labels
bool g_logged = false;

// (Re)finds our items; after a GUI reload the old ones are gone.
bool Ready()
{
    void* guiRoot = game::GuiRoot();
    if (guiRoot != g_root) {
        g_root = guiRoot;
        g_row = gui::Find(guiRoot, kRow);
        g_rowHeight = g_row ? static_cast<float>(game::Field<int>(g_row, game::off.itemSize + static_cast<int32_t>(sizeof(int)))) : 0;
        g_shifts.clear();
        g_sections = nullptr;
        g_sectionsPanel = nullptr;
        g_fit = {};
        g_color = 0;
        for (size_t i = 0; i < std::size(kSlots); i++) {
            Cell& c = g_cells[i];
            c = {};
            c.item = gui::Find(g_row, kSlots[i]);
            c.label = gui::Find(c.item, kName);
            c.icon = gui::Find(c.item, "#Icon");
            for (int64_t s = 1; s < game::off.invCount; s++) {
                const char* name = game::fn.inventoryName(static_cast<int>(s));
                if (name && strcmp(name, kSlots[i]) == 0)
                    c.slot = s;
            }
        }
        if (!g_row && !g_logged) {
            game::api->Log("gui/operator_tooltip.xml: %s not found; no support gear row in the tooltip", kRow);
            g_logged = true;
        }
    }
    return g_row != nullptr;
}

const char* NameOf(const void* tmpl)
{
    const char* id = game::Field<const char*>(tmpl, game::off.eqpTooltip);
    const char* text = id && *id ? game::fn.getTextForId(id, true, false) : nullptr;
    if (text && *text)
        return text;
    const char* name = game::Field<const char*>(tmpl, game::off.eqpName + game::off.hashedStringText);
    return name ? name : "";
}

// The item whose child list holds `child`. Sections are re-parented by the game (m_parent), but the child lists stay
// as loaded, so this finds the XML container of the panel and its sibling sections.
void* ContainerOf(void* from, const void* child, int depth = 0)
{
    if (!from || depth > 16)
        return nullptr;
    std::vector<void*> children = gui::Children(from);
    for (void* c : children)
        if (c == child)
            return from;
    for (void* c : children)
        if (void* found = ContainerOf(c, child, depth + 1))
            return found;
    return nullptr;
}

// Moves an aligned section vertically the way the game moves aligned items (the #InvArmorHelmet row layout,
// 0x1400EA0DD): the same delta on m_alignOffset and m_localOrigin. The SetOrigin action would write both to the
// given value instead, and for an align="lb" section the two don't mean the same thing.
void ShiftBy(void* item, float dy)
{
    game::Field<float>(item, game::off.itemAlignOffset + static_cast<int32_t>(sizeof(float))) += dy;
    game::Field<float>(item, game::off.itemLocalOrigin + static_cast<int32_t>(sizeof(float))) += dy;
}

} // namespace

void HudTip_Before(void* root)
{
    if (!root || !g_settings.tooltip || !Ready()) // switched off: the row isn't attached to new tooltips
        return;
    void* panel = gui::Find(root, kPanel);
    if (!panel)
        return;
    if (panel != g_sectionsPanel) { // first time, or another tooltip (the deploy screen may use its own)
        g_sections = ContainerOf(root, panel);
        g_sectionsPanel = panel;
    }
    if (gui::Parent(g_row) == panel)
        return;
    gui::AddChild(panel, g_row); // the game's fill, right after this, finds the slot-named cells in it (shown or not)
    // the tooltip's own text color (cream in stock, white in Deluke UI): its weapon name
    g_color = gui::TextColor(gui::Find(gui::Find(root, "#InvWeapon0"), "#Name"));
    for (Cell& c : g_cells) {
        if (g_color)
            gui::SetTextColor(c.label, g_color);
        c.shownTemplate = nullptr;
    }
}

void HudTip_After(void* root, const void* equipment)
{
    if (!root || !g_row || gui::Parent(g_row) != gui::Find(root, kPanel))
        return;
    void* panel = gui::Parent(g_row);

    if (!g_settings.tooltip) {
        // switched off: the row is still in this tooltip (the game fills its cells), so hide it and put back what
        // we moved
        gui::ShowIf(g_row, false);
        for (Shift& s : g_shifts) {
            if (s.applied != 0)
                ShiftBy(s.item, -s.applied);
            s.applied = 0;
        }
        return;
    }

    bool anyShown = false;
    for (Cell& c : g_cells) {
        if (!c.item || gui::IsHidden(c.item))
            continue;
        anyShown = true;
        const void* tmpl = equipment && c.slot > 0
                               ? game::Field<const void*>(equipment, static_cast<int32_t>(c.slot * game::off.equipmentClientSize) + game::off.equipmentClientItem)
                               : nullptr;
        if (tmpl && tmpl != c.shownTemplate) {
            gui::SetTextFitted(c.label, NameOf(tmpl), g_fit);
            if (g_color)
                gui::SetTextColor(c.label, g_color);
            c.shownTemplate = tmpl;
        }
        gui::ShowIf(c.label, tmpl != nullptr);
        // the game set the icon texture and kept the XML scale; modded icons come in any size
        gui::FitImage(c.icon, kIconBox, kIconBox, kIconMaxScale);
    }

    // the row's full-width background: only while one of its slots is filled
    gui::ShowIf(g_row, anyShown);

    // make room: the section the game chained under the panel moves down while the row shows
    bool rowShown = anyShown && !gui::IsHidden(panel);
    std::vector<void*> followers;
    for (void* section : gui::Children(g_sections ? g_sections : root))
        if (section != g_row && gui::Parent(section) == panel)
            followers.push_back(section);
    for (void* f : followers) {
        bool known = false;
        for (const Shift& s : g_shifts)
            known |= s.item == f;
        if (!known)
            g_shifts.push_back({f, 0});
    }
    for (Shift& s : g_shifts) {
        bool follows = false;
        for (void* f : followers)
            follows |= f == s.item;
        float wanted = follows && rowShown ? -g_rowHeight : 0; // GUI space is +y up: negative is down
        if (wanted != s.applied) {
            ShiftBy(s.item, wanted - s.applied);
            s.applied = wanted;
        }
    }
}
