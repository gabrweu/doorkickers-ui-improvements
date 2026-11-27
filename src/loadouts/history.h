// Undo for the loadouts panel: per operator, the gear before each change (a loaded loadout, an equipped item, Paste
// loadout...), in memory until the game closes.
#pragma once

#include <string>

namespace loadouts::history {

// Records a change: if the operator's gear differs from what was last seen, the old gear becomes an undo step (label
// names it; empty: made up from the change, eg. "Equipped MP7" or "Changed GUNS"). The first call for an operator only
// takes note of the gear.
void Track(const void* trooper, const std::string& label = {});
// How many steps the operator can undo.
int Steps(const void* trooper);

struct Undone {
    bool done = false;    // there was a step
    std::string label;    // what was undone, eg. "Loaded team GUNS 2"
    std::string restored; // eg. "GUNS back to HK416D 10.5 Suppressed / Glock 19" ("" if nothing needed changing)
    int changed = 0;      // slots written
    int skipped = 0;      // items the operator can't use any more, left as they are
    int locked = 0;       // locked slots (with their attachments) left as they are
};
// Puts the gear from before the last step back (doesn't save or refresh: the caller does, if changed).
Undone Undo(void* trooper);

} // namespace loadouts::history
