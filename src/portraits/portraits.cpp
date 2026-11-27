#include "portraits/portraits.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/game.h"
#include "core/natural.h"

namespace portraits {

namespace {

constexpr int kMaxPortraits = 10000;
constexpr int kMaxSquads = 1000;

// sHumanPortrait::tex is a fixed char array that ends where the next field starts
size_t PortraitTexCapacity()
{
    return static_cast<size_t>(game::sHumanPortrait_unit.Offset() - game::sHumanPortrait_tex.Offset());
}

bool LooksLikeTexturePath(const char* s, size_t capacity)
{
    size_t n = strnlen(s, capacity);
    if (n == 0 || n == capacity)
        return false;
    for (size_t i = 0; i < n; i++)
        if (static_cast<unsigned char>(s[i]) < 0x20 || static_cast<unsigned char>(s[i]) > 0x7e)
            return false;
    return strchr(s, '.') != nullptr;
}

std::string Lower(const char* s)
{
    std::string r(s);
    for (char& c : r)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return r;
}

// Reads the library's portrait list, checking it looks like what we expect (the PDB has several ObjectLibrary
// layouts; a wrong one must not crash the game).
bool Pool(const char** data, int* count)
{
    static bool loggedBad = false;
    void* library = *game::g_pObjectLibrary.Address();
    if (!library)
        return false;
    const char* list = &dk2ml::At<char>(library, game::ObjectLibrary_m_humanPortraits.Offset());
    *data = dk2ml::At<const char*>(list, game::List_HumanPortrait_m_list.Offset());
    *count = dk2ml::At<int>(list, game::List_HumanPortrait_m_elements.Offset());
    bool ok = *data && *count > 0 && *count <= kMaxPortraits;
    for (int i = 0; ok && i < *count; i++)
        ok = LooksLikeTexturePath(*data + i * game::sizeof_sHumanPortrait.Get() + game::sHumanPortrait_tex.Offset(), PortraitTexCapacity());
    if (!ok && !loggedBad) {
        game::api->Log("portrait list doesn't look right (count %d), picker disabled", *count);
        loggedBad = true;
    }
    return ok;
}

struct UnitInfo {
    bool known;           // a real ArmyOrg::Unit (mods can give portraits a made-up unit to keep a separate pool)
    std::string sortName; // internal name, eg. "Rangers"
    std::string title;    // header: the localized name, upper case
};

const char* FileName(const char* path)
{
    const char* name = path;
    for (const char* c = path; *c; c++)
        if (*c == '/' || *c == '\\')
            name = c + 1;
    return name;
}

// File names in "natural" order (core/natural.h: ranger2 < ranger10). Equal names fall back to the full path so the
// order is total.
bool NaturalLess(const char* pathA, const char* pathB)
{
    if (int c = natural::Compare(FileName(pathA), FileName(pathB)))
        return c < 0;
    return _stricmp(pathA, pathB) < 0;
}

std::string Upper(std::string s)
{
    for (char& c : s)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return s;
}

// The unit a portrait's unit hash belongs to. The library only has the hash; ObjectLibrary::GetUnit matches units on
// their name's hash alone, so a key with nothing but m_hash set finds it.
const UnitInfo& Unit(unsigned hash)
{
    static std::unordered_map<unsigned, UnitInfo> cache;
    auto it = cache.find(hash);
    if (it != cache.end())
        return it->second;

    UnitInfo info{false, std::string(), "OTHER PORTRAITS"};
    alignas(8) char key[64] = {};
    void* library = *game::g_pObjectLibrary.Address();
    if (library && game::sizeof_StaticString48.Get() <= sizeof(key)) {
        dk2ml::At<unsigned>(key, game::StaticString48_m_hash.Offset()) = hash;
        if (const void* u = game::ObjectLibrary_GetUnit(library, key)) {
            info.known = true;
            info.sortName = &dk2ml::At<char>(u, game::ArmyOrg_Unit_name.Offset() + game::StaticString48_m_szString.Offset());
            const char* title = info.sortName.c_str();
            const char* nameUI = game::UnitNameUIOffset() >= 0 ? dk2ml::At<const char*>(u, game::UnitNameUIOffset()) : nullptr;
            if (nameUI && nameUI[0]) {
                const char* localized = game::Loca_GetTextForId(nameUI, true, false);
                title = localized && localized[0] ? localized : nameUI;
            }
            info.title = Upper(title);
        }
    }
    return cache.emplace(hash, std::move(info)).first->second;
}

} // namespace

Filtered ForTrooper(const void* trooper, UnitFilter unit)
{
    Filtered out;
    const char* data;
    int count;
    if (!trooper || !Pool(&data, &count))
        return out;

    unsigned unitHash = dk2ml::At<unsigned>(trooper, game::Roster_Trooper_unit.Offset() + game::StaticString48_m_hash.Offset());
    unsigned classHash = dk2ml::At<unsigned>(trooper, game::Roster_Trooper_className.Offset() + game::StaticString48_m_hash.Offset());

    // stock rule (human_identities.xml): once any portrait names the trooper's class, only those are used for it
    bool classTagged = false;
    if (unit == UnitFilter::ThisUnit && classHash != 0) {
        for (int i = 0; i < count && !classTagged; i++) {
            const char* p = data + i * game::sizeof_sHumanPortrait.Get();
            classTagged = dk2ml::At<unsigned>(p, game::sHumanPortrait_unit.Offset()) == unitHash &&
                          dk2ml::At<unsigned>(p, game::sHumanPortrait_className.Offset()) == classHash;
        }
    }

    std::unordered_set<std::string> seen;
    for (int i = 0; i < count; i++) {
        const char* p = data + i * game::sizeof_sHumanPortrait.Get();
        unsigned pu = dk2ml::At<unsigned>(p, game::sHumanPortrait_unit.Offset());
        if (unit == UnitFilter::ThisUnit) {
            unsigned pc = dk2ml::At<unsigned>(p, game::sHumanPortrait_className.Offset());
            if (pu != unitHash || (classTagged ? pc != classHash : (pc != 0 && pc != classHash)))
                continue;
        }
        int g = dk2ml::At<int>(p, game::sHumanPortrait_gender.Offset());
        const char* tex = p + game::sHumanPortrait_tex.Offset();
        if (!seen.insert(Lower(tex)).second)
            continue;
        out.entries.push_back({p, tex, g, pu});
    }

    if (unit == UnitFilter::ThisUnit) {
        std::sort(out.entries.begin(), out.entries.end(), [](const Entry& a, const Entry& b) { return NaturalLess(a.tex, b.tex); });
        out.groups.push_back({std::string(), 0, out.entries.size()});
        return out;
    }

    // rank: 0 = the trooper's unit, 1 = other real units (then by name), 2 = not a unit ("OTHER PORTRAITS")
    auto rank = [&](unsigned h) { return h == unitHash ? 0 : Unit(h).known ? 1 : 2; };
    std::stable_sort(out.entries.begin(), out.entries.end(), [&](const Entry& a, const Entry& b) {
        int ra = rank(a.unit), rb = rank(b.unit);
        if (ra != rb)
            return ra < rb;
        if (ra == 1 && a.unit != b.unit) {
            int c = _stricmp(Unit(a.unit).sortName.c_str(), Unit(b.unit).sortName.c_str());
            return c != 0 ? c < 0 : a.unit < b.unit;
        }
        return NaturalLess(a.tex, b.tex); // same group
    });
    for (size_t i = 0; i < out.entries.size(); i++) {
        const UnitInfo& u = Unit(out.entries[i].unit);
        bool sameGroup = !out.groups.empty() &&
                         (u.known ? out.entries[i - 1].unit == out.entries[i].unit : !Unit(out.entries[i - 1].unit).known);
        if (sameGroup)
            out.groups.back().count++;
        else
            out.groups.push_back({u.title, i, 1});
    }
    return out;
}

const char* TrooperTex(const void* trooper)
{
    return &dk2ml::At<char>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_tex.Offset());
}

const char* TrooperName(const void* trooper)
{
    return &dk2ml::At<char>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_name.Offset() + game::StaticString48_m_szString.Offset());
}

namespace {

struct RosterTrooper {
    const void* trooper;
    const void* squad;
};

// Every trooper of every squad (Roster::m_squads, List<Squad*>), validated: a squad count that looks wrong gives
// nothing, a squad whose trooper count doesn't fit its inline array is skipped (both logged once).
std::vector<RosterTrooper> ReadRoster()
{
    std::vector<RosterTrooper> out;
    void** squads = dk2ml::At<void**>(game::Roster_m_squads.Address(), game::List_SquadPtr_m_list.Offset());
    int numSquads = dk2ml::At<int>(game::Roster_m_squads.Address(), game::List_SquadPtr_m_elements.Offset());
    if (!squads || numSquads < 0 || numSquads > kMaxSquads) {
        static bool logged = false;
        if (!logged && numSquads != 0)
            game::api->Log("roster squad count %d looks wrong, not marking what squadmates use", numSquads);
        logged = true;
        return out;
    }
    // Squad::roster is an inline Trooper array; the count must fit before the next field
    int capacity = (game::Roster_Squad_diedInCampaign.Offset() - game::Roster_Squad_roster.Offset()) / static_cast<int>(game::sizeof_Roster_Trooper.Get());
    for (int s = 0; s < numSquads; s++) {
        const void* squad = squads[s];
        if (!squad)
            continue;
        int count = dk2ml::At<int>(squad, game::Roster_Squad_numInRoster.Offset());
        if (count < 0 || count > capacity) {
            static bool logged = false;
            if (!logged)
                game::api->Log("squad trooper count %d doesn't fit its roster (%d), skipped", count, capacity);
            logged = true;
            continue;
        }
        const char* roster = &dk2ml::At<char>(squad, game::Roster_Squad_roster.Offset());
        for (int i = 0; i < count; i++)
            out.push_back({roster + i * game::sizeof_Roster_Trooper.Get(), squad});
    }
    return out;
}

} // namespace

std::vector<User> UsedInRoster(const void* trooper)
{
    std::vector<User> out;
    if (!trooper)
        return out;
    const void* ownSquad = game::Roster_GetSquadForTrooper(trooper);
    for (const RosterTrooper& r : ReadRoster()) {
        if (r.trooper == trooper)
            continue;
        const char* tex = TrooperTex(r.trooper);
        const char* squadName = &dk2ml::At<char>(r.squad, game::Roster_Squad_name.Offset() + game::StaticString32_m_szString.Offset());
        if (tex[0])
            out.push_back({tex, TrooperName(r.trooper), squadName, r.squad == ownSquad});
    }
    return out;
}

std::vector<const void*> Squadmates(const void* trooper)
{
    std::vector<const void*> out;
    const void* ownSquad = trooper ? game::Roster_GetSquadForTrooper(trooper) : nullptr;
    if (!ownSquad)
        return out;
    for (const RosterTrooper& r : ReadRoster())
        if (r.squad == ownSquad && r.trooper != trooper)
            out.push_back(r.trooper);
    return out;
}

void Assign(void* trooper, const Entry& e)
{
    char* tex = &dk2ml::At<char>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_tex.Offset());
    size_t capacity = static_cast<size_t>(game::sHumanId_texHash.Offset() - game::sHumanId_tex.Offset());
    strncpy_s(tex, capacity, e.tex, _TRUNCATE);
    dk2ml::At<int>(trooper, game::Roster_Trooper_id.Offset() + game::sHumanId_gender.Offset()) = e.gender;
}

} // namespace portraits
