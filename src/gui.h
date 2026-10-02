// Small helpers over the game's GUI: lookups, the game's own actions (AddChild, SetOrigin) and our Callback actions.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// void callback(const GUI::sAction&): what an XML <Action type="Callback"/> calls when it runs
using Callback = void (*)(const void* action);

void* Find(void* parent, const char* name); // recursive; null-safe
bool IsA(const void* item, const void* vtbl);
bool IsHidden(const void* item);
void* Parent(const void* item);
// The item's own children, in order (drawing order; the last one is on top).
std::vector<void*> Children(const void* item);
// The item's XML name, or "" for an unnamed one (clones have their name cleared).
const char* Name(const void* item);
// A StaticText's current text, or null if item isn't a StaticText.
const char* Text(const void* item);
void ShowIf(void* item, bool show); // null-safe
// Sets a StaticText's color (0xAARRGGBB as in XML textColor) / reads it. Null-safe; non-StaticTexts are ignored.
void SetTextColor(void* item, unsigned color);
unsigned TextColor(const void* item);

// A word-wrapping label's line heights, measured once (see SetTextFitted); 0 until measured, -1 if it can't be.
struct TextFit {
    int oneLine = 0;
    int twoLines = 0;
};
// Sets text on a StaticText with a sizeX (the wrap width) and no sizeY, in at most two lines: if the game wraps it
// onto a third, whole words are dropped from the end (then characters, for one very long word) and "..." added until
// it fits. ChangeText lays the text out right away and, with no sizeY, stores the wrapped height in m_size.y, so every
// try is measured by the game itself.
void SetTextFitted(void* label, const std::string& text, TextFit& fit);
// Scales a StaticImage (after SetImageAndImplicitSize, which keeps the old scale) to fit boxW x boxH, never above
// maxScale.
void FitImage(void* image, float boxW, float boxH, float maxScale);

// The game's <Action type="AddChild" target="item"/> owned by parent: unlinks item from its parent and appends it to
// parent's children (so it's drawn and hit-tested on top of its siblings). Doesn't show it.
void AddChild(void* parent, void* item);
// <Action type="SetOrigin" params="x y">: the item's origin relative to its parent's center, +y up
void SetOrigin(void* item, float x, float y);

// Points every Callback action of one of item's events (GUI::Item::eItemEventType, eg. game::off.eventCursorHover)
// at fn. Only ever used on items from our own gui XML.
void SetCallback(void* item, int64_t event, Callback fn);

// The item that owns an action (for a cloned cell: the clone).
void* ActionOwner(const void* action);
// Cursor y (GUI space, +y up) of the event the action is running for.
float ActionCursorY(const void* action);

} // namespace gui
