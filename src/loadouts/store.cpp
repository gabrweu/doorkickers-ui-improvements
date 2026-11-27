// Loadouts' data. A trooper's equipped items are Roster::Trooper::inventory, a StaticString<48> per Inventory::eType
// slot (the template name, and its FNV-1a hash in m_hash, 0 when empty). The game's own Paste loadout
// (GameGUI::Customize_OnPasteLoadout, its per-slot copy at 0x140118FB0 in build 112) copies exactly that, name and
// hash: a weapon together with its scope/ammo/muzzle and the helmet together with its NVG, after checking the item is
// still bound to the target trooper (0x140112B40). Write does the same.
//
// A profile is the whole loadout: every slot of every category. The categories (GUNS / GEAR / LOOKS) are only used
// for undo (history.cpp) and the rows' summaries.
//
// There's no trooper or squad id in the roster, so an operator is known by its squad's name and its own name (plus
// its place in the squad when two share a name), and a squad by its name. Renaming starts afresh; the old entry stays
// in the file.
#include "loadouts/store.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>

#include "core/game.h"
#include "portraits/portraits.h"

namespace loadouts {

namespace {

const char* const kCategoryLabels[kCategories] = {"GUNS", "GEAR", "LOOKS"};

constexpr const char* kInventoryType = "Inventory::eType";

struct SlotDef {
    dk2ml::Enum value; // optional: a slot this build doesn't have is left out of its category
    int category;
    int parent;        // index in g_slots of the slot it goes with (its weapon / helmet), or -1
};

// Parents come before their attachments. The game's GUNS / GEAR / LOOKS split is ours: every slot that depends on
// another (attachments and suppressed "_Sup" twins on their weapon, NVG on the helmet) is in its category.
SlotDef g_slots[] = {
    {{kInventoryType, "PRIMARY_WEAPON", dk2ml::Optional}, kGuns, -1},            // 0
    {{kInventoryType, "PRIMARY_WEAPON_SCOPE", dk2ml::Optional}, kGuns, 0},
    {{kInventoryType, "PRIMARY_WEAPON_AMMO", dk2ml::Optional}, kGuns, 0},
    {{kInventoryType, "PRIMARY_WEAPON_MUZZLE", dk2ml::Optional}, kGuns, 0},
    {{kInventoryType, "SECONDARY_WEAPON", dk2ml::Optional}, kGuns, -1},          // 4
    {{kInventoryType, "SECONDARY_WEAPON_SCOPE", dk2ml::Optional}, kGuns, 4},
    {{kInventoryType, "SECONDARY_WEAPON_AMMO", dk2ml::Optional}, kGuns, 4},
    {{kInventoryType, "SECONDARY_WEAPON_MUZZLE", dk2ml::Optional}, kGuns, 4},
    {{kInventoryType, "MOUNTED_GUN", dk2ml::Optional}, kGuns, -1},
    {{kInventoryType, "UTILITY_POUCH1", dk2ml::Optional}, kGear, -1},            // 9
    {{kInventoryType, "UTILITY_POUCH2", dk2ml::Optional}, kGear, -1},
    {{kInventoryType, "SUPPORT_GEAR1", dk2ml::Optional}, kGear, -1},
    {{kInventoryType, "SUPPORT_GEAR2", dk2ml::Optional}, kGear, -1},
    {{kInventoryType, "SUPPORT_GEAR3", dk2ml::Optional}, kGear, -1},             // 13
    {{kInventoryType, "HELMET", dk2ml::Optional}, kLooks, -1},                   // 14
    {{kInventoryType, "HELMET_NVG", dk2ml::Optional}, kLooks, 14},
    {{kInventoryType, "ARMOR", dk2ml::Optional}, kLooks, -1},                    // 16
    {{kInventoryType, "CUSTOM1", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM2", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM3", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM4", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM5", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM6", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM7", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM8", dk2ml::Optional}, kLooks, -1},
    {{kInventoryType, "CUSTOM9", dk2ml::Optional}, kLooks, -1},
};
constexpr int kSlotCount = static_cast<int>(std::size(g_slots));

// what a summary names, per category (indices in g_slots), and how
struct SummaryDef {
    int slots[5];
    int count;
    const char* separator;
};
const SummaryDef kSummaries[kCategories] = {
    {{0, 4}, 2, " / "},              // primary / secondary
    {{9, 10, 11, 12, 13}, 5, ", "},  // pouches, support gear
    {{16, 14}, 2, " / "},            // armor / helmet
};

constexpr const char* kTeamPrefix = "team:";
constexpr const char* kProfileKey = "profile"; // a profile line's head: "profile3"
constexpr const char* kNameKey = "~name";       // a profile line's name pair (slot names never start with ~)

std::map<std::string, Set> g_sets; // operators by OperatorKey(), squads by kTeamPrefix + squad name

bool Usable(const SlotDef& s)
{
    return s.value.Resolved() && s.value.Get() > 0 && s.value.Get() < game::Inventory_NUM_VALUES.Get();
}

const char* SlotName(const SlotDef& s)
{
    const char* name = game::Inventory_GetNameForEnum(static_cast<int>(s.value.Get()));
    return name ? name : "";
}

// the slot's StaticString<48> in Roster::Trooper::inventory
const char* Equipped(const void* trooper, int64_t slot)
{
    return &dk2ml::At<char>(trooper, game::Roster_Trooper_inventory.Offset() + static_cast<int32_t>(slot * game::sizeof_StaticString48.Get()));
}

const char* SquadNameOf(const void* squad)
{
    return &dk2ml::At<char>(squad, game::Roster_Squad_name.Offset() + game::StaticString32_m_szString.Offset());
}

std::string OperatorKey(const void* trooper)
{
    const void* squad = game::Roster_GetSquadForTrooper(trooper);
    const char* name = portraits::TrooperName(trooper);
    if (!squad || !name[0])
        return {};
    std::string key = std::string(SquadNameOf(squad)) + "|" + name;

    // two troopers of the same name in the squad: told apart by their place in it (Squad::roster, an inline array)
    int count = dk2ml::At<int>(squad, game::Roster_Squad_numInRoster.Offset());
    int capacity = (game::Roster_Squad_diedInCampaign.Offset() - game::Roster_Squad_roster.Offset()) / static_cast<int>(game::sizeof_Roster_Trooper.Get());
    if (count < 0 || count > capacity)
        return key;
    const char* roster = &dk2ml::At<char>(squad, game::Roster_Squad_roster.Offset());
    int index = -1, namesakes = 0;
    for (int i = 0; i < count; i++) {
        const char* other = roster + i * game::sizeof_Roster_Trooper.Get();
        if (other == trooper)
            index = i;
        else if (strcmp(portraits::TrooperName(other), name) == 0)
            namesakes++;
    }
    if (namesakes && index >= 0)
        key += "#" + std::to_string(index + 1);
    return key;
}

std::wstring Path()
{
    const wchar_t* dir = game::api->GetConfigDir(); // "" if the loader has no config folder
    return dir && *dir ? std::wstring(dir) + L"loadouts.ini" : std::wstring();
}

bool InCategory(const SlotDef& s, int category)
{
    return category == kAll || s.category == category;
}

// "profile2<TAB>~name=Stealth<TAB>PrimaryWeapon=HK416<TAB>PrimaryWeaponScope=..." (1-based; the name optional). Lines
// with another head (the old per-category "guns2" / "gear2" / "looks2") are skipped, so the next Save drops them.
void ParseProfile(Set& set, const std::string& line)
{
    size_t end = line.find('\t');
    std::string head = line.substr(0, end);
    size_t digit = head.find_first_of("0123456789");
    if (digit == std::string::npos || head.compare(0, digit, kProfileKey) != 0)
        return;
    int index = atoi(head.c_str() + digit) - 1;
    if (index < 0 || index >= kProfiles)
        return;
    Profile& out = set.profiles[index];
    out = {};
    while (end != std::string::npos) {
        size_t start = end + 1;
        end = line.find('\t', start);
        std::string pair = line.substr(start, end == std::string::npos ? std::string::npos : end - start);
        size_t eq = pair.find('=');
        if (eq == std::string::npos || eq == 0)
            continue;
        if (pair.compare(0, eq, kNameKey) == 0)
            out.name = pair.substr(eq + 1);
        else
            out.items.emplace_back(pair.substr(0, eq), pair.substr(eq + 1));
    }
    out.used = !out.items.empty(); // a line with only a name: a named profile nothing was saved in
}

const std::string* Find(const Items& items, const char* slot)
{
    for (const auto& [s, item] : items)
        if (s == slot)
            return &item;
    return nullptr;
}

// the item's shown name (its tooltip's text, as the game's lists show it), "" for an EMPTY item
std::string ShownName(const std::string& name)
{
    const void* library = game::ObjectLibrary();
    const void* tmpl = library ? game::ObjectLibrary_GetEquipment(library, name.c_str()) : nullptr;
    if (!tmpl)
        return name + " (missing)";
    if (dk2ml::At<int>(tmpl, game::Equipment_Template_type.Offset()) == game::EquipmentType_EMPTY.Get())
        return {};
    const char* id = dk2ml::At<const char*>(tmpl, game::Equipment_Template_tooltip.Offset());
    const char* text = id && *id ? game::Loca_GetTextForId(id, true, false) : nullptr;
    return text && *text ? text : name;
}

int SlotIndex(const std::string& slot)
{
    for (int i = 0; i < kSlotCount; i++)
        if (Usable(g_slots[i]) && slot == SlotName(g_slots[i]))
            return i;
    return -1;
}

// The slot's group (itself, or the weapon / helmet it goes with) is in the operator's locked slots.
bool GroupLocked(const Set* own, int i)
{
    if (!own || own->locked.empty())
        return false;
    std::string root = SlotName(g_slots[g_slots[i].parent < 0 ? i : g_slots[i].parent]);
    return std::find(own->locked.begin(), own->locked.end(), root) != own->locked.end();
}

} // namespace

const char* CategoryLabel(int category)
{
    return category >= 0 && category < kCategories ? kCategoryLabels[category] : "";
}

void Load()
{
    for (int c = 0; c < kCategories; c++) {
        std::string slots;
        for (const SlotDef& s : g_slots)
            if (s.category == c && Usable(s))
                slots += std::string(slots.empty() ? "" : " ") + SlotName(s);
        game::api->Log("loadouts: %s = %s", kCategoryLabels[c], slots.empty() ? "(no slots)" : slots.c_str());
    }

    std::wstring path = Path();
    FILE* f = path.empty() ? nullptr : _wfopen(path.c_str(), L"rb");
    if (!f)
        return;
    std::string text;
    char buf[4096];
    for (size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;)
        text.append(buf, n);
    fclose(f);

    Set* current = nullptr;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        std::string line = text.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        pos = eol == std::string::npos ? text.size() : eol + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == ';')
            continue;
        if (line[0] == '[' && line.back() == ']') {
            current = &g_sets[line.substr(1, line.size() - 2)];
        } else if (current && line.compare(0, 7, "locked=") == 0) { // "locked=PrimaryWeapon Helmet"
            current->locked.clear();
            for (size_t start = 7; start < line.size();) {
                size_t space = line.find(' ', start);
                std::string slot = line.substr(start, space == std::string::npos ? std::string::npos : space - start);
                if (!slot.empty())
                    current->locked.push_back(slot);
                start = space == std::string::npos ? line.size() : space + 1;
            }
        } else if (current) {
            ParseProfile(*current, line);
        }
    }
    game::api->Log("loadouts: %d operator(s) / squad(s) read", static_cast<int>(g_sets.size()));
}

void Save()
{
    std::wstring path = Path();
    if (path.empty())
        return;
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) {
        game::api->Log("loadouts: couldn't write %ls", path.c_str());
        return;
    }
    fprintf(f, "; Era's UI Improvements: saved loadout profiles. [squad|name] = an operator's own, [team:squad] = the squad's.\n");
    for (const auto& [key, set] : g_sets) {
        bool any = !set.locked.empty();
        for (const Profile& p : set.profiles)
            any |= p.used || !p.name.empty();
        if (!any)
            continue;
        fprintf(f, "[%s]\n", key.c_str());
        if (!set.locked.empty()) {
            fprintf(f, "locked=");
            for (size_t k = 0; k < set.locked.size(); k++)
                fprintf(f, "%s%s", k ? " " : "", set.locked[k].c_str());
            fprintf(f, "\n");
        }
        for (int i = 0; i < kProfiles; i++) {
            const Profile& p = set.profiles[i];
            if (!p.used && p.name.empty())
                continue;
            fprintf(f, "%s%d", kProfileKey, i + 1);
            if (!p.name.empty())
                fprintf(f, "\t%s=%s", kNameKey, p.name.c_str());
            for (const auto& [slot, item] : p.items)
                fprintf(f, "\t%s=%s", slot.c_str(), item.c_str());
            fprintf(f, "\n");
        }
    }
    fclose(f);
}

Set* For(const void* trooper)
{
    std::string key = trooper ? OperatorKey(trooper) : std::string();
    return key.empty() ? nullptr : &g_sets[key];
}

Set* TeamFor(const void* trooper)
{
    const void* squad = trooper ? game::Roster_GetSquadForTrooper(trooper) : nullptr;
    return squad ? &g_sets[kTeamPrefix + std::string(SquadNameOf(squad))] : nullptr;
}

std::string KeyOf(const void* trooper)
{
    return trooper ? OperatorKey(trooper) : std::string();
}

std::string ItemName(const std::string& templateName)
{
    return ShownName(templateName);
}

std::string SquadName(const void* trooper)
{
    const void* squad = trooper ? game::Roster_GetSquadForTrooper(trooper) : nullptr;
    return squad ? SquadNameOf(squad) : "";
}

Items Read(const void* trooper, int category)
{
    Items out;
    for (const SlotDef& s : g_slots) {
        if (!InCategory(s, category) || !Usable(s))
            continue;
        const char* equipped = Equipped(trooper, s.value.Get());
        bool empty = dk2ml::At<unsigned>(equipped, game::StaticString48_m_hash.Offset()) == 0;
        out.emplace_back(SlotName(s), empty ? "" : equipped + game::StaticString48_m_szString.Offset());
    }
    return out;
}

Items StartingLoadout(const void* trooper)
{
    if (!trooper || !game::StartingLoadoutUsable())
        return {};
    const void* unit = &dk2ml::At<char>(trooper, game::Roster_Trooper_unit.Offset());
    const void* cls = &dk2ml::At<char>(trooper, game::Roster_Trooper_className.Offset());
    int gender = dk2ml::At<int>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_gender.Offset());
    const void* tmpl = game::Human_GetTemplateForUnitClassGender(unit, cls, gender);
    if (!tmpl)
        return {};

    // StaticString<48>[NUM_VALUES], zeroed (every slot empty) as Human_Client::Init does
    size_t stride = static_cast<size_t>(game::sizeof_StaticString48.Get());
    std::vector<char> slots(static_cast<size_t>(game::Inventory_NUM_VALUES.Get()) * stride);
    game::Inventory_MapRandomInventoryToSlots(&dk2ml::At<char>(tmpl, game::Human_Template_inventory.Offset()), slots.data());

    Items out;
    bool any = false;
    for (const SlotDef& s : g_slots) {
        if (!Usable(s))
            continue;
        const char* slot = slots.data() + static_cast<size_t>(s.value.Get()) * stride;
        bool empty = dk2ml::At<unsigned>(slot, game::StaticString48_m_hash.Offset()) == 0;
        out.emplace_back(SlotName(s), empty ? "" : slot + game::StaticString48_m_szString.Offset());
        any = any || !empty;
    }
    return any ? out : Items{};
}

WriteResult Write(void* trooper, int category, const Items& items)
{
    WriteResult result;
    const void* library = game::ObjectLibrary();
    if (!trooper || !library)
        return result;

    // what goes in each slot (null: not in the saved loadout, eg. a slot added to the game since; left alone)
    const std::string* wanted[kSlotCount] = {};
    bool skip[kSlotCount] = {};
    bool locked[kSlotCount] = {};   // the slot's group is locked by the operator
    bool lockHit[kSlotCount] = {};  // by group root: a locked group that would have changed
    const Set* own = For(trooper);
    for (int i = 0; i < kSlotCount; i++) {
        const SlotDef& s = g_slots[i];
        if (!InCategory(s, category) || !Usable(s))
            continue;
        const char* slot = SlotName(s);
        wanted[i] = Find(items, slot);
        locked[i] = GroupLocked(own, i);
        if (!wanted[i] || wanted[i]->empty() || locked[i])
            continue; // not saved, the slot's Empty choice, or locked (not checked: it won't be written)
        // attachments and NVG aren't bound to classes; Paste loadout checks only the weapon / helmet
        const void* tmpl = game::ObjectLibrary_GetEquipment(library, wanted[i]->c_str());
        if (!tmpl || (s.parent < 0 && !game::BoundToTrooper(library, tmpl, trooper))) {
            game::api->Log("loadouts: %s %s %s, left as it is", slot, wanted[i]->c_str(),
                           tmpl ? "isn't available to this operator" : "doesn't exist (a mod removed?)");
            skip[s.parent < 0 ? i : s.parent] = true;
            result.skipped++;
        }
    }
    // a weapon and its attachments (the helmet and its NVG) change together or not at all
    for (int i = 0; i < kSlotCount; i++)
        if (g_slots[i].parent >= 0 && skip[g_slots[i].parent])
            skip[i] = true;

    size_t capacity = static_cast<size_t>(game::StaticString48_m_hash.Offset() - game::StaticString48_m_szString.Offset());
    for (int i = 0; i < kSlotCount; i++) {
        if (!wanted[i] || skip[i])
            continue;
        char* equipped = const_cast<char*>(Equipped(trooper, g_slots[i].value.Get())); // trooper isn't const here
        char* name = equipped + game::StaticString48_m_szString.Offset();
        unsigned& hash = dk2ml::At<unsigned>(equipped, game::StaticString48_m_hash.Offset());
        if (strcmp(hash ? name : "", wanted[i]->c_str()) == 0)
            continue;
        if (locked[i]) {
            lockHit[g_slots[i].parent < 0 ? i : g_slots[i].parent] = true;
            continue;
        }
        strncpy_s(name, capacity, wanted[i]->c_str(), _TRUNCATE);
        hash = game::Hash(name);
        result.changed++;
    }
    for (bool hit : lockHit)
        result.locked += hit ? 1 : 0;
    return result;
}

bool Same(const void* trooper, const Items& current, const Items& saved)
{
    const Set* own = For(trooper);
    for (const auto& [slot, item] : saved) {
        int i = SlotIndex(slot);
        if (i >= 0 && GroupLocked(own, i))
            continue;
        const std::string* now = Find(current, slot.c_str());
        if (now && *now != item)
            return false;
    }
    return true;
}

std::vector<std::string> LockableSlots()
{
    std::vector<std::string> out;
    for (const SlotDef& s : g_slots)
        if (s.parent < 0 && Usable(s))
            out.push_back(SlotName(s));
    return out;
}

bool IsLocked(const void* trooper, const std::string& slot)
{
    const Set* own = For(trooper);
    return own && std::find(own->locked.begin(), own->locked.end(), slot) != own->locked.end();
}

bool ToggleLock(const void* trooper, const std::string& slot)
{
    Set* own = For(trooper);
    if (!own)
        return false;
    auto it = std::find(own->locked.begin(), own->locked.end(), slot);
    if (it != own->locked.end()) {
        own->locked.erase(it);
        return false;
    }
    own->locked.push_back(slot);
    return true;
}

std::string Summary(int category, const Items& items)
{
    if (category < 0 || category >= kCategories)
        return {};
    const SummaryDef& def = kSummaries[category];
    std::string out;
    for (int k = 0; k < def.count; k++) {
        const SlotDef& s = g_slots[def.slots[k]];
        const std::string* item = Usable(s) ? Find(items, SlotName(s)) : nullptr;
        std::string shown = item && !item->empty() ? ShownName(*item) : std::string();
        if (!shown.empty())
            out += (out.empty() ? "" : def.separator) + shown;
    }
    return out;
}

std::vector<Shown> Contents(int category, const Items& items)
{
    std::vector<Shown> out;
    const void* library = game::ObjectLibrary();
    for (int i = 0; i < kSlotCount; i++) {
        const SlotDef& s = g_slots[i];
        if (s.category != category || s.parent >= 0 || !Usable(s))
            continue;
        const std::string* item = Find(items, SlotName(s));
        std::string name = item && !item->empty() ? ShownName(*item) : std::string();
        if (name.empty())
            continue; // not saved, or the slot's Empty choice
        Shown shown;
        shown.name = name;
        const void* tmpl = library ? game::ObjectLibrary_GetEquipment(library, item->c_str()) : nullptr;
        const char* image = tmpl ? dk2ml::At<const char*>(tmpl, game::Equipment_Template_guiImage.Offset()) : nullptr;
        shown.image = image && *image ? image : nullptr;
        for (int k = i + 1; k < kSlotCount; k++) { // attachments come right after their parent
            if (g_slots[k].parent != i || !Usable(g_slots[k]))
                continue;
            const std::string* att = Find(items, SlotName(g_slots[k]));
            std::string attName = att && !att->empty() ? ShownName(*att) : std::string();
            if (!attName.empty())
                shown.attachments += (shown.attachments.empty() ? "" : ", ") + attName;
        }
        out.push_back(std::move(shown));
    }
    return out;
}

} // namespace loadouts
