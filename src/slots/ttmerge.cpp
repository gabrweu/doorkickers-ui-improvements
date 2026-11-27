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
//
// TT's credits are baked into the second page's background, which nobody sees while merged, so our CREDITS button
// (gui/tt_credits.xml, its pop-up shown on hover from XML) is moved into #inventory_panel at the spot of the hidden
// toggle and shown while merged.
//
// TT's main-page slot frames are baked into its background, so ttbg.cpp swaps it for ours and frames each slot itself
// while merged.
#include "slots/ttmerge.h"

#include <cstring>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"
#include "slots/ttbg.h"

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

constexpr const char* kCredits = "#dk2ml_tt_credits"; // our CREDITS button (optional)
constexpr const char* kCreditsPopup = "#dk2ml_tt_credits_popup";

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
void* g_inventory = nullptr; // #inventory_panel (the main page; TT shows it again when a list closes)
void* g_panel = nullptr;   // #cosmetic_panel
void* g_reference = nullptr; // the main-page box the merged ones copy (Custom3)
void* g_toggle = nullptr;  // #cosmetic_panel_toggle
void* g_close = nullptr;   // #close_cosmetic_panel
void* g_credits = nullptr; // our CREDITS button, moved into #inventory_panel
bool g_merged = false;
bool g_internal = false;   // our own Show/Hide calls
bool g_reported = false;   // "not found" logged for this GUI

bool OffsetsOk()
{
    int32_t buttons = game::GUI_Checkbox_m_pButtons.Offset();
    uint32_t renderObjectSize = game::sizeof_RenderObject2D.Get();
    return buttons > 0 && buttons % 8 == 0 && game::GUI_Button_m_renderObjects.Offset() > 0 && renderObjectSize >= 64 &&
           renderObjectSize <= 1024;
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
    bool aligned = dk2ml::At<unsigned>(item, game::GUI_Item_m_alignFlags.Offset()) != 0;
    return &dk2ml::At<float>(item, aligned ? game::GUI_Item_m_alignOffset.Offset() : game::GUI_Item_m_localOrigin.Offset());
}

int* Size(void* item)
{
    return &dk2ml::At<int>(item, game::GUI_Item_m_size.Offset());
}

void* Button(void* checkbox, int i)
{
    return dk2ml::At<void*>(checkbox, game::GUI_Checkbox_m_pButtons.Offset() + i * static_cast<int32_t>(sizeof(void*)));
}

char* Render(void* button, int state)
{
    return &dk2ml::At<char>(button, game::GUI_Button_m_renderObjects.Offset() + state * static_cast<int32_t>(game::sizeof_RenderObject2D.Get()));
}

float* Scale(char* render)
{
    return &dk2ml::At<float>(render, game::RenderObject2D_scale.Offset());
}

void* Image(void* checkbox)
{
    void* image = gui::Find(checkbox, "#image");
    return gui::IsA(image, game::GUI_StaticImage_vftable.Address()) ? image : nullptr;
}

float* ImageScale(void* image)
{
    return Scale(&dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset()));
}

void SetScale(char* render, float x, float y)
{
    float* scale = Scale(render);
    if (scale[0] == x && scale[1] == y)
        return;
    scale[0] = x;
    scale[1] = y;
    dk2ml::At<bool>(render, game::RenderObject2D_bNeedsUpdate.Offset()) = true;
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
        if (!gui::IsA(item, game::GUI_Checkbox_vftable.Address()) || !Buttons(item))
            return false;
        g_slots[i].item = item;
    }
    reference = nullptr;
    for (const char* name : kReferences) {
        void* item = gui::Find(screen, name);
        if (gui::IsA(item, game::GUI_Checkbox_vftable.Address()) && Buttons(item) && gui::Parent(item) != g_panel) {
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
            const float* size = &dk2ml::At<float>(r, game::RenderObject2D_size.Offset());
            const float* refSize = &dk2ml::At<float>(rr, game::RenderObject2D_size.Offset());
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
        SetScale(&dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset()), scale[0], scale[1]);
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
        SetScale(&dk2ml::At<char>(image, game::GUI_StaticImage_m_renderObject.Offset()), v.imageScale[0], v.imageScale[1]);
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
    SetShown(g_credits, true);
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
    const float* size = &dk2ml::At<float>(box, game::RenderObject2D_size.Offset());
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
    void* screen = gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizationScreen.Offset()) : nullptr;
    if (!screen || gui::IsHidden(screen))
        return;
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
        // the credits button, at the toggle's spot in the toggle's parent; last child, so it and its pop-up draw on top
        g_credits = gui::Find(game::GuiRoot(), kCredits);
        if (g_credits && g_inventory) {
            if (gui::Parent(g_credits) != g_inventory)
                gui::AddChild(g_inventory, g_credits);
        } else {
            game::api->Log("ttmerge: no %s (gui/tt_credits.xml); TT's credits stay on its second page", kCredits);
            g_credits = nullptr;
        }
        g_reference = reference;
        g_merged = true;
        game::api->Log("ttmerge: %s's size for Custom7-9 and SupportGear2/3, now on the main page", gui::Name(reference));
    }
    ShowMerged();
    TtBg_Apply(screen, g_inventory); // after Reshape: the merged slots' frames take their new size
    Fit();
}

void TtMerge_Arrange()
{
    if (g_merged)
        Arrange();
}

void TtMerge_OnShow(const void* item)
{
    if (g_merged && !g_internal && Is(item, g_inventory))
        Fit(); // a list closed (TT's GUI_CUSTOMIZE_SHOW_DEFAULT): an equip may have changed an icon
}

bool TtMerge_SkipShow(const void* item)
{
    if (!g_merged || g_internal)
        return false;
    return Is(item, g_toggle) || Is(item, g_close) || TtBg_SkipShow(item);
}

bool TtMerge_SkipHide(const void* item)
{
    if (!g_merged || g_internal)
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
    if (!g_merged)
        return;
    for (Slot& s : g_slots) {
        Restore(s);
        SetShown(s.item, false); // TT's main page: second-page slots hidden
    }
    SetShown(g_close, false);
    SetShown(g_toggle, true);
    SetShown(gui::Find(g_credits, kCreditsPopup), false);
    SetShown(g_credits, false);
    TtBg_Remove();
    g_merged = false;
    game::api->Log("ttmerge: off, Trooper Toolbox's second page is back");
}

void TtMerge_OnGuiLoaded()
{
    // The game reloaded its GUI: the old items are gone (nothing to restore), and their addresses may be reused, so
    // forget them; the next TtMerge_Apply merges the new ones.
    g_merged = false;
    g_reported = false;
    g_panel = g_toggle = g_close = g_inventory = g_reference = g_credits = nullptr;
    for (Slot& s : g_slots)
        s.item = nullptr;
    TtBg_OnGuiLoaded();
}
