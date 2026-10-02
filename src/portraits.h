// The portrait pool: ObjectLibrary::m_humanPortraits, every <Portrait> from data/units/*human_identities*.xml of the
// game and of enabled mods, merged at startup.
#pragma once

#include <string>
#include <vector>

namespace portraits {

enum class UnitFilter { ThisUnit, All };

struct Entry {
    const void* portrait; // sHumanPortrait in the library
    const char* tex;      // its texture path, eg. data/textures/portraits/ranger10.dds
    int gender;
    unsigned unit;        // hash of the unit name (StaticString<48>::m_hash)
};

// A run of entries from the same unit. title is the unit's localized name (empty: no header).
struct Group {
    std::string title;
    size_t first, count;
};

struct Filtered {
    std::vector<Entry> entries; // grouped as in groups
    std::vector<Group> groups;
};

// The pool filtered for a trooper, one entry per texture. Empty if the pool looks wrong (logged).
// ThisUnit matches the stock PREV/NEXT arrows (same unit, and same class when the portrait names one): one group
// without a title. All: grouped by unit, the trooper's own unit first, then the other units by name, then "OTHER
// PORTRAITS" (mod pools whose unit isn't a real unit). Within a group, by file name in natural order (ranger2 before
// ranger10).
Filtered ForTrooper(const void* trooper, UnitFilter unit);

// The same, keeping only the entries keep() accepts (group titles kept, empty groups dropped).
template <typename Pred>
Filtered Without(const Filtered& in, Pred keep)
{
    Filtered out;
    for (const Group& g : in.groups) {
        Group kept{g.title, out.entries.size(), 0};
        for (size_t i = g.first; i < g.first + g.count; i++) {
            if (keep(in.entries[i])) {
                out.entries.push_back(in.entries[i]);
                kept.count++;
            }
        }
        if (kept.count)
            out.groups.push_back(kept);
    }
    return out;
}

const char* TrooperTex(const void* trooper);
const char* TrooperName(const void* trooper);

// The portraits every other trooper in the roster wears (not the trooper's own), squadmates flagged. Empty if the
// roster can't be read (logged).
struct User {
    const char* tex;
    const char* name;
    const char* squad; // the squad's name
    bool sameSquad;    // in the trooper's own squad
};
std::vector<User> UsedInRoster(const void* trooper);
// The other troopers of the trooper's own squad (Roster::Trooper*), validated like UsedInRoster.
std::vector<const void*> Squadmates(const void* trooper);

// Gives the trooper this portrait (texture + gender, as the stock arrows do). Doesn't save or refresh anything.
void Assign(void* trooper, const Entry& e);

} // namespace portraits
