// The Concealment bar in the Customization screen's EQUIPMENT stats only means something for classes the game gives a
// base concealment: Human_Template::concealment, from <Concealment default="..."/> in
// data/entities/humans_goodguys*.xml (the CIA's undercover 10 and black ops 7). Every other class (Rangers, SWAT,
// Nowheraki, militia, allies) has none (no element, or default="0"; the constructor's default is 0), and the bar always
// reads OVERT, empty. For those the section is hidden.
//
// The game's stats refresh (0x1401157C0 in build 112: Customize_OnTrooperSelected, an equip, Paste loadout) takes the
// template from Human_GetTemplateForUnitClassGender(unit, class, gender) and fills #concealment_stats and the
// OVERT / SUSPICIOUS / COVERT labels under #player_equipment_related_stats_parent; it never shows or hides the section
// around them (the parent of #concealment_stats), so it's set once per trooper select. Trooper Toolbox has two such
// sections under that parent (#player_equipment_stats_container, and _equipmentview shown in its equipment view), so
// every #concealment_stats there counts.
#include "slots/concealment.h"

#include <cstring>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"

namespace {

std::vector<void*> g_sections; // the parents of each #concealment_stats: background, title, labels and bar
bool g_looked = false;         // looked them up for this GUI

void* Screen()
{
    void* gameGUI = game::GameGUI();
    return gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizationScreen.Offset()) : nullptr;
}

// every item named name under item (any depth)
void FindAll(void* item, const char* name, std::vector<void*>& out, int depth = 0)
{
    if (!item || depth > 16)
        return;
    for (void* child : gui::Children(item)) {
        if (strcmp(gui::Name(child), name) == 0)
            out.push_back(child);
        FindAll(child, name, out, depth + 1);
    }
}

// The trooper's class has a base concealment (unknown: assume it does, so nothing is hidden by mistake).
bool CanConceal(const void* trooper)
{
    const void* unit = &dk2ml::At<char>(trooper, game::Roster_Trooper_unit.Offset());
    const void* cls = &dk2ml::At<char>(trooper, game::Roster_Trooper_className.Offset());
    int gender = dk2ml::At<int>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_gender.Offset());
    const void* tmpl = game::Human_GetTemplateForUnitClassGender(unit, cls, gender);
    return !tmpl || dk2ml::At<float>(tmpl, game::Human_Template_concealment.Offset()) > 0;
}

} // namespace

void Concealment_Refresh()
{
    if (!game::ConcealmentUsable())
        return;
    void* gameGUI = game::GameGUI();
    void* screen = Screen();
    const void* trooper = gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizeSelection.Offset()) : nullptr;
    if (!screen || gui::IsHidden(screen))
        return; // Customize_OnTrooperSelected does nothing while the screen is hidden; neither do we
    if (!g_looked) {
        g_looked = true;
        std::vector<void*> stats;
        FindAll(gui::Find(screen, "#player_equipment_related_stats_parent"), "#concealment_stats", stats);
        for (void* s : stats)
            if (void* section = gui::Parent(s))
                g_sections.push_back(section);
        game::api->Log("concealment: %d section(s)%s", static_cast<int>(g_sections.size()),
                       g_sections.empty() ? " (no #concealment_stats under #player_equipment_related_stats_parent, the bar stays)" : "");
    }
    bool show = !g_settings.hideConcealment || !trooper || CanConceal(trooper);
    for (void* section : g_sections)
        gui::ShowIf(section, show);
}

void Concealment_SetEnabled(bool)
{
    Concealment_Refresh(); // reads g_settings.hideConcealment
}

void Concealment_OnGuiLoaded()
{
    g_sections.clear();
    g_looked = false;
}
