// Per-feature on/off switches, saved in dk2ml's config folder, shown as checkboxes on this mod's page of the mod
// loader's Native mods screen (main menu > Native mods > Era's UI Improvements > SETTINGS).
#pragma once

#include <atomic>

#include "dk2ml.h"

struct Settings {
    std::atomic<bool> grid{true};       // the equipment grid instead of the stock list
    std::atomic<bool> sortByName{true}; // the grid's items alphabetical within each group (else the game's order)
    std::atomic<int> weaponColumns{2};  // cells per row in weapon lists: 2 (icon + name), 3 or 4 (icons only)
    std::atomic<bool> emptySlots{true}; // hide inventory slots with nothing to choose
    std::atomic<bool> hideConcealment{true}; // hide the Concealment bar for classes that can't conceal
    std::atomic<bool> backKey{true};    // Esc / Back close an open equipment list first
    std::atomic<bool> portraits{true};  // the portrait picker and the fixed PREV/NEXT arrows
    std::atomic<bool> squadUsage{true}; // mark equipment squadmates already have equipped
    std::atomic<bool> ttMerge{true};    // Trooper Toolbox's second-page slots on its main page
    std::atomic<bool> loadoutTabs{true}; // the loadouts panel (own and team GUNS / GEAR / LOOKS loadouts)
    std::atomic<bool> tooltip{true};    // the support gear row in the trooper hover tooltip
    std::atomic<bool> wheel{true};      // the mission action wheel's two pages on one wheel (read on each open)
    std::atomic<bool> campaignCards{true}; // the squads that completed each campaign, on the campaign selection cards
    std::atomic<bool> cursor{true};     // our mouse cursor (cursor.cpp, on the FRAME event)
};

extern Settings g_settings;

// Reads settings.ini from dk2ml's config folder for this plugin (defaults: everything on, 2 weapons per row).
void Settings_Load();
// Adds the switches to the loader's Native mods screen (DK2ML_API::AddOption). Call from DK2ML_PluginInit.
void Settings_Register(const DK2ML_API* api);
