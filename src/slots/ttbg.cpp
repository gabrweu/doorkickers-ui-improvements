// Trooper Toolbox paints its main page's slot frames into its background texture (customization_menu_bg.tga; the slot
// boxes themselves are frameless translucent fills). So a slot slots.cpp hides leaves an empty frame behind, and the
// slots ttmerge.cpp brings over from the second page sit where the art has none. While merged, TT's background image
// is hidden and ours (#dk2ml_tt_bg, gui/tt_background.xml: TT's art with the frames painted out) shown in its place,
// and every slot gets a frame of its own, as its child: a hidden slot takes its frame with it.
//
// Ours goes into TT's customization_bg_image, next to TT's image, so TT's Show/Hide of that item (a black background
// while an equipment list is open) applies to it. A frame is a clone of a template with four bars, stretched to the
// slot's drawn box; the merged slots are framed after ttmerge.cpp resized them, and SupportGear2/3's moves (Arrange)
// carry their frames along.
#include "slots/ttbg.h"

#include <cstring>

#include "core/game.h"
#include "core/gui.h"

namespace {

constexpr const char* kTtBackground = "customization_bg_image"; // TT's item; its StaticImage child is the texture
constexpr const char* kBackground = "#dk2ml_tt_bg";
constexpr const char* kFrame = "#dk2ml_tt_frame";
constexpr const char* kFrameDark = "#dk2ml_tt_frame_dark";
constexpr const char* kBars[] = {"#dk2ml_tt_frame_t", "#dk2ml_tt_frame_b", "#dk2ml_tt_frame_l", "#dk2ml_tt_frame_r"};

// the slots TT frames on its main page, then the merged ones
constexpr const char* kFramed[] = {"PrimaryWeapon", "SecondaryWeapon", "Helmet", "Armor", "UtilityPouch1",
                                   "UtilityPouch2", "SupportGear1", "Custom1", "Custom2", "Custom3", "Custom4",
                                   "Custom5", "Custom6", "Custom7", "Custom8", "Custom9", "SupportGear2", "SupportGear3"};
constexpr size_t kCount = sizeof(kFramed) / sizeof(kFramed[0]);
constexpr const char* kDark = "Armor"; // on the figure's cream plate carrier

constexpr float kThick = 4;  // bar thickness
constexpr float kMargin = 4; // the slot textures' transparent border, in texture pixels

void* g_ttImage = nullptr; // TT's background image (hidden while ours is up)
void* g_ours = nullptr;
void* g_frames[kCount] = {};
bool g_ready = false;    // set up for this GUI
bool g_failed = false;   // couldn't be, for this GUI (logged once)
bool g_shown = false;    // ours replaces TT's

// A slot's box as drawn: its unchecked Button's normal render object, size x scale, less the texture's border.
bool Box(void* checkbox, float& w, float& h)
{
    void* button = dk2ml::At<void*>(checkbox, game::GUI_Checkbox_m_pButtons.Offset());
    if (!button)
        return false;
    char* render = &dk2ml::At<char>(button, game::GUI_Button_m_renderObjects.Offset());
    const float* size = &dk2ml::At<float>(render, game::RenderObject2D_size.Offset());
    const float* scale = &dk2ml::At<float>(render, game::RenderObject2D_scale.Offset());
    w = (size[0] - 2 * kMargin) * scale[0];
    h = (size[1] - 2 * kMargin) * scale[1];
    return w > 2 * kThick && h > 2 * kThick;
}

// The four bars along the inside of a w x h box centered on the frame (+y up).
void Shape(void* frame, float w, float h)
{
    void* bars[4];
    for (int i = 0; i < 4; i++)
        bars[i] = gui::Find(frame, kBars[i]);
    gui::StretchTo(bars[0], w, kThick);
    gui::SetOrigin(bars[0], 0, (h - kThick) / 2);
    gui::StretchTo(bars[1], w, kThick);
    gui::SetOrigin(bars[1], 0, -(h - kThick) / 2);
    gui::StretchTo(bars[2], kThick, h);
    gui::SetOrigin(bars[2], -(w - kThick) / 2, 0);
    gui::StretchTo(bars[3], kThick, h);
    gui::SetOrigin(bars[3], (w - kThick) / 2, 0);
}

// what the game does for its own cloned list items (as cards.cpp does): re-resolve action targets, drop the name
void* Clone(void* tmpl)
{
    void* clone = game::GUI_StaticImage_CloneInternal(tmpl);
    if (!clone)
        return nullptr;
    game::GUI_Item_UpdatePointerReferences(clone, clone, game::GuiRoot());
    game::GUI_Item_SetName(clone, nullptr);
    return clone;
}

bool Fail(const char* why)
{
    if (!g_failed)
        game::api->Log("ttbg: %s; Trooper Toolbox's own background stays", why);
    g_failed = true;
    return false;
}

bool Setup(void* screen, void* inventory)
{
    if (!game::GUI_StaticImage_CloneInternal.Resolved())
        return Fail("no StaticImage::CloneInternal");
    void* ttItem = gui::Find(screen, kTtBackground);
    void* ttImage = nullptr;
    for (void* child : gui::Children(ttItem))
        if (gui::IsA(child, game::GUI_StaticImage_vftable.Address())) {
            ttImage = child;
            break;
        }
    if (!ttImage || !inventory)
        return Fail("TT's customization_bg_image or #inventory_panel not found");
    void* root = game::GuiRoot();
    void* ours = gui::Find(root, kBackground);
    void* light = gui::Find(root, kFrame);
    void* dark = gui::Find(root, kFrameDark);
    if (!gui::IsA(ours, game::GUI_StaticImage_vftable.Address()) || !gui::IsA(light, game::GUI_StaticImage_vftable.Address()) ||
        !gui::IsA(dark, game::GUI_StaticImage_vftable.Address()))
        return Fail("gui/tt_background.xml's items not found");

    int framed = 0;
    for (size_t i = 0; i < kCount; i++) {
        void* slot = gui::Find(inventory, kFramed[i]);
        float w, h;
        if (!gui::IsA(slot, game::GUI_Checkbox_vftable.Address()) || !Box(slot, w, h))
            continue;
        void* frame = Clone(strcmp(kFramed[i], kDark) == 0 ? dark : light);
        if (!frame)
            continue;
        gui::AddChild(slot, frame); // last: drawn over the box's fill
        gui::SetOrigin(frame, 0, 0);
        Shape(frame, w, h);
        g_frames[i] = frame;
        framed++;
    }
    if (framed == 0)
        return Fail("no slot to frame"); // keep TT's frames rather than none
    if (gui::Parent(ours) != ttItem)
        gui::AddChild(ttItem, ours); // last: hit-tested first, as TT's image was
    g_ttImage = ttImage;
    g_ours = ours;
    game::api->Log("ttbg: our background in place of Trooper Toolbox's, %d slots framed", framed);
    return true;
}

} // namespace

void TtBg_Apply(void* screen, void* inventory)
{
    if (!g_ready) {
        if (g_failed || !Setup(screen, inventory))
            return;
        g_ready = true;
    }
    g_shown = true;
    gui::ShowIf(g_ttImage, false);
    gui::ShowIf(g_ours, true);
    for (void* frame : g_frames)
        gui::ShowIf(frame, true);
}

void TtBg_Remove()
{
    if (!g_ready)
        return;
    g_shown = false;
    for (void* frame : g_frames)
        gui::ShowIf(frame, false);
    gui::ShowIf(g_ours, false);
    gui::ShowIf(g_ttImage, true);
}

bool TtBg_SkipShow(const void* item)
{
    return g_shown && item && item == g_ttImage;
}

void TtBg_OnGuiLoaded()
{
    g_ttImage = g_ours = nullptr;
    for (void*& frame : g_frames)
        frame = nullptr;
    g_ready = g_failed = g_shown = false;
}
