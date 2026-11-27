// Settings: one switch per feature (plus a few choices), saved as key=value lines in settings.ini in dk2ml's config folder for this plugin
// (%LOCALAPPDATA%\KillHouseGames\DoorKickers2\dk2ml\dk2_eras_ui_improvements\), which survives mod updates.
//
// They're edited on this mod's page of the mod loader's Native mods screen (main menu > Native mods), registered with
// DK2ML_API::AddOption like the Free Camera mod's options. The loader keeps a pointer to each value (a plain
// bool here), reads it when the screen opens, writes it when the player clicks the checkbox and then calls onChange;
// onChange copies it into g_settings (the flags the hooks and the frame tick read), applies the switch at once and
// saves. Options must be added during DK2ML_PluginInit: the loader builds the screen when the GUI loads.
#include "core/settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

#include "campaigns/cards.h"
#include "core/game.h"
#include "equipment/grid.h"
#include "loadouts/panel.h"
#include "portraits/picker.h"
#include "slots/concealment.h"
#include "slots/slots.h"
#include "slots/ttmerge.h"

Settings g_settings;

namespace {

struct Switch {
    const char* key;     // in settings.ini
    const char* label;   // on the Native mods page
    const char* tooltip;
    std::atomic<bool>* flag;
    void (*apply)(bool on); // optional: what to do right away (the other flags are checked where they're used)
    bool value;             // what the loader's checkbox points at
};

// in the order of the Native mods page; Settings_Register puts a header before each group's first switch
Switch g_switches[] = {
    {"equipment_grid", "Equipment grid", "Pick weapons and gear from a grouped icon grid instead of the one-row list.",
     &g_settings.grid, Grid_SetEnabled},
    {"sort_by_name", "Sort guns (by caliber, then name)",
     "In the equipment grid, list guns by caliber (smallest first), then alphabetically. Other gear keeps the game's order, as it does with this off.",
     &g_settings.sortByName, nullptr}, // read when a list is built
    {"hide_empty_slots", "Hide empty inventory slots", "Hide slots the trooper has nothing for, including Trooper Toolbox slots.",
     &g_settings.emptySlots, Slots_SetEnabled},
    {"hide_concealment", "Hide concealment for operators who can't conceal",
     "Hide the Concealment bar in the equipment stats for classes without concealment (everyone but the CIA's undercover and black ops), where it always reads OVERT.",
     &g_settings.hideConcealment, Concealment_SetEnabled},
    {"esc_closes_lists", "Esc closes open lists first", "Esc / Back close an open equipment list before leaving the Customization screen.",
     &g_settings.backKey, nullptr},
    {"portrait_picker", "Portrait picker and fixed portrait arrows", "The grid button on the trooper's portrait, and PREV/NEXT arrows that don't get stuck.",
     &g_settings.portraits, Picker_SetEnabled},
    {"squad_usage", "Mark gear used by squadmates", "Dim items someone else in the squad has equipped; hovering one shows who.",
     &g_settings.squadUsage, nullptr}, // read when a list is built
    {"tt_main_page", "Trooper Toolbox: all slots on one page",
     "Move the slots from Trooper Toolbox's second page (Custom 7-9, Support Gear 2/3) onto its main page, and remove its More Gear and Cosmetics button. Its credits move behind a CREDITS button, and its background is redrawn so hidden slots leave no empty frame.",
     &g_settings.ttMerge, TtMerge_SetEnabled},
    {"loadout_tabs", "Loadouts panel (yours and your squad's)",
     "A panel left of the Customization screen with saved loadouts, each holding everything (guns, gear, helmet/armor, cosmetics): five per operator, and five team loadouts per squad that every squadmate can load.",
     &g_settings.loadoutTabs, Loadouts_SetEnabled},
    {"tooltip_gear", "Breaching gear in the trooper tooltip", "Show lockpicks, crowbars and halligans (Support Gear 2/3) when hovering a trooper.",
     &g_settings.tooltip, nullptr},
    {"one_page_wheel", "Action wheel: all actions on one page",
     "Right-clicking an operator shows the target's actions (doors, windows, enemies, hostages) on an outer ring around the path actions (reload, sprint, grenades, go-codes), instead of two pages with a button between them.",
     &g_settings.wheel, nullptr}, // read when the wheel opens
    {"campaign_cards", "Squads that completed each campaign",
     "A badge on each campaign card on the campaign selection screen; hovering the card lists the squads that finished it.",
     &g_settings.campaignCards, Campaigns_SetEnabled},
    {"custom_cursor", "Custom mouse cursor", "The mod's own cursor, sized to the screen, instead of the Windows arrow.",
     &g_settings.cursor, nullptr},
};

// A left/right selector on the Native mods page; the loader points at `index`, settings.ini stores the value.
struct Choice {
    const char* key;
    const char* label;
    const char* tooltip;
    const char* const* names; // what the selector shows, one per value
    int first, count;         // values first .. first + count - 1, index 0 .. count - 1
    std::atomic<int>* setting;
    int index;
};

const char* const kWeaponColumns[] = {"2 (with names)", "3", "4"};

// added right after the equipment_grid switch (Settings_Register)
Choice g_choices[] = {
    {"weapon_columns", "Weapons per row",
     "Weapon lists: 2 per row with names, or 3 / 4 per row as icons only (hover an icon for its name and details).",
     kWeaponColumns, 2, 3, &g_settings.weaponColumns},
};

std::wstring Path()
{
    const wchar_t* dir = game::api->GetConfigDir(); // "" if the loader has no config folder
    return dir && *dir ? std::wstring(dir) + L"settings.ini" : std::wstring();
}

void Save()
{
    std::wstring path = Path();
    if (path.empty())
        return;
    FILE* f = _wfopen(path.c_str(), L"w");
    if (!f) {
        game::api->Log("settings: couldn't write %ls", path.c_str());
        return;
    }
    fprintf(f, "; Era's UI Improvements: 1 = on, 0 = off (also on the main menu's Native mods screen)\n");
    for (const Switch& s : g_switches)
        fprintf(f, "%s=%d\n", s.key, s.flag->load() ? 1 : 0);
    for (const Choice& c : g_choices)
        fprintf(f, "%s=%d\n", c.key, c.setting->load());
    fclose(f);
}

void Set(Switch& s, bool on)
{
    s.value = on;
    if (s.flag->exchange(on) == on)
        return;
    if (s.apply)
        s.apply(on);
    game::api->Log("settings: %s %s", s.key, on ? "on" : "off");
}

// a checkbox changed: the loader already wrote s.value
void OnChange(const DK2ML_Option*, void* user)
{
    auto* s = static_cast<Switch*>(user);
    Set(*s, s->value);
    Save();
}

// a selector changed: the loader already wrote c.index
void OnChoice(const DK2ML_Option*, void* user)
{
    auto* c = static_cast<Choice*>(user);
    int value = c->first + std::clamp(c->index, 0, c->count - 1);
    c->setting->store(value); // read when the next list is built
    game::api->Log("settings: %s %d", c->key, value);
    Save();
}

void OnAllOn(const DK2ML_Option*, void*)
{
    for (Switch& s : g_switches)
        Set(s, true);
    Save(); // the loader re-reads the page after a button
}

DK2ML_Option Option(DK2ML_OptionType type, const char* label, const char* tooltip)
{
    DK2ML_Option o{};
    o.structSize = sizeof(DK2ML_Option);
    o.type = type;
    o.label = label;
    o.tooltip = tooltip;
    return o;
}

void Add(const DK2ML_API* api, const DK2ML_Option& o)
{
    if (api->AddOption(&o) != DK2ML_OK)
        game::api->Log("settings: the loader rejected option \"%s\"", o.label);
}

void AddChoice(const DK2ML_API* api, Choice& c)
{
    DK2ML_Option o = Option(DK2ML_OPTION_CHOICE, c.label, c.tooltip);
    o.value = &c.index;
    o.choices = c.names;
    o.choiceCount = c.count;
    o.onChange = OnChoice;
    o.user = &c;
    Add(api, o);
}

void AddSwitch(const DK2ML_API* api, Switch& s)
{
    DK2ML_Option o = Option(DK2ML_OPTION_BOOL, s.label, s.tooltip);
    o.value = &s.value;
    o.onChange = OnChange;
    o.user = &s;
    Add(api, o);
}

} // namespace

void Settings_Load()
{
    std::wstring path = Path();
    FILE* f = path.empty() ? nullptr : _wfopen(path.c_str(), L"r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char key[128];
            int value;
            if (sscanf(line, " %127[^= ] = %d", key, &value) != 2)
                continue;
            for (Switch& s : g_switches)
                if (strcmp(key, s.key) == 0)
                    s.flag->store(value != 0);
            for (Choice& c : g_choices)
                if (strcmp(key, c.key) == 0)
                    c.setting->store(value);
        }
        fclose(f);
    }
    std::string off;
    for (Switch& s : g_switches) {
        s.value = s.flag->load();
        if (!s.value)
            off += std::string(off.empty() ? "" : ", ") + s.key;
    }
    std::string choices;
    for (Choice& c : g_choices) {
        int value = std::clamp(c.setting->load(), c.first, c.first + c.count - 1);
        c.setting->store(value);
        c.index = value - c.first;
        choices += std::string(", ") + c.key + "=" + std::to_string(value);
    }
    game::api->Log("settings: %s%s%s", off.empty() ? "all on" : "off: ", off.c_str(), choices.c_str());
}

void Settings_Register(const DK2ML_API* api)
{
    struct Group {
        const char* header;
        const char* firstKey;
    };
    const Group groups[] = {{"Customization screen", "equipment_grid"},
                            {"Missions", "tooltip_gear"},
                            {"Campaign selection", "campaign_cards"},
                            {"General", "custom_cursor"}};
    for (Switch& s : g_switches) {
        for (const Group& g : groups)
            if (strcmp(s.key, g.firstKey) == 0)
                Add(api, Option(DK2ML_OPTION_HEADER, g.header, nullptr));
        AddSwitch(api, s);
        if (strcmp(s.key, "equipment_grid") == 0)
            for (Choice& c : g_choices)
                AddChoice(api, c);
    }
    DK2ML_Option all = Option(DK2ML_OPTION_BUTTON, "Turn everything on", "Switch every part of the mod back on.");
    all.onChange = OnAllOn;
    Add(api, all);
}
