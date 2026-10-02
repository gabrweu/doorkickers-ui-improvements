// Per-feature on/off switches, saved in dk2ml's config folder, shown as checkboxes on this mod's page of the mod
// loader's Native mods screen (main menu > Native mods > Era's UI Improvements > SETTINGS).
#pragma once

#include <atomic>

#include "dk2ml.h"

struct Settings {
    std::atomic<bool> grid{true};       // the equipment grid instead of the stock list
    std::atomic<bool> emptySlots{true}; // hide inventory slots with nothing to choose
    std::atomic<bool> backKey{true};    // Esc / Back close an open equipment list first
    std::atomic<bool> portraits{true};  // the portrait picker and the fixed PREV/NEXT arrows
    std::atomic<bool> squadUsage{true}; // mark equipment squadmates already have equipped
    std::atomic<bool> ttMerge{true};    // Trooper Toolbox's second-page slots on its main page
    std::atomic<bool> tooltip{true};    // the support gear row in the trooper hover tooltip
    std::atomic<bool> cursor{true};     // our mouse cursor (read by cursor.cpp's thread)
};

extern Settings g_settings;

// Reads settings.ini from dk2ml's config folder for this plugin (defaults: everything on).
void Settings_Load();
// Adds the switches to the loader's Native mods screen (DK2ML_API::AddOption, API v5). Call from DK2ML_PluginInit.
void Settings_Register(const DK2ML_API* api);
