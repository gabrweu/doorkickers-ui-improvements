// Undo history. The game keeps none, and gear changes through several paths (our LOAD, the equipment grid / stock
// list, the unlock pop-up, Paste loadout, the deploy screen), so the history doesn't hook any of them: the panel calls
// Track whenever it refreshes (trooper select, after every Customize_OnEvent, after its own actions), and a change since
// the gear last seen becomes a step. Undo writes the old gear back with store.cpp's Write, as LOAD does.
#include "loadouts/history.h"

#include <deque>
#include <map>

#include "loadouts/store.h"

namespace loadouts::history {

namespace {

constexpr size_t kMaxSteps = 20;

struct Gear {
    Items categories[kCategories];
};

struct Step {
    Gear before;
    std::string label;
};

struct History {
    bool seen = false;
    Gear baseline; // the gear last seen
    std::deque<Step> steps;
};

std::map<std::string, History> g_histories; // by loadouts::KeyOf

Gear Current(const void* trooper)
{
    Gear g;
    for (int c = 0; c < kCategories; c++)
        g.categories[c] = Read(trooper, c);
    return g;
}

bool Same(const Gear& a, const Gear& b)
{
    for (int c = 0; c < kCategories; c++)
        if (a.categories[c] != b.categories[c])
            return false;
    return true;
}

// "Equipped MP7" / "Removed Halligan" for one slot, else "Changed GUNS and LOOKS"
std::string Describe(const Gear& before, const Gear& after)
{
    int changed = 0;
    std::string oldItem, newItem, categories;
    bool categoryChanged[kCategories] = {};
    for (int c = 0; c < kCategories; c++) {
        const Items& a = before.categories[c];
        const Items& b = after.categories[c];
        if (a.size() != b.size()) {
            categoryChanged[c] = true;
            changed += 2;
            continue;
        }
        for (size_t k = 0; k < a.size(); k++) {
            if (a[k].second == b[k].second)
                continue;
            categoryChanged[c] = true;
            changed++;
            oldItem = a[k].second;
            newItem = b[k].second;
        }
    }
    if (changed == 1) {
        std::string shown = newItem.empty() ? std::string() : ItemName(newItem);
        if (!shown.empty())
            return "Equipped " + shown;
        std::string old = oldItem.empty() ? std::string() : ItemName(oldItem);
        return old.empty() ? "Changed an item" : "Removed " + old;
    }
    int count = 0, total = 0;
    for (bool b : categoryChanged)
        total += b ? 1 : 0;
    for (int c = 0; c < kCategories; c++) {
        if (!categoryChanged[c])
            continue;
        count++;
        categories += std::string(count == 1 ? "" : count == total ? " and " : ", ") + CategoryLabel(c);
    }
    return "Changed " + categories;
}

} // namespace

void Track(const void* trooper, const std::string& label)
{
    std::string key = KeyOf(trooper);
    if (key.empty())
        return;
    History& h = g_histories[key];
    Gear now = Current(trooper);
    if (!h.seen) {
        h.seen = true;
        h.baseline = now;
        return;
    }
    if (Same(now, h.baseline))
        return;
    h.steps.push_back({h.baseline, label.empty() ? Describe(h.baseline, now) : label});
    if (h.steps.size() > kMaxSteps)
        h.steps.pop_front();
    h.baseline = now;
}

int Steps(const void* trooper)
{
    auto it = g_histories.find(KeyOf(trooper));
    return it == g_histories.end() ? 0 : static_cast<int>(it->second.steps.size());
}

Undone Undo(void* trooper)
{
    Undone out;
    auto it = g_histories.find(KeyOf(trooper));
    if (it == g_histories.end() || it->second.steps.empty())
        return out;
    History& h = it->second;
    Step step = h.steps.back();
    h.steps.pop_back();
    out.done = true;
    out.label = step.label;

    Gear now = Current(trooper);
    for (int c = 0; c < kCategories; c++) {
        if (step.before.categories[c] == now.categories[c])
            continue;
        WriteResult r = Write(trooper, c, step.before.categories[c]);
        out.changed += r.changed;
        out.skipped += r.skipped;
        out.locked += r.locked;
        std::string summary = Summary(c, step.before.categories[c]);
        out.restored += std::string(out.restored.empty() ? "" : "; ") + CategoryLabel(c) + " back to " +
                        (summary.empty() ? "nothing equipped" : summary);
    }
    h.baseline = Current(trooper); // the undo itself isn't a step
    return out;
}

} // namespace loadouts::history
