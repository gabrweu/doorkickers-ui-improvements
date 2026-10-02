// Trooper Toolbox (TT) splits the Customization screen in two pages: its "More Gear and Cosmetics" button
// (#cosmetic_panel_toggle) opens #cosmetic_panel, with Custom7-9 (cosmetics) and SupportGear2/3 (breaching tools) as
// big boxes, and "Go Back" (#close_cosmetic_panel) returns. Here those five slots go onto the main page instead, in the
// free spots around the figure, at the size of the main page's own Custom3 box, and the button is hidden.
//
// Nothing is copied or reparented: #cosmetic_panel is a child of #inventory_panel in TT's customization.xml, in the
// same coordinate space (both align="r", no origin), so the slots just get main-page origins (SetOrigin, as TT's XML
// does) and their render objects rescaled. TT's own actions keep working on the same items. TT's XML would undo it:
//   - GUI_CUSTOMIZE_SHOW_DEFAULT (screen open, list closed) hides the five slots and #close_cosmetic_panel, and shows
//     #cosmetic_panel_toggle. Those Hides of the slots and Shows of the two buttons are skipped while merged.
//   - every slot's OnClick hides all slot boxes and #inventory_panel to open the list; hiding the panel already hides
//     our five (its children), so skipping their own Hide changes nothing there.
//   - CLOSE_COSMETIC_GUI / Go Back hide #cosmetic_panel: skipped too (unreachable without the button anyway).
// slots.cpp's own Show/Hide calls (empty slots) aren't filtered (plugin.cpp checks Slots_IsInternal first), so a merged
// slot the trooper has nothing for is hidden like any other.
#include "ttmerge.h"

#include <cstring>

#include "game.h"
#include "gui.h"
#include "settings.h"

namespace {

// Main-page origins in #inventory_panel space (align r), for boxes the size of Custom3. Fitted on a 2000x1125
// screenshot of TT's page: x = -544 + (left - 1022) * 1.28, y = 528 - (top - 88) * 1.285.
struct Place {
    const char* name;
    float x, y;
    int group; // slots of one group (non-zero) share their places: the visible ones fill them left to right, no gaps
};
constexpr Place kPlaces[] = {
    {"Custom7", -593, 304, 0},       // eyepro: over the left shoulder, between Helmet and Custom3
    {"Custom9", -135, 304, 0},       // patch: over the right shoulder, between Custom2 and Custom4
    {"Custom8", -135, -435, 0},       // gloves: under Custom6, on the pouch row
    {"SupportGear2", -526, -435, 1}, // tools: after UtilityPouch2, on the pouch row (labels stay inside the window)
    {"SupportGear3", -329, -435, 1},
};
constexpr size_t kCount = sizeof(kPlaces) / sizeof(kPlaces[0]);
// the main-page box the merged ones copy their size from (the first one found)
constexpr const char* kReferences[] = {"Custom3", "Custom4", "Custom5", "Custom6"};

constexpr int kButtons = 2; // Checkbox::m_pButtons: unchecked, checked
constexpr int kStates = 3;  // Button::m_renderObjects: normal, hover, click

struct Saved {
    float origin[2];
    int size[2];
    int buttonSize[kButtons][2];
    float scale[kButtons][kStates][2];
    float imageOrigin[2];
    float imageScale[2];
    float nameOrigin[2];
};

struct Slot {
    void* item;
    Saved saved;
    float at[2]; // the origin we gave it (Arrange moves grouped slots)
};

Slot g_slots[kCount];
void* g_root = nullptr;    // the GUI the pointers below belong to
void* g_inventory = nullptr; // #inventory_panel (the main page; TT shows it again when a list closes)
void* g_panel = nullptr;   // #cosmetic_panel
void* g_reference = nullptr; // the main-page box the merged ones copy (Custom3)
void* g_toggle = nullptr;  // #cosmetic_panel_toggle
void* g_close = nullptr;   // #close_cosmetic_panel
bool g_merged = false;
bool g_internal = false;   // our own Show/Hide calls
bool g_reported = false;   // "not found" logged for this GUI

bool OffsetsOk()
{
    using game::off;
    return off.checkboxButtons > 0 && off.checkboxButtons % 8 == 0 && off.buttonRenderObjects > 0 &&
           off.renderObjectSize >= 64 && off.renderObjectSize <= 1024;
}

// After a GUI reload the old items are gone: forget them (nothing to restore).
bool SameRoot()
{
    void* root = game::GuiRoot();
    if (root == g_root)
        return true;
    g_root = root;
    g_merged = false;
    g_reported = false;
    g_panel = g_toggle = g_close = g_inventory = g_reference = nullptr;
    for (Slot& s : g_slots)
        s.item = nullptr;
    return false;
}

void SetShown(void* item, bool show)
{
    g_internal = true;
    gui::ShowIf(item, show);
    g_internal = false;
}

// What SetOrigin would have been given: the XML origin (m_alignOffset) of an aligned item, else m_localOrigin.
float* Origin(void* item)
{
    bool aligned = game::Field<unsigned>(item, game::off.itemAlignFlags) != 0;
    return &game::Field<float>(item, aligned ? game::off.itemAlignOffset : game::off.itemLocalOrigin);
}

int* Size(void* item)
{
    return &game::Field<int>(item, game::off.itemSize);
}

void* Button(void* checkbox, int i)
{
    return game::Field<void*>(checkbox, game::off.checkboxButtons + i * static_cast<int32_t>(sizeof(void*)));
}

char* Render(void* button, int state)
{
    return &game::Field<char>(button, game::off.buttonRenderObjects + state * static_cast<int32_t>(game::off.renderObjectSize));
}

float* Scale(char* render)
{
    return &game::Field<float>(render, game::off.renderScale);
}

void* Image(void* checkbox)
{
    void* image = gui::Find(checkbox, "#image");
    return gui::IsA(image, game::globals.staticImageVtbl) ? image : nullptr;
}

float* ImageScale(void* image)
{
    return Scale(&game::Field<char>(image, game::off.staticImageRender));
}

void SetScale(char* render, float x, float y)
{
    float* scale = Scale(render);
    if (scale[0] == x && scale[1] == y)
        return;
    scale[0] = x;
    scale[1] = y;
    game::Field<bool>(render, game::off.renderNeedsUpdate) = true;
}

bool Buttons(void* checkbox)
{
    return Button(checkbox, 0) && Button(checkbox, 1);
}

bool Find(void* screen, void*& reference)
{
    g_inventory = gui::Find(screen, "#inventory_panel");
    g_panel = gui::Find(screen, "#cosmetic_panel");
    g_toggle = gui::Find(screen, "#cosmetic_panel_toggle");
    g_close = gui::Find(screen, "#close_cosmetic_panel");
    if (!g_panel || !g_toggle)
        return false;
    for (size_t i = 0; i < kCount; i++) {
        void* item = gui::Find(g_panel, kPlaces[i].name);
        if (!gui::IsA(item, game::globals.checkboxVtbl) || !Buttons(item))
            return false;
        g_slots[i].item = item;
    }
    reference = nullptr;
    for (const char* name : kReferences) {
        void* item = gui::Find(screen, name);
        if (gui::IsA(item, game::globals.checkboxVtbl) && Buttons(item) && gui::Parent(item) != g_panel) {
            reference = item;
            break;
        }
    }
    return reference != nullptr;
}

void Save(Slot& s)
{
    Saved& v = s.saved;
    memcpy(v.origin, Origin(s.item), sizeof(v.origin));
    memcpy(v.size, Size(s.item), sizeof(v.size));
    for (int b = 0; b < kButtons; b++) {
        memcpy(v.buttonSize[b], Size(Button(s.item, b)), sizeof(v.buttonSize[b]));
        for (int st = 0; st < kStates; st++)
            memcpy(v.scale[b][st], Scale(Render(Button(s.item, b), st)), sizeof(v.scale[b][st]));
    }
    if (void* image = Image(s.item)) {
        memcpy(v.imageOrigin, Origin(image), sizeof(v.imageOrigin));
        memcpy(v.imageScale, ImageScale(image), sizeof(v.imageScale));
    }
    if (void* name = gui::Find(s.item, "#name"))
        memcpy(v.nameOrigin, Origin(name), sizeof(v.nameOrigin));
}

// The box drawn the size of the reference's: each state's quad scaled to the reference's drawn size (size * scale),
// the hit area (m_size) copied, the icon and its label placed and scaled as in the reference.
void Reshape(Slot& s, const Place& p, void* ref)
{
    memcpy(Size(s.item), Size(ref), 2 * sizeof(int));
    for (int b = 0; b < kButtons; b++) {
        void* own = Button(s.item, b);
        void* theirs = Button(ref, b);
        memcpy(Size(own), Size(theirs), 2 * sizeof(int));
        for (int st = 0; st < kStates; st++) {
            char* r = Render(own, st);
            char* rr = Render(theirs, st);
            const float* size = &game::Field<float>(r, game::off.renderSize);
            const float* refSize = &game::Field<float>(rr, game::off.renderSize);
            const float* refScale = Scale(rr);
            if (size[0] > 0 && size[1] > 0 && refSize[0] > 0 && refSize[1] > 0)
                SetScale(r, refSize[0] * refScale[0] / size[0], refSize[1] * refScale[1] / size[1]);
        }
    }
    gui::SetOrigin(s.item, p.x, p.y);
    s.at[0] = p.x;
    s.at[1] = p.y;
    void* image = Image(s.item);
    void* refImage = Image(ref);
    if (image && refImage) {
        const float* scale = ImageScale(refImage);
        SetScale(&game::Field<char>(image, game::off.staticImageRender), scale[0], scale[1]);
        const float* origin = Origin(refImage);
        gui::SetOrigin(image, origin[0], origin[1]);
    }
    void* name = gui::Find(s.item, "#name");
    void* refName = gui::Find(ref, "#name");
    if (name && refName) {
        const float* origin = Origin(refName);
        gui::SetOrigin(name, origin[0], origin[1]);
    }
}

void Restore(Slot& s)
{
    const Saved& v = s.saved;
    memcpy(Size(s.item), v.size, sizeof(v.size));
    for (int b = 0; b < kButtons; b++) {
        memcpy(Size(Button(s.item, b)), v.buttonSize[b], sizeof(v.buttonSize[b]));
        for (int st = 0; st < kStates; st++)
            SetScale(Render(Button(s.item, b), st), v.scale[b][st][0], v.scale[b][st][1]);
    }
    gui::SetOrigin(s.item, v.origin[0], v.origin[1]);
    if (void* image = Image(s.item)) {
        SetScale(&game::Field<char>(image, game::off.staticImageRender), v.imageScale[0], v.imageScale[1]);
        gui::SetOrigin(image, v.imageOrigin[0], v.imageOrigin[1]);
    }
    if (void* name = gui::Find(s.item, "#name"))
        gui::SetOrigin(name, v.nameOrigin[0], v.nameOrigin[1]);
}

// The main page as merged: the second page's panel up, its slots shown (slots.cpp may keep some hidden), the two
// page buttons down.
void ShowMerged()
{
    SetShown(g_panel, true);
    for (Slot& s : g_slots)
        SetShown(s.item, true);
    SetShown(g_toggle, false);
    SetShown(g_close, false);
}

bool Is(const void* item, const void* which)
{
    return item && item == which;
}

// Grouped slots: the visible ones (m_hidden here means slots.cpp hid it, since TT's own Hides are skipped) take the
// group's places in order, so a hidden one leaves no hole.
void Arrange()
{
    for (int group = 1;; group++) {
        size_t members[kCount];
        size_t count = 0;
        for (size_t i = 0; i < kCount; i++)
            if (kPlaces[i].group == group)
                members[count++] = i;
        if (count == 0)
            return;
        size_t next = 0;
        for (size_t k = 0; k < count; k++) {
            Slot& s = g_slots[members[k]];
            if (gui::IsHidden(s.item))
                continue;
            const Place& p = kPlaces[members[next++]];
            if (s.at[0] == p.x && s.at[1] == p.y)
                continue;
            gui::SetOrigin(s.item, p.x, p.y);
            s.at[0] = p.x;
            s.at[1] = p.y;
        }
    }
}

// Each icon scaled to fit its box (as the reference box is drawn, minus room for the label), never above the
// reference icon's scale. The game sets icons keeping the old scale, so this runs after every refresh.
void Fit()
{
    void* refImage = Image(g_reference);
    if (!refImage)
        return;
    char* box = Render(Button(g_reference, 0), 0);
    const float* size = &game::Field<float>(box, game::off.renderSize);
    const float* scale = Scale(box);
    float maxScale = ImageScale(refImage)[0];
    for (Slot& s : g_slots)
        gui::FitImage(Image(s.item), size[0] * scale[0] * 0.85f, size[1] * scale[1] * 0.7f, maxScale);
}

} // namespace

void TtMerge_Apply()
{
    if (!g_settings.ttMerge || !OffsetsOk())
        return;
    void* gameGUI = game::GameGUI();
    void* screen = gameGUI ? game::Field<void*>(gameGUI, game::off.customizationScreen) : nullptr;
    if (!screen || gui::IsHidden(screen))
        return;
    SameRoot();
    if (!g_merged) {
        void* reference = nullptr;
        if (!Find(screen, reference)) {
            if (!g_reported)
                game::api->Log("ttmerge: Trooper Toolbox's second page not found; nothing to merge");
            g_reported = true;
            g_panel = g_toggle = g_close = g_inventory = nullptr;
            for (Slot& s : g_slots)
                s.item = nullptr;
            return;
        }
        for (size_t i = 0; i < kCount; i++) {
            Save(g_slots[i]);
            Reshape(g_slots[i], kPlaces[i], reference);
        }
        g_reference = reference;
        g_merged = true;
        game::api->Log("ttmerge: %s's size for Custom7-9 and SupportGear2/3, now on the main page", gui::Name(reference));
    }
    ShowMerged();
    Fit();
}

void TtMerge_Arrange()
{
    if (g_merged && SameRoot())
        Arrange();
}

void TtMerge_OnShow(const void* item)
{
    if (g_merged && !g_internal && Is(item, g_inventory) && SameRoot())
        Fit(); // a list closed (TT's GUI_CUSTOMIZE_SHOW_DEFAULT): an equip may have changed an icon
}

bool TtMerge_SkipShow(const void* item)
{
    if (!g_merged || g_internal || !SameRoot())
        return false;
    return Is(item, g_toggle) || Is(item, g_close);
}

bool TtMerge_SkipHide(const void* item)
{
    if (!g_merged || g_internal || !SameRoot())
        return false;
    if (Is(item, g_panel))
        return true;
    for (const Slot& s : g_slots)
        if (Is(item, s.item))
            return true;
    return false;
}

void TtMerge_SetEnabled(bool on)
{
    if (on) {
        TtMerge_Apply(); // if the screen is up; else the next trooper select
        return;
    }
    if (!g_merged || !SameRoot())
        return;
    for (Slot& s : g_slots) {
        Restore(s);
        SetShown(s.item, false); // TT's main page: second-page slots hidden
    }
    SetShown(g_close, false);
    SetShown(g_toggle, true);
    g_merged = false;
    game::api->Log("ttmerge: off, Trooper Toolbox's second page is back");
}
