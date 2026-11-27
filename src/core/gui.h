// Small helpers over the game's GUI. Lookups, the child walk, show/hide, the game's own actions (AddChild, SetOrigin)
// and our Callback actions go through the loader's GUI kit; the rest (text fitting, icon scaling, colours)
// is ours.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dk2ml.h"

namespace gui {

// what one of our XML <Action type="Callback"/> runs: the item whose event it is (for a cloned cell, the clone) and
// the cursor in GUI space (+y up)
using Callback = DK2ML_GuiCallbackFn;

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
// Sets text on a StaticText with a sizeX (the wrap width) and no sizeY, in at most `lines` lines (1 or 2): if the game
// wraps it onto more, whole words are dropped from the end (then characters, for one very long word) and "..." added
// until it fits. ChangeText lays the text out right away and, with no sizeY, stores the wrapped height in m_size.y, so
// every try is measured by the game itself.
void SetTextFitted(void* label, const std::string& text, TextFit& fit, int lines = 2);
// Sets a StaticText's text and returns its laid-out height. ChangeText lays the text out at once and stores its size
// in m_size for each axis the XML didn't fix: with a sizeX (wrap width) and no sizeY, m_size.y is the wrapped height;
// with neither, m_size.x is the width too.
float SetTextMeasured(void* text, const std::string& s);
// Sets text on a one-line StaticText with no sizeX / sizeY, at most maxW wide: if the game lays it out wider (m_size.x,
// see SetTextMeasured), whole words are dropped from the end (then characters, for one very long word) and "..."
// added until it fits. Such labels also take an lt origin exactly; one with a sizeX doesn't (see CLAUDE.md, XML gotchas).
void SetTextFittedWidth(void* label, const std::string& text, float maxW);
// Stretches a StaticImage's texture to w x h drawn pixels (its render scale); an axis given as <= 0 is kept.
void StretchTo(void* image, float w, float h);
// Scales a StaticImage (after SetImageAndImplicitSize, which keeps the old scale) to fit boxW x boxH, never above
// maxScale.
void FitImage(void* image, float boxW, float boxH, float maxScale);

// The game's <Action type="AddChild" target="item"/> owned by parent: unlinks item from its parent and appends it to
// parent's children (so it's drawn and hit-tested on top of its siblings). Doesn't show it.
void AddChild(void* parent, void* item);
// <Action type="SetOrigin" params="x y">: the item's origin relative to its parent's center, +y up
void SetOrigin(void* item, float x, float y);
// Moves an aligned item vertically (+y up) the way the game moves aligned items (the #InvArmorHelmet row layout,
// 0x1400EA0DD): the same delta on m_alignOffset and m_localOrigin. SetOrigin would write both to the given value
// instead, and for an aligned item the two don't mean the same thing. Callers track what they applied.
void ShiftBy(void* item, float dy);
// An aligned item's m_alignOffset.y, the field ShiftBy moves: lets a caller see that the game re-placed the item
float OffsetY(void* item);

// Points every Callback action of one of item's events (GUI::Item::eItemEventType, eg. game::EVENT_CURSOR_HOVER.Get())
// at fn, under the loader's crash containment. Only ever used on items from our own gui XML; set it on a template
// before cloning (clones copy it).
void SetCallback(void* item, int64_t event, Callback fn);

} // namespace gui
