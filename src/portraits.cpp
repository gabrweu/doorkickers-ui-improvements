#include "portraits.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "game.h"

namespace portraits {

namespace {

constexpr int kMaxPortraits = 10000;
constexpr int kMaxSquads = 1000;

// sHumanPortrait::tex is a fixed char array that ends where the next field starts
size_t PortraitTexCapacity()
{
    return static_cast<size_t>(game::off.portraitUnit - game::off.portraitTex);
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
    void* library = *game::globals.objectLibrary;
    if (!library)
        return false;
    const char* list = &game::Field<char>(library, game::off.libraryPortraits);
    *data = game::Field<const char*>(list, game::off.listData);
    *count = game::Field<int>(list, game::off.listCount);
    bool ok = *data && *count > 0 && *count <= kMaxPortraits;
    for (int i = 0; ok && i < *count; i++)
        ok = LooksLikeTexturePath(*data + i * game::off.portraitSize + game::off.portraitTex, PortraitTexCapacity());
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

int Fold(char c)
{
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : static_cast<unsigned char>(c);
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// File names in "natural" order: case-insensitive, digit runs compared as numbers (ranger2 < ranger10). Equal names
// fall back to the full path so the order is total.
bool NaturalLess(const char* pathA, const char* pathB)
{
    const char* a = FileName(pathA);
    const char* b = FileName(pathB);
    while (*a && *b) {
        if (IsDigit(*a) && IsDigit(*b)) {
            while (*a == '0')
                a++;
            while (*b == '0')
                b++;
            const char* ea = a;
            const char* eb = b;
            while (IsDigit(*ea))
                ea++;
            while (IsDigit(*eb))
                eb++;
            if (ea - a != eb - b)
                return ea - a < eb - b; // fewer digits = smaller number
            for (; a < ea; a++, b++)
                if (*a != *b)
                    return *a < *b;
            continue;
        }
        if (Fold(*a) != Fold(*b))
            return Fold(*a) < Fold(*b);
        a++;
        b++;
    }
    if (*a || *b)
        return *b != 0; // a is a prefix of b
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
    void* library = *game::globals.objectLibrary;
    if (library && game::off.staticStringSize <= sizeof(key)) {
        game::Field<unsigned>(key, game::off.staticStringHash) = hash;
        if (const void* u = game::fn.getUnit(library, key)) {
            info.known = true;
            info.sortName = &game::Field<char>(u, game::off.unitName + game::off.staticStringText);
            const char* title = info.sortName.c_str();
            const char* nameUI = game::off.unitNameUI >= 0 ? game::Field<const char*>(u, game::off.unitNameUI) : nullptr;
            if (nameUI && nameUI[0]) {
                const char* localized = game::fn.getTextForId(nameUI, true, false);
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

    unsigned unitHash = game::Field<unsigned>(trooper, game::off.trooperUnit + game::off.staticStringHash);
    unsigned classHash = game::Field<unsigned>(trooper, game::off.trooperClass + game::off.staticStringHash);

    // stock rule (human_identities.xml): once any portrait names the trooper's class, only those are used for it
    bool classTagged = false;
    if (unit == UnitFilter::ThisUnit && classHash != 0) {
        for (int i = 0; i < count && !classTagged; i++) {
            const char* p = data + i * game::off.portraitSize;
            classTagged = game::Field<unsigned>(p, game::off.portraitUnit) == unitHash &&
                          game::Field<unsigned>(p, game::off.portraitClass) == classHash;
        }
    }

    std::unordered_set<std::string> seen;
    for (int i = 0; i < count; i++) {
        const char* p = data + i * game::off.portraitSize;
        unsigned pu = game::Field<unsigned>(p, game::off.portraitUnit);
        if (unit == UnitFilter::ThisUnit) {
            unsigned pc = game::Field<unsigned>(p, game::off.portraitClass);
            if (pu != unitHash || (classTagged ? pc != classHash : (pc != 0 && pc != classHash)))
                continue;
        }
        int g = game::Field<int>(p, game::off.portraitGender);
        const char* tex = p + game::off.portraitTex;
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
    return &game::Field<char>(trooper, game::off.trooperId + game::off.humanIdTex);
}

const char* TrooperName(const void* trooper)
{
    return &game::Field<char>(trooper, game::off.trooperId + game::off.humanIdName + game::off.staticStringText);
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
    void** squads = game::Field<void**>(game::globals.rosterSquads, game::off.squadListData);
    int numSquads = game::Field<int>(game::globals.rosterSquads, game::off.squadListCount);
    if (!squads || numSquads < 0 || numSquads > kMaxSquads) {
        static bool logged = false;
        if (!logged && numSquads != 0)
            game::api->Log("roster squad count %d looks wrong, not marking what squadmates use", numSquads);
        logged = true;
        return out;
    }
    // Squad::roster is an inline Trooper array; the count must fit before the next field
    int capacity = (game::off.squadRosterEnd - game::off.squadRoster) / static_cast<int>(game::off.trooperSize);
    for (int s = 0; s < numSquads; s++) {
        const void* squad = squads[s];
        if (!squad)
            continue;
        int count = game::Field<int>(squad, game::off.squadCount);
        if (count < 0 || count > capacity) {
            static bool logged = false;
            if (!logged)
                game::api->Log("squad trooper count %d doesn't fit its roster (%d), skipped", count, capacity);
            logged = true;
            continue;
        }
        const char* roster = &game::Field<char>(squad, game::off.squadRoster);
        for (int i = 0; i < count; i++)
            out.push_back({roster + i * game::off.trooperSize, squad});
    }
    return out;
}

} // namespace

std::vector<User> UsedInRoster(const void* trooper)
{
    std::vector<User> out;
    if (!trooper)
        return out;
    const void* ownSquad = game::fn.getSquadForTrooper(trooper);
    for (const RosterTrooper& r : ReadRoster()) {
        if (r.trooper == trooper)
            continue;
        const char* tex = TrooperTex(r.trooper);
        const char* squadName = &game::Field<char>(r.squad, game::off.squadName + game::off.staticString32Text);
        if (tex[0])
            out.push_back({tex, TrooperName(r.trooper), squadName, r.squad == ownSquad});
    }
    return out;
}

std::vector<const void*> Squadmates(const void* trooper)
{
    std::vector<const void*> out;
    const void* ownSquad = trooper ? game::fn.getSquadForTrooper(trooper) : nullptr;
    if (!ownSquad)
        return out;
    for (const RosterTrooper& r : ReadRoster())
        if (r.squad == ownSquad && r.trooper != trooper)
            out.push_back(r.trooper);
    return out;
}

void Assign(void* trooper, const Entry& e)
{
    char* tex = &game::Field<char>(trooper, game::off.trooperId + game::off.humanIdTex);
    size_t capacity = static_cast<size_t>(game::off.humanIdTexHash - game::off.humanIdTex);
    strncpy_s(tex, capacity, e.tex, _TRUNCATE);
    game::Field<int>(trooper, game::off.trooperId + game::off.humanIdGender) = e.gender;
}

} // namespace portraits
