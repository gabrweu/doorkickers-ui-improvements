#include "equipment/entries.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/natural.h"
#include "core/settings.h"
#include "portraits/portraits.h"

namespace entries {

namespace {

std::string Upper(const std::string& s)
{
    std::string u(s);
    for (char& c : u)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return u;
}

bool Has(const std::string& s, const char* part)
{
    return s.find(part) != std::string::npos;
}

// Pistol-calibre caliber keys (caliberInfoText): 9x19 (and 9x19subsonic), 9x18, 4.6x30, 5.7x28, .45 ACP, and modded
// "_9mm" keys (eg. bv_caliber_9mm_name). 9x39 and .300 BLK are rifle cartridges and don't match.
bool SmgCaliber(const std::string& c)
{
    return Has(c, "9x19") || Has(c, "9x18") || Has(c, "46x30") || Has(c, "57x28") || Has(c, "45acp") || Has(c, "_9mm");
}

// Eastern-bloc rifle caliber keys: 7.62x39, 5.45x39, 9x39 (subsonic / AP / a raw "9x39mm"), 7.62x54R, 12.7x55, the
// AKM/GP-34's 76239gl40, and BV's bv_caliber_545_name. No NATO-calibre rifle in the base game or the mods checked
// matches.
bool RedforCaliber(const std::string& c)
{
    return Has(c, "762x39") || Has(c, "545x39") || Has(c, "9x39") || Has(c, "762x54") || Has(c, "127x55") ||
           Has(c, "76239") || Has(c, "_545");
}

// AKs in NATO calibres (Korsakov's 5.56 AK-102 and AK-102/GP-25) still count as Redfor: a display name starting with
// "AK". AK names read the same in every game language, and no NATO rifle's does.
bool AkName(const std::string& name)
{
    size_t i = name.find_first_not_of(' ');
    return i != std::string::npos && name.size() >= i + 2 && std::toupper(static_cast<unsigned char>(name[i])) == 'A' &&
           std::toupper(static_cast<unsigned char>(name[i + 1])) == 'K';
}

// Mods file some machine guns and marksman rifles as "rifle" (Wotan's Minimi 7.62 and SCAR 20; NGF/GOON's PKMs).
// Their stats give them away. Belt-fed guns hold 100-250 rounds, and no rifle in the base game or the mods checked
// reaches 100 (the biggest are 75-round RPK/AK drums). Marksman rifles have flat accuracy of 400-450 (M110A1, M14 EBR,
// MK20 SSR 450/450, M16A4 Marksman 400/400), and no ordinary rifle tops 300 (Ranger M16A4, AUG).
constexpr int kBeltFedRounds = 100;
constexpr float kMarksmanAccuracy = 400;

// Firearms by their XML category. Stock categories come in unit flavours (usrifle, opforlmg, militiapistol, ...),
// which are merged; unknown (modded) ones get their own group under their raw name. GroupOf then refines RIFLES:
// misfiled MGs and marksman rifles by their stats, and the rest by caliber into SMGS, REDFOR RIFLES and BLUFOR RIFLES
// (the game has no SMG category and no faction split).
std::string WeaponGroup(const std::string& c)
{
    if (Has(c, "pistol"))
        return "PISTOLS";
    if (Has(c, "lmg"))
        return "MACHINE GUNS";
    if (Has(c, "dmr"))
        return "MARKSMAN RIFLES";
    if (c == "magnified")
        return "SCOPED RIFLES";
    if (Has(c, "shotgun"))
        return "SHOTGUNS";
    if (c == "rpg")
        return "LAUNCHERS";
    if (c == "melee")
        return "MELEE";
    if (c == "shield")
        return "SHIELDS";
    if (Has(c, "rifle"))
        return "RIFLES";
    return Upper(c);
}

// Group labels for the other item types, by EquipmentType::eType enumerator name: optional bindings, resolved with
// the rest in DK2ML_PluginInit (dk2ml::ResolveAll logs a name a game update drops, and its items fall into OTHER).
// FIREARM and EMPTY are handled in GroupOf; SCOPE depends on the slot.
struct TypeLabel {
    dk2ml::Enum value;
    const char* label;
};

constexpr const char* kEquipmentType = "EquipmentType::eType";
TypeLabel g_typeLabels[] = {
    {{kEquipmentType, "SHIELD", dk2ml::Optional}, "SHIELDS"},
    {{kEquipmentType, "GRENADE", dk2ml::Optional}, "GRENADES"},
    {{kEquipmentType, "EXPLOSIVE_CHARGE", dk2ml::Optional}, "CHARGES"},
    {{kEquipmentType, "ARMOR", dk2ml::Optional}, "ARMOR"},
    {{kEquipmentType, "HELMET", dk2ml::Optional}, "HELMETS"},
    {{kEquipmentType, "HELMET_NVG", dk2ml::Optional}, "NIGHT VISION"},
    {{kEquipmentType, "AMMO", dk2ml::Optional}, "AMMO"},
    {{kEquipmentType, "DISGUISE", dk2ml::Optional}, "DISGUISES"},
    {{kEquipmentType, "LOCKPICK", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "LOCKPICK_MACHINE", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "CROWBAR", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "DYNAMIC_HAMMER", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "BOLT_CUTTER", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "BREACHING_KIT", dk2ml::Optional}, "TOOLS"},
    {{kEquipmentType, "TEC_TORCH", dk2ml::Optional}, "TOOLS"},
};
dk2ml::Enum g_scopeType{kEquipmentType, "SCOPE", dk2ml::Optional};

// The header an item goes under. Scope items are optics in the weapon scope slots, but Trooper Toolbox's Custom
// slots use them for cosmetics.
std::string GroupOf(const Entry& e, const std::string& binding)
{
    if (e.type == game::EquipmentType_EMPTY.Get())
        return "NONE";
    if (e.type == game::EquipmentType_FIREARM.Get()) {
        if (e.category.empty())
            return "OTHER WEAPONS";
        std::string group = WeaponGroup(e.category);
        if (group != "RIFLES")
            return group;
        if (e.rounds >= kBeltFedRounds)
            return "MACHINE GUNS";
        if (e.accuracy >= kMarksmanAccuracy)
            return "MARKSMAN RIFLES";
        if (game::CaliberInfoOffset() < 0) // without the caliber field, the rest of the rifles stay one group
            return group;
        if (SmgCaliber(e.caliber))
            return "SMGS";
        return RedforCaliber(e.caliber) || AkName(e.name) ? "REDFOR RIFLES" : "BLUFOR RIFLES";
    }
    if (g_scopeType.Resolved() && e.type == g_scopeType.Get())
        return Has(binding, "Scope") ? "OPTICS" : "GEAR";
    for (const TypeLabel& t : g_typeLabels)
        if (t.value.Resolved() && e.type == t.value.Get())
            return t.label;
    return "OTHER";
}

const char* Str(const void* p, int32_t offset)
{
    const char* s = dk2ml::At<const char*>(p, offset);
    return s ? s : "";
}

bool Visible(void* parent, const char* name)
{
    void* item = gui::Find(parent, name);
    return item && !gui::IsHidden(item);
}

bool Digit(char c)
{
    return c >= '0' && c <= '9';
}

// A number at name[i] ("5.56", ".45", "5,56": '.' or ',' as the decimal mark), moving i past it. False if none.
bool Number(const std::string& name, size_t& i, float& out)
{
    std::string digits;
    for (; i < name.size(); i++) {
        char c = name[i];
        bool mark = (c == '.' || c == ',') && i + 1 < name.size() && Digit(name[i + 1]);
        if (!Digit(c) && !mark)
            break;
        digits += c == ',' ? '.' : c;
    }
    if (digits.empty())
        return false;
    out = std::strtof(digits.c_str(), nullptr);
    return true;
}

// A caliber's place in the size order: kind 0 (bullets, by bore then case length), 1 (shotgun gauges), 2 (unknown).
// The bore is in mm, rounded to 0.1 so near-equal rounds (.300 BLK and 7.62 mm, .38 and .380) compare as one size.
struct CaliberKey {
    int kind = 2;
    float bore = 0, length = 0;
};

// Parses the caliber as the game shows it: "5.56x45mm" (bore x case length), "5.56x45mm/40mm HE" (the first round),
// ".45 ACP" (inches), "40mm Low Velocity" / "84mm HEAT" (bore only), "12 Gauge". Anything without a number is unknown.
CaliberKey CaliberSize(const std::string& name)
{
    CaliberKey key;
    size_t i = 0;
    while (i < name.size() && !Digit(name[i]) && !(name[i] == '.' && i + 1 < name.size() && Digit(name[i + 1])))
        i++;
    if (i == name.size())
        return key;
    bool inches = name[i] == '.';
    float n = 0;
    if (!Number(name, i, n))
        return key;
    key.kind = 0;
    key.bore = inches ? n * 25.4f : n;
    if (!inches) {
        while (i < name.size() && name[i] == ' ')
            i++;
        float length = 0;
        size_t after = i + 1;
        if (i < name.size() && (name[i] == 'x' || name[i] == 'X') && Number(name, after, length)) {
            key.length = length;
        } else {
            std::string rest = name.substr(i, 5);
            for (char& c : rest)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (rest.rfind("ga", 0) == 0)
                key.kind = 1; // "12 Gauge"
        }
    }
    key.bore = std::round(key.bore * 10) / 10;
    return key;
}

// Guns by caliber size, then the caliber's name (keeps "9x19mm" and "9x19mm Subsonic" apart, GL variants after the
// plain round), then by name; a gun whose caliber is unknown goes last, by name. Only gun groups are sorted.
bool CaliberThenName(const Entry& a, const CaliberKey& ka, const Entry& b, const CaliberKey& kb)
{
    if (ka.kind != kb.kind)
        return ka.kind < kb.kind;
    if (ka.bore != kb.bore)
        return ka.bore < kb.bore;
    if (ka.length != kb.length)
        return ka.length < kb.length;
    if (int c = natural::Compare(a.caliberName.c_str(), b.caliberName.c_str()))
        return c < 0;
    return natural::Compare(a.name.c_str(), b.name.c_str()) < 0;
}

} // namespace

bool FromStockList(void* stockList, List& out)
{
    const void* library = game::ObjectLibrary();
    if (!library)
        return false;
    std::vector<Entry> read;
    std::string binding;
    for (void* item : gui::Children(stockList)) {
        // the list's children are the cloned #equipment_item_template buttons (plus the scrollbar's ButtonGraphics)
        if (!gui::IsA(item, game::GUI_Button_vftable.Address()))
            continue;
        void* idLabel = gui::Find(item, "#id");
        if (!gui::IsA(idLabel, game::GUI_StaticText_vftable.Address()))
            continue;
        const char* id = gui::Text(idLabel);
        Entry e{};
        e.id = id ? id : "";
        const char* name = gui::Text(gui::Find(item, "#name"));
        e.name = name && *name ? name : e.id;
        e.type = -1;
        if (e.id.empty()) {
            // The slot's Empty choice: the builder fills EMPTY items with a null template (0x140117EF0), which leaves
            // #id empty and #name "@menu_cust_item_empty". Our cell gets the same empty #id, so equipping it does
            // what the stock item does.
            e.type = game::EquipmentType_EMPTY.Get();
        } else if (const void* tmpl = game::ObjectLibrary_GetEquipment(library, e.id.c_str())) {
            e.type = dk2ml::At<int>(tmpl, game::Equipment_Template_type.Offset());
            e.image = dk2ml::At<const char*>(tmpl, game::Equipment_Template_guiImage.Offset());
            e.category = Str(tmpl, game::Equipment_Template_category.Offset() + game::HashedString_m_pString.Offset());
            for (char& c : e.category)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (e.type == game::EquipmentType_FIREARM.Get()) {
                // only firearm templates are EqpFirearm_Template, so only they have the field
                int32_t caliber = game::CaliberInfoOffset();
                if (caliber >= 0) {
                    e.caliber = Str(tmpl, caliber);
                    // looked up with the key as written (the lookup may be case-sensitive: "9x39AP")
                    const char* shown = e.caliber.empty() ? nullptr : game::Loca_GetTextForId(e.caliber.c_str(), true, false);
                    e.caliberName = shown ? shown : "";
                    for (char& c : e.caliber)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                e.rounds = static_cast<int>(game::ModifiableParam(tmpl, "roundsPerMagazine", 0));
                e.accuracy = std::min(game::ModifiableParam(tmpl, "accuracyStart", 0), game::ModifiableParam(tmpl, "accuracyEnd", 0));
                // once: SmgCaliber expects the XML key (eg. "@firearm_caliber_9x19_name"), not localized text, and
                // rounds/accuracy show whether the <ModifiableParams> walk found its names
                static bool logged = false;
                if (!logged) {
                    logged = true;
                    game::api->Log("caliber sample: %s = \"%s\" rounds=%d accuracy=%.0f", e.id.c_str(), e.caliber.c_str(),
                                   e.rounds, e.accuracy);
                }
            }
            if (binding.empty())
                binding = Str(tmpl, game::Equipment_Template_inventoryBinding.Offset());
        }
        // as the stock filler (0x140115A20 in game build 112) left the item
        e.equipped = Visible(item, "#equipment_item_equipped");
        e.locked = Visible(item, "#unlock");
        e.battleHonors = e.locked && Visible(item, "#unlock_bh_bgnd");
        if (e.locked) {
            const char* cost = gui::Text(gui::Find(item, "#unlock_stars"));
            e.cost = cost ? cost : "";
        }
        read.push_back(std::move(e));
    }

    if (read.empty())
        return false;

    // groups in order of first appearance, each group's entries in stock order (gun groups sorted below); the slot's Empty choice (NONE) first
    std::vector<std::string> titles;
    std::unordered_map<std::string, size_t> index;
    std::vector<std::vector<size_t>> members;
    size_t firearms = 0;
    for (size_t i = 0; i < read.size(); i++) {
        firearms += read[i].type == game::EquipmentType_FIREARM.Get();
        std::string title = GroupOf(read[i], binding);
        auto [it, added] = index.emplace(title, titles.size());
        if (added) {
            titles.push_back(title);
            members.emplace_back();
        }
        members[it->second].push_back(i);
    }
    for (size_t g = 1; g < titles.size(); g++) {
        if (read[members[g][0]].type != game::EquipmentType_EMPTY.Get())
            continue;
        std::rotate(titles.begin(), titles.begin() + g, titles.begin() + g + 1);
        std::rotate(members.begin(), members.begin() + g, members.begin() + g + 1);
        break;
    }
    // each group's items by their shown (localized) name, in natural order (G3 < G36); same names keep stock order
    // gun groups (every member a firearm) by caliber, then name (CaliberThenName); other gear keeps the stock order
    if (g_settings.sortByName) {
        std::vector<CaliberKey> keys(read.size());
        for (size_t i = 0; i < read.size(); i++) {
            if (read[i].caliberName.empty())
                continue;
            keys[i] = CaliberSize(read[i].caliberName);
            static bool logged = false; // once: shows the parse works on this language's caliber names
            if (!logged) {
                logged = true;
                game::api->Log("caliber size sample: \"%s\" = kind %d, %.1f x %.0f", read[i].caliberName.c_str(), keys[i].kind,
                               keys[i].bore, keys[i].length);
            }
        }
        for (std::vector<size_t>& m : members) {
            bool guns = std::all_of(m.begin(), m.end(), [&](size_t i) { return read[i].type == game::EquipmentType_FIREARM.Get(); });
            if (guns)
                std::stable_sort(m.begin(), m.end(), [&](size_t a, size_t b) { return CaliberThenName(read[a], keys[a], read[b], keys[b]); });
        }
    }

    out.binding = binding;
    out.wide = firearms * 2 > read.size();
    out.entries.clear();
    out.groups.clear();
    for (size_t g = 0; g < titles.size(); g++) {
        out.groups.push_back({titles[g], out.entries.size(), members[g].size()});
        for (size_t i : members[g])
            out.entries.push_back(std::move(read[i]));
    }
    return true;
}

} // namespace entries

namespace entries {

namespace {

// The plain weapon a suppressed firearm (one bound to a muzzle slot) switches back to, or null.
const char* SuppressedTwinOf(const void* library, const char* name)
{
    if (game::SuppressedSwitchOffset() < 0 || !library)
        return nullptr;
    const void* tmpl = game::ObjectLibrary_GetEquipment(library, name);
    if (!tmpl || dk2ml::At<int>(tmpl, game::Equipment_Template_type.Offset()) != game::EquipmentType_FIREARM.Get())
        return nullptr;
    const char* binding = dk2ml::At<const char*>(tmpl, game::Equipment_Template_inventoryBinding.Offset());
    const char* plain = dk2ml::At<const char*>(tmpl, game::SuppressedSwitchOffset());
    return binding && strstr(binding, "Muzzle") && plain && *plain ? plain : nullptr;
}

} // namespace

void MarkSquadUse(List& list, const void* trooper)
{
    // template name -> the squadmates who have it equipped
    std::unordered_map<std::string, std::vector<const void*>> users;
    auto add = [&](const std::string& name, const void* mate) {
        std::vector<const void*>& who = users[name];
        if (std::find(who.begin(), who.end(), mate) == who.end()) // the same item in two of their slots: once
            who.push_back(mate);
    };
    const void* library = game::ObjectLibrary();
    int twins = 0;
    std::string example;
    for (const void* mate : portraits::Squadmates(trooper)) {
        for (int64_t slot = 1; slot < game::Inventory_NUM_VALUES.Get(); slot++) {
            const char* item = &dk2ml::At<char>(mate, game::Roster_Trooper_inventory.Offset() + static_cast<int32_t>(slot * game::sizeof_StaticString48.Get()));
            if (dk2ml::At<unsigned>(item, game::StaticString48_m_hash.Offset()) == 0 || !item[game::StaticString48_m_szString.Offset()])
                continue;
            const char* name = item + game::StaticString48_m_szString.Offset();
            add(name, mate);
            // A suppressor isn't an attachment: the muzzle slot equips the weapon's suppressed twin (eg. FRWD_CSAW_Sup,
            // inventoryBinding PrimaryWeaponMuzzle), linked to the plain one by suppressedSwitch. Someone with the
            // suppressed twin also uses the plain weapon, so they count for it in the weapon list (not the other way:
            // the muzzle list counts only those with the suppressor).
            if (const char* plain = SuppressedTwinOf(library, name)) {
                add(plain, mate);
                if (twins++ == 0)
                    example = std::string(name) + " -> " + plain;
            }
        }
    }
    if (twins)
        game::api->Log("squad usage: %d suppressed weapon(s) counted for their plain version (eg. %s)", twins, example.c_str());
    for (Entry& e : list.entries) {
        e.usedBy.clear();
        auto it = e.id.empty() ? users.end() : users.find(e.id);
        if (it == users.end())
            continue;
        for (const void* mate : it->second)
            e.usedBy.emplace_back(portraits::TrooperName(mate));
    }
}

} // namespace entries
