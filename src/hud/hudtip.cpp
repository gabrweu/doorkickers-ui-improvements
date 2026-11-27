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
// #InvBuffs is re-placed by the game on every update while it shows a buff (0x1400EB4B5: its origin y written outright,
// -its text height), which drops our shift; so a section whose origin isn't what we left counts as unshifted.
#include "hud/hudtip.h"

#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"

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
    float left = 0; // its origin y right after our last shift
};

// The game re-placed the section from its own origin (#InvBuffs, every update): our shift is gone.
void CheckReset(Shift& s)
{
    if (s.applied != 0 && gui::OffsetY(s.item) != s.left)
        s.applied = 0;
}

void Apply(Shift& s, float wanted)
{
    CheckReset(s);
    if (wanted != s.applied) {
        gui::ShiftBy(s.item, wanted - s.applied);
        s.applied = wanted;
        s.left = gui::OffsetY(s.item);
    }
}

bool g_stale = true; // our items must be found (again): at first, and after every GUI reload (HudTip_OnGuiLoaded)
void* g_row = nullptr;
float g_rowHeight = 0;
Cell g_cells[std::size(kSlots)];
gui::TextFit g_fit;
std::vector<Shift> g_shifts;
void* g_sections = nullptr; // the item whose children are the tooltip's sections (#NameHeader in stock hud.xml)
void* g_sectionsPanel = nullptr;
unsigned g_color = 0; // the tooltip's own text color, copied to our labels
bool g_logged = false;

// (Re)finds our items: the first time, and after a GUI reload, which takes the old ones away.
bool Ready()
{
    if (g_stale) {
        void* guiRoot = game::GuiRoot();
        if (!guiRoot)
            return false; // no GUI yet: look again next time
        g_stale = false;
        g_row = gui::Find(guiRoot, kRow);
        g_rowHeight = g_row ? static_cast<float>(dk2ml::At<int>(g_row, game::GUI_Item_m_size.Offset() + static_cast<int32_t>(sizeof(int)))) : 0;
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
            for (int64_t s = 1; s < game::Inventory_NUM_VALUES.Get(); s++) {
                const char* name = game::Inventory_GetNameForEnum(static_cast<int>(s));
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
    const char* id = dk2ml::At<const char*>(tmpl, game::Equipment_Template_tooltip.Offset());
    const char* text = id && *id ? game::Loca_GetTextForId(id, true, false) : nullptr;
    if (text && *text)
        return text;
    const char* name = dk2ml::At<const char*>(tmpl, game::Equipment_Template_name.Offset() + game::HashedString_m_pString.Offset());
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
        for (Shift& s : g_shifts)
            Apply(s, 0);
        return;
    }

    bool anyShown = false;
    for (Cell& c : g_cells) {
        if (!c.item || gui::IsHidden(c.item))
            continue;
        anyShown = true;
        const void* tmpl = equipment && c.slot > 0
                               ? dk2ml::At<const void*>(equipment, static_cast<int32_t>(c.slot * game::sizeof_Equipment_Client.Get()) + game::Equipment_Client_pItem.Offset())
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
        Apply(s, follows && rowShown ? -g_rowHeight : 0); // GUI space is +y up: negative is down
    }
}

void HudTip_OnGuiLoaded()
{
    // our row and the tooltip's items went with the old GUI (their addresses may be reused): find them again
    g_stale = true;
    g_row = nullptr;
    g_sections = nullptr;
    g_sectionsPanel = nullptr;
    g_shifts.clear();
}
