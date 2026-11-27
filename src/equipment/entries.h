// What the equipment grid shows: the stock list's items, read back after the game built it, grouped per slot.
#pragma once

#include <string>
#include <vector>

namespace entries {

struct Entry {
    std::string id;           // the stock item's #id: the Equipment_Template name the game equips by
    std::string name;         // the stock item's #name (localized)
    const char* image;        // Equipment_Template::guiImage (lives in the ObjectLibrary), or null
    std::string category;     // Equipment_Template::category, lower case ("" for non-firearms)
    std::string caliber;      // EqpFirearm_Template::szCaliberInfo, a localization key, lower case ("" if not a firearm or unknown)
    std::string caliberName;  // that caliber as the game shows it (Loca::GetTextForId), eg. "5.56x45mm"; "" if none
    int rounds;               // <ModifiableParams roundsPerMagazine> (0 if not a firearm or unknown)
    float accuracy;           // min(accuracyStart, accuracyEnd) from <ModifiableParams> (0 if not a firearm or unknown)
    long long type;           // EquipmentType::eType
    bool equipped;            // the trooper has it on
    bool locked;              // needs unlocking first; the cost is in `cost`
    bool battleHonors;        // the cost is in battle honors, not stars
    std::string cost;
    std::vector<std::string> usedBy; // names of the squadmates who have it equipped (MarkSquadUse); empty if none
};

struct Group {
    std::string title;        // header text, eg. "RIFLES"
    size_t first, count;      // the group's entries: [first, first + count)
};

struct List {
    std::string binding;      // the slot, eg. "PrimaryWeapon" (Equipment_Template::inventoryBinding; "" for ammo)
    bool wide;                // mostly firearms (weapon and muzzle lists): wide icons, the 3-column layout
    std::vector<Entry> entries; // grouped: each group's entries are contiguous, in stock order
    std::vector<Group> groups;  // in order of first appearance in the stock list (which the game sorts by sortId)
};

// Reads the items the game just put into #equipment_list_parent, for any slot (stock, attachments, Trooper Toolbox's
// Custom slots), and groups them by item type. Returns false (stock list stays) when there's nothing readable.
bool FromStockList(void* stockList, List& out);

// Fills each entry's usedBy with the names of the other troopers in `trooper`'s own squad who have it equipped (in
// any inventory slot). Other squads don't count.
void MarkSquadUse(List& list, const void* trooper);

} // namespace entries
