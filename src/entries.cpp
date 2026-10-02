#include "entries.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "game.h"
#include "gui.h"
#include "portraits.h"

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

// Firearms by their XML category. Stock categories come in unit flavours (usrifle, opforlmg, militiapistol, ...),
// which are merged; unknown (modded) ones get their own group under their raw name.
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

// Group labels for the other item types, by EquipmentType::eType enumerator name (resolved from the PDB once; a
// name a game update drops just falls into OTHER). FIREARM and EMPTY are handled in GroupOf; SCOPE depends on the
// slot.
struct TypeLabel {
    const char* type;
    const char* label;
    int64_t value;
    bool resolved;
};

TypeLabel g_typeLabels[] = {
    {"SHIELD", "SHIELDS"},
    {"GRENADE", "GRENADES"},
    {"EXPLOSIVE_CHARGE", "CHARGES"},
    {"ARMOR", "ARMOR"},
    {"HELMET", "HELMETS"},
    {"HELMET_NVG", "NIGHT VISION"},
    {"AMMO", "AMMO"},
    {"DISGUISE", "DISGUISES"},
    {"LOCKPICK", "TOOLS"},
    {"LOCKPICK_MACHINE", "TOOLS"},
    {"CROWBAR", "TOOLS"},
    {"DYNAMIC_HAMMER", "TOOLS"},
    {"BOLT_CUTTER", "TOOLS"},
    {"BREACHING_KIT", "TOOLS"},
    {"TEC_TORCH", "TOOLS"},
};
int64_t g_scopeType = -1;
bool g_typesResolved = false;

void ResolveTypes()
{
    if (g_typesResolved)
        return;
    g_typesResolved = true;
    for (TypeLabel& t : g_typeLabels) {
        t.resolved = game::api->GetEnumValue("EquipmentType::eType", t.type, &t.value) == DK2ML_OK;
        if (!t.resolved)
            game::api->Log("EquipmentType::%s not found; those items are grouped under OTHER", t.type);
    }
    if (game::api->GetEnumValue("EquipmentType::eType", "SCOPE", &g_scopeType) != DK2ML_OK)
        g_scopeType = -1;
}

// The header an item goes under. Scope items are optics in the weapon scope slots, but Trooper Toolbox's Custom
// slots use them for cosmetics.
std::string GroupOf(const Entry& e, const std::string& binding)
{
    if (e.type == game::off.eqpEmpty)
        return "NONE";
    if (e.type == game::off.eqpFirearm)
        return e.category.empty() ? "OTHER WEAPONS" : WeaponGroup(e.category);
    if (e.type == g_scopeType)
        return Has(binding, "Scope") ? "OPTICS" : "GEAR";
    for (const TypeLabel& t : g_typeLabels)
        if (t.resolved && e.type == t.value)
            return t.label;
    return "OTHER";
}

const char* Str(const void* p, int32_t offset)
{
    const char* s = game::Field<const char*>(p, offset);
    return s ? s : "";
}

bool Visible(void* parent, const char* name)
{
    void* item = gui::Find(parent, name);
    return item && !gui::IsHidden(item);
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
        if (!gui::IsA(item, game::globals.buttonVtbl))
            continue;
        void* idLabel = gui::Find(item, "#id");
        if (!gui::IsA(idLabel, game::globals.staticTextVtbl))
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
            e.type = game::off.eqpEmpty;
        } else if (const void* tmpl = game::fn.getEquipment(library, e.id.c_str())) {
            e.type = game::Field<int>(tmpl, game::off.eqpType);
            e.image = game::Field<const char*>(tmpl, game::off.eqpGuiImage);
            e.category = Str(tmpl, game::off.eqpCategory + game::off.hashedStringText);
            for (char& c : e.category)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (binding.empty())
                binding = Str(tmpl, game::off.eqpBinding);
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
    ResolveTypes();

    // groups in order of first appearance, each group's entries in stock order; the slot's Empty choice (NONE) first
    std::vector<std::string> titles;
    std::unordered_map<std::string, size_t> index;
    std::vector<std::vector<size_t>> members;
    size_t firearms = 0;
    for (size_t i = 0; i < read.size(); i++) {
        firearms += read[i].type == game::off.eqpFirearm;
        std::string title = GroupOf(read[i], binding);
        auto [it, added] = index.emplace(title, titles.size());
        if (added) {
            titles.push_back(title);
            members.emplace_back();
        }
        members[it->second].push_back(i);
    }
    for (size_t g = 1; g < titles.size(); g++) {
        if (read[members[g][0]].type != game::off.eqpEmpty)
            continue;
        std::rotate(titles.begin(), titles.begin() + g, titles.begin() + g + 1);
        std::rotate(members.begin(), members.begin() + g, members.begin() + g + 1);
        break;
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
    if (game::off.firearmSuppressedSwitch < 0 || !library)
        return nullptr;
    const void* tmpl = game::fn.getEquipment(library, name);
    if (!tmpl || game::Field<int>(tmpl, game::off.eqpType) != game::off.eqpFirearm)
        return nullptr;
    const char* binding = game::Field<const char*>(tmpl, game::off.eqpBinding);
    const char* plain = game::Field<const char*>(tmpl, game::off.firearmSuppressedSwitch);
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
        for (int64_t slot = 1; slot < game::off.invCount; slot++) {
            const char* item = &game::Field<char>(mate, game::off.trooperInventory + static_cast<int32_t>(slot * game::off.staticStringSize));
            if (game::Field<unsigned>(item, game::off.staticStringHash) == 0 || !item[game::off.staticStringText])
                continue;
            const char* name = item + game::off.staticStringText;
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
