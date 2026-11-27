// Loadouts' data: saved profiles, each the whole loadout (every slot), 5 per operator and 5 per squad (team profiles,
// shared by every squadmate), kept in loadouts.ini in dk2ml's config folder for this plugin. The slots are split into
// GUNS, GEAR and LOOKS internally, for undo and the rows' summaries.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace loadouts {

constexpr int kCategories = 3; // GUNS, GEAR, LOOKS
enum { kGuns, kGear, kLooks };
constexpr int kAll = -1;       // every category, for Read / Write
constexpr int kProfiles = 5;   // profiles per operator, and per squad

// A category's label ("GUNS", "GEAR", "LOOKS").
const char* CategoryLabel(int category);

// slot name (Inventory::GetNameForEnum, eg. "PrimaryWeapon") -> equipped template name ("" = the slot's Empty choice)
using Items = std::vector<std::pair<std::string, std::string>>;

struct Profile {
    bool used = false; // saved at least once
    Items items;
    std::string name;  // given by the player ("" = none), kept when the profile is saved over
};

// An operator's profiles, or a squad's (team profiles).
struct Set {
    Profile profiles[kProfiles];
    std::vector<std::string> locked; // an operator's locked slots (main slots' names; their attachments go with them)
};

// Reads loadouts.ini. Call once after game::Resolve: it also looks up which slots each category holds (logged).
void Load();
// Rewrites loadouts.ini.
void Save();
// The operator's own profiles (created empty, not saved until something is), or null if the trooper can't be told
// apart (no squad or no name).
Set* For(const void* trooper);
// The profiles of the trooper's squad, or null without a squad.
Set* TeamFor(const void* trooper);
// The trooper's squad's name ("" without one).
std::string SquadName(const void* trooper);
// The key the operator's profiles are kept under ("" if it can't be told apart: no squad or no name).
std::string KeyOf(const void* trooper);
// A template's shown name (its tooltip's text, as the game's lists show it); "" for an EMPTY item, the name plus
// " (missing)" for one that's gone.
std::string ItemName(const std::string& templateName);
// What the trooper has equipped in the category's slots (kAll: every slot).
Items Read(const void* trooper, int category);
// The trooper's class starting loadout (its human template's <Equipment> list, put in slots the game's way), every
// slot: "" where it has nothing, so loading it empties those. {} if unknown (bindings off, no template, nothing listed).
Items StartingLoadout(const void* trooper);

struct WriteResult {
    int changed = 0; // slots that now hold something else
    int skipped = 0; // items the trooper can't use (template gone, not bound to its class or unit), left as they were
    int locked = 0;  // locked slots that would have changed, left as they were
};
// Equips items in the category's slots (kAll: every slot), as Paste loadout does (the name and its hash in
// Roster::Trooper::inventory). A weapon changes together with its attachments, a helmet with its NVG, or not at all.
// Slots the operator locked (with their attachments) are left alone. Doesn't save or refresh anything.
WriteResult Write(void* trooper, int category, const Items& items);
// Whether everything in saved is what current has equipped, leaving out the operator's locked slots.
bool Same(const void* trooper, const Items& current, const Items& saved);

// The slots that can be locked: the main ones (a weapon's attachments and the helmet's NVG go with them), by
// Inventory::GetNameForEnum name.
std::vector<std::string> LockableSlots();
bool IsLocked(const void* trooper, const std::string& slot);
// Locks or unlocks the slot for this operator (not saved: call Save). Returns the new state.
bool ToggleLock(const void* trooper, const std::string& slot);
// The category's key items' shown names in items, eg. "HK416D 10.5 Suppressed / Glock 19" (GUNS: weapons, GEAR:
// pouches and support gear, LOOKS: armor and helmet). Items may hold other categories' slots too.
std::string Summary(int category, const Items& items);

// One saved item as the hover pop-up shows it.
struct Shown {
    std::string name;            // its shown name (ItemName)
    const char* image = nullptr; // its list icon (Equipment_Template::guiImage), null for a template that's gone
    std::string attachments;     // what goes with it, shown names joined by ", " (scope / ammo / muzzle; a helmet's NVG)
};
// The category's saved main slots (attachments and NVG folded into their weapon / helmet), in slot order, leaving out
// empty ones: GUNS primary, secondary, mounted gun; GEAR pouches, support gear; LOOKS helmet, armor, Custom1-9.
std::vector<Shown> Contents(int category, const Items& items);

} // namespace loadouts
