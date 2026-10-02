// Settings: one switch per feature, saved as key=0/1 lines in settings.ini in dk2ml's config folder for this plugin
// (%LOCALAPPDATA%\KillHouseGames\DoorKickers2\dk2ml\dk2_eras_ui_improvements\), which survives mod updates.
//
// They're edited on this mod's page of the mod loader's Native mods screen (main menu > Native mods), registered with
// DK2ML_API::AddOption (API v5) like the Free Camera mod's options. The loader keeps a pointer to each value (a plain
// bool here), reads it when the screen opens, writes it when the player clicks the checkbox and then calls onChange;
// onChange copies it into g_settings (the flags the hooks and the cursor thread read), applies the switch at once and
// saves. Options must be added during DK2ML_PluginInit: the loader builds the screen when the GUI loads.
#include "settings.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

#include "game.h"
#include "grid.h"
#include "picker.h"
#include "slots.h"
#include "ttmerge.h"

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

Switch g_switches[] = {
    {"equipment_grid", "Equipment grid", "Pick weapons and gear from a grouped icon grid instead of the one-row list.",
     &g_settings.grid, Grid_SetEnabled},
    {"hide_empty_slots", "Hide empty inventory slots", "Hide slots the trooper has nothing for, including Trooper Toolbox slots.",
     &g_settings.emptySlots, Slots_SetEnabled},
    {"esc_closes_lists", "Esc closes open lists first", "Esc / Back close an open equipment list before leaving the Customization screen.",
     &g_settings.backKey, nullptr},
    {"portrait_picker", "Portrait picker and fixed portrait arrows", "The grid button on the trooper's portrait, and PREV/NEXT arrows that don't get stuck.",
     &g_settings.portraits, Picker_SetEnabled},
    {"squad_usage", "Mark gear used by squadmates", "Dim items someone else in the squad has equipped; hovering one shows who.",
     &g_settings.squadUsage, nullptr}, // read when a list is built
    {"tt_main_page", "Trooper Toolbox: all slots on one page",
     "Move the slots from Trooper Toolbox's second page (Custom 7-9, Support Gear 2/3) onto its main page, and remove its More Gear and Cosmetics button.",
     &g_settings.ttMerge, TtMerge_SetEnabled},
    {"tooltip_gear", "Breaching gear in the trooper tooltip", "Show lockpicks, crowbars and halligans (Support Gear 2/3) when hovering a trooper.",
     &g_settings.tooltip, nullptr},
    {"custom_cursor", "Custom mouse cursor", "The mod's own cursor, sized to the screen, instead of the Windows arrow.",
     &g_settings.cursor, nullptr},
};

std::wstring Path()
{
    const wchar_t* dir = game::api->GetConfigDir ? game::api->GetConfigDir() : nullptr;
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
        }
        fclose(f);
    }
    std::string off;
    for (Switch& s : g_switches) {
        s.value = s.flag->load();
        if (!s.value)
            off += std::string(off.empty() ? "" : ", ") + s.key;
    }
    game::api->Log("settings: %s%s", off.empty() ? "all on" : "off: ", off.c_str());
}

void Settings_Register(const DK2ML_API* api)
{
    Add(api, Option(DK2ML_OPTION_HEADER, "Customization screen", nullptr));
    for (int i = 0; i < 6; i++)
        AddSwitch(api, g_switches[i]);
    Add(api, Option(DK2ML_OPTION_HEADER, "Missions", nullptr));
    AddSwitch(api, g_switches[6]);
    Add(api, Option(DK2ML_OPTION_HEADER, "General", nullptr));
    AddSwitch(api, g_switches[7]);
    DK2ML_Option all = Option(DK2ML_OPTION_BUTTON, "Turn everything on", "Switch every part of the mod back on.");
    all.onChange = OnAllOn;
    Add(api, all);
}
