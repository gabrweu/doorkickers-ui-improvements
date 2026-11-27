// The mission action wheel on one page. Right-clicking a trooper or a waypoint opens #waypoint_menu
// (data/gui/waypoints.xml), built by GameGUI::OpenWaypointMenu(wp, type). With a target nearby (door, window, enemy,
// hostage, ...) it has two pages: the target's actions and the path's (reload, switch weapon, sprint, crouch,
// grenades, delete path, go-codes, the top toggles). A "more" button (#switch_to_path_options /
// #switch_to_target_options) flips between them by rebuilding the wheel (WaypointMenu_OnEvent 82 / 83).
//
// Each build deletes every child of #menu_parent (the action buttons, clones of #action_template), hides the static
// buttons, then clones a button per action and lays them out on one circle (radius = #menu_ring's width 256 x 0.73;
// general actions centred at the top, equipment at the bottom), shows the static buttons it used and the right "more"
// button, and places the menu at the waypoint. Show on the already shown menu does nothing, so building again doesn't
// fire its OnOpen (GUI_GAME_PAUSE) twice.
//
// After an open with a "more" button showing, this builds the target page (the open's own, or a TARGETED build),
// moves its buttons out of #menu_parent into our hidden #dk2ml_wm_hold so the next build doesn't delete them, builds
// the path page (PATH: last, so the static buttons, go-codes and toggles end as stock has them), and moves the target
// buttons back into #menu_parent (the game deletes them on the next open, as its own) on an outer ring. Both "more"
// buttons are hidden, and #dk2ml_wm_outer (gui/action_wheel.xml: a larger ring and a click catcher) shows behind it.
//
// A button carries its action type and inventory slot (the clone's TriggerEvent action), but not the target: the
// open stores the target entity id in a static the menu's user data points at (0 on the path page), and
// GUI_GAME_WAYPOINT_CLICKED passes it to ActionWP_Client::OnChangeAction. So a click on a target button writes the
// target page's id back there first (Wheel_OnEvent).
//
// Both pages have an equipment arc (grenades, launchers, ...): the path page's uses the item with no target (thrown at
// a spot), the target page's uses it on the target (the game asks the target per item, so some are greyed out there).
// Stock never shows both; merged, both stay (they do different things): each outer one sits right outside its inner
// twin, at the same angle, and gets our door badge (#dk2ml_wm_badge, cloned into it; the game deletes it with the
// button). Two buttons are for the same item when their click action carries the same inventory slot (ClickOf).
//
// The general actions don't always differ: the human-target builder (0x1400FCB40 in build 112) falls back to the path
// page's general actions (0x1400FC020) when the human isn't a usable target (downed, too far, ...), so that target page
// repeats the path page's stance, speed, check corners... An outer general button whose click (action type and slot)
// a shown inner button already has is hidden (HideRepeats); the inner copy, the path page's, stays.
//
// The rings are ours while the setting is on, on every wheel: the stock ring (#menu_ring and its mirror: a dashed and a
// dotted line) is hidden for #dk2ml_wm_inner (one dotted line), and the outer ring is the same art, larger; both a
// little darker than stock so the buttons stand out (gui/action_wheel.xml).
#include "hud/wheel.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"

namespace {

constexpr const char* kMenuParent = "#menu_parent";
constexpr const char* kToPath = "#switch_to_path_options";
constexpr const char* kToTarget = "#switch_to_target_options";
constexpr const char* kEquipmentHover = "#hover_eqp"; // shown on a button made for an equipment item
constexpr const char* kHold = "#dk2ml_wm_hold";
constexpr const char* kOuter = "#dk2ml_wm_outer";
constexpr const char* kBadge = "#dk2ml_wm_badge";
constexpr const char* kInner = "#dk2ml_wm_inner";
constexpr const char* kStockRing = "#menu_ring"; // the stock ring's left half; its mirrored right half is unnamed
constexpr float kBadgeX = 40, kBadgeY = 40; // top right of the 120 px button (its count is bottom right)
constexpr float kInnerRadius = 187; // the game's: #menu_ring's width (256) x 0.73, if no button tells us
constexpr float kButton = 120;      // the action buttons' size, which also sets the game's spacing (step = 120 / radius)
constexpr float kRingGap = 128;     // outer radius = inner + a button + a little
constexpr float kPi = 3.14159265f;

struct Vec {
    float x, y;
};

// what a button's click does: GUI_GAME_WAYPOINT_CLICKED with its action type and inventory slot
struct Click {
    bool known = false; // false: no such click action, or it can't be read
    int type = 0;
    int slot = 0; // 0 = no item
};

// a target-page button, kept while the path page is built
struct Kept {
    void* item;
    Vec origin;     // where the game put it (its angle gives the order)
    bool equipment; // grenades, charges, ...: the bottom arc, as stock; the rest at the top
    Click click;
};

// an inner (path page) equipment button's slot and angle: an outer button for the same slot goes right outside it
struct Twin {
    int slot;
    float angle;
    bool taken = false;
};

bool g_stale = true; // our items must be found (again): at first, and after every GUI reload
void* g_hold = nullptr;
void* g_outer = nullptr;
void* g_badge = nullptr; // the template
bool g_loggedNoBadge = false;
void* g_inner = nullptr;
bool g_ringsPlaced = false; // our rings moved into this GUI's #waypoint_menu, the stock ring's images found
std::vector<void*> g_stockRing; // #menu_ring and its mirror (hidden while ours shows)
bool g_loggedNoRing = false;
bool g_loggedMissing = false;
bool g_loggedMerge = false;
bool g_building = false; // our own OpenWaypointMenu calls: the hook leaves them alone
std::vector<void*> g_targetButtons; // the merged wheel's target-page buttons (only compared, never dereferenced)
unsigned g_target = 0;              // the target id their page was built with

void* Menu()
{
    void* gameGUI = game::GameGUI();
    return gameGUI ? game::GameGUI_m_pWaypointMenu(gameGUI) : nullptr;
}

// The static unsigned the open stored its target id in (the menu's user data), or null.
unsigned* TargetSlot(void* menu)
{
    return static_cast<unsigned*>(dk2ml::At<void*>(menu, game::GUI_Item_m_userData.Offset() + game::GUI_sUserData_ptr.Offset()));
}

Vec Origin(void* item)
{
    float* o = &dk2ml::At<float>(item, game::GUI_Item_m_localOrigin.Offset());
    return {o[0], o[1]};
}

void Ready()
{
    if (!g_stale)
        return;
    void* root = game::GuiRoot();
    if (!root)
        return;
    g_stale = false;
    g_hold = gui::Find(root, kHold);
    g_outer = gui::Find(root, kOuter);
    g_inner = gui::Find(root, kInner);
    g_badge = gui::Find(root, kBadge);
    if (!gui::IsA(g_badge, game::GUI_StaticImage_vftable.Address()) || !game::GUI_StaticImage_CloneInternal.Resolved())
        g_badge = nullptr; // cloned with StaticImage::CloneInternal
    if ((!g_hold || !g_outer) && !g_loggedMissing) {
        game::api->Log("gui/action_wheel.xml: %s%s%s not found%s", g_hold ? "" : kHold, !g_hold && !g_outer ? ", " : "",
                       g_outer ? "" : kOuter, g_hold ? "; the merged wheel has no outer ring backdrop" : "; the action wheel keeps its pages");
        g_loggedMissing = true;
    }
}

void Build(const void* wp, int64_t type)
{
    g_building = true;
    game::GameGUI_OpenWaypointMenu(game::GameGUI(), wp, static_cast<int>(type));
    g_building = false;
}

bool IsEquipment(void* button, Vec origin)
{
    void* hover = gui::Find(button, kEquipmentHover);
    return hover ? !gui::IsHidden(hover) : origin.y < 0;
}

// The action type and inventory slot a wheel button uses: the game's button helper writes them into the iParam1 /
// iParam2 of each click action whose eventId is GUI_GAME_WAYPOINT_CLICKED.
Click ClickOf(void* button)
{
    int64_t click = game::EVENT_CLICK.Get();
    if (!game::WheelButtonSlotsUsable() || click < 0 || click > 31)
        return {};
    uint8_t* actions = &dk2ml::At<uint8_t>(button, game::GUI_Item_m_eventProperties.Offset() +
                                                       static_cast<int32_t>(click * game::sizeof_ItemEventProperties.Get()) +
                                                       game::ItemEventProperties_actions.Offset());
    auto** list = static_cast<void**>(dk2ml::At<void*>(actions, game::List_ActionPtr_m_list.Offset()));
    int count = dk2ml::At<int>(actions, game::List_ActionPtr_m_elements.Offset());
    if (!list || count <= 0 || count > 16) // the template has 2 click actions
        return {};
    int32_t params = game::GUI_sAction_eventParams.Offset();
    for (int i = 0; i < count; i++)
        if (void* action = list[i]; action && dk2ml::At<int>(action, game::GUI_sAction_eventId.Offset()) == game::GUI_GAME_WAYPOINT_CLICKED.Get())
            return {true, dk2ml::At<int>(action, params + game::GUI_sEventParams_iParam1.Offset()),
                    dk2ml::At<int>(action, params + game::GUI_sEventParams_iParam2.Offset())};
    return {};
}

// Hides the outer general buttons whose click a shown inner (path page) button already has, and takes them out of
// `top`. They stay in #menu_parent, so the game deletes them on the next open. Returns how many were hidden.
int HideRepeats(void* menuParent, std::vector<Kept*>& top)
{
    std::vector<Click> inner;
    for (void* c : gui::Children(menuParent))
        if (!gui::IsHidden(c))
            if (Click k = ClickOf(c); k.known)
                inner.push_back(k);
    std::vector<Kept*> rest;
    int hidden = 0;
    for (Kept* k : top) {
        bool repeat = k->click.known && std::any_of(inner.begin(), inner.end(), [k](const Click& c) {
            return c.type == k->click.type && c.slot == k->click.slot;
        });
        if (repeat) {
            gui::ShowIf(k->item, false);
            hidden++;
        } else {
            rest.push_back(k);
        }
    }
    top = std::move(rest);
    return hidden;
}

// The path page's ring: how far its buttons are from the centre.
float InnerRadius(void* menuParent)
{
    float radius = 0;
    for (void* c : gui::Children(menuParent)) {
        Vec o = Origin(c);
        radius = std::max(radius, std::sqrt(o.x * o.x + o.y * o.y));
    }
    return radius > kButton / 2 ? radius : kInnerRadius;
}

// angle - centre, in (-pi, pi]
float Delta(float angle, float centre)
{
    float d = std::fmod(angle - centre, 2 * kPi);
    if (d > kPi)
        d -= 2 * kPi;
    if (d <= -kPi)
        d += 2 * kPi;
    return d;
}

// Lays an arc out the game's way at `radius`: buttons a button's width apart, centred on `centre`, in the order the
// game had them.
void Arc(std::vector<Kept*>& arc, float centre, float radius)
{
    std::sort(arc.begin(), arc.end(), [centre](const Kept* a, const Kept* b) {
        return Delta(std::atan2(a->origin.y, a->origin.x), centre) < Delta(std::atan2(b->origin.y, b->origin.x), centre);
    });
    float step = kButton / radius;
    float start = centre + step / 2 - static_cast<float>(arc.size()) * step / 2;
    for (size_t i = 0; i < arc.size(); i++) {
        float a = start + static_cast<float>(i) * step;
        gui::SetOrigin(arc[i]->item, radius * std::cos(a), radius * std::sin(a));
    }
}

// The path page's equipment buttons on the inner ring's bottom arc (only equipment is there), by slot and angle.
std::vector<Twin> InnerTwins(void* menuParent)
{
    std::vector<Twin> twins;
    for (void* c : gui::Children(menuParent)) {
        Vec o = Origin(c);
        if (o.y >= 0 || gui::IsHidden(c) || !IsEquipment(c, o))
            continue;
        if (int slot = ClickOf(c).slot)
            twins.push_back({slot, 3 * kPi / 2 + Delta(std::atan2(o.y, o.x), 3 * kPi / 2)});
    }
    return twins;
}

// Gives a button our door badge: a clone of the template, inside the button (so the game deletes it with it).
void AddBadge(void* button)
{
    if (!g_badge) {
        if (!g_loggedNoBadge) {
            game::api->Log("action wheel: no %s (gui/action_wheel.xml or StaticImage::CloneInternal missing): no door badges", kBadge);
            g_loggedNoBadge = true;
        }
        return;
    }
    void* clone = game::GUI_StaticImage_CloneInternal(g_badge);
    if (!clone)
        return;
    game::GUI_Item_UpdatePointerReferences(clone, clone, game::GuiRoot());
    game::GUI_Item_SetName(clone, nullptr);
    gui::AddChild(button, clone);
    gui::SetOrigin(clone, kBadgeX, kBadgeY);
    gui::ShowIf(clone, true);
}

// Lays the outer bottom arc out at `radius`: a button whose item has an inner twin right outside it, at its angle,
// with our badge; the others in the game's order, at the outer step, those the game had before the first twinned one
// to the left of the twinned ones, the rest to the right. Without any twin, the game's centred arc. Returns how many
// were twinned.
int PairedArc(std::vector<Kept*>& arc, std::vector<Twin>& twins, float radius)
{
    const float centre = 3 * kPi / 2;
    std::sort(arc.begin(), arc.end(), [centre](const Kept* a, const Kept* b) {
        return Delta(std::atan2(a->origin.y, a->origin.x), centre) < Delta(std::atan2(b->origin.y, b->origin.x), centre);
    });
    std::vector<float> angle(arc.size(), 0);
    std::vector<bool> twinned(arc.size(), false);
    int paired = 0;
    size_t first = arc.size();
    float lo = 0, hi = 0;
    for (size_t i = 0; i < arc.size(); i++) {
        if (arc[i]->click.slot == 0)
            continue;
        for (Twin& t : twins)
            if (!t.taken && t.slot == arc[i]->click.slot) {
                t.taken = true;
                twinned[i] = true;
                angle[i] = t.angle;
                lo = paired ? std::min(lo, t.angle) : t.angle;
                hi = paired ? std::max(hi, t.angle) : t.angle;
                first = std::min(first, i);
                paired++;
                break;
            }
    }
    if (paired == 0) {
        Arc(arc, centre, radius);
        return 0;
    }
    float step = kButton / radius;
    // (angles grow left to right along the bottom: 180 degrees is the left, 270 the bottom)
    for (size_t i = 0; i < first; i++) // to the left of the twinned ones, keeping their order
        angle[i] = lo - step * static_cast<float>(first - i);
    int after = 0;
    for (size_t i = first; i < arc.size(); i++)
        if (!twinned[i])
            angle[i] = hi + step * static_cast<float>(++after);
    for (size_t i = 0; i < arc.size(); i++) {
        gui::SetOrigin(arc[i]->item, radius * std::cos(angle[i]), radius * std::sin(angle[i]));
        if (twinned[i])
            AddBadge(arc[i]->item);
    }
    return paired;
}

unsigned TextureOf(void* staticImage)
{
    return dk2ml::At<unsigned>(staticImage, game::GUI_StaticImage_m_renderObject.Offset() + game::RenderObject2D_texture.Offset());
}

// Once per GUI: finds the stock ring's images (#menu_ring and the StaticImage beside it with the same texture, its
// flipped half) and puts #dk2ml_wm_outer, then #dk2ml_wm_inner, right after them among #waypoint_menu's children:
// where the stock ring is drawn, over the stock shadows (two multiply_inverse images over the middle 640x640, which
// would darken our rings, the inner one most) and under everything else. Children are hit-tested last to first, so the
// stock 360 catcher, the portrait and every button still win over the outer ring's click catcher; what's before it
// (shadows, stock ring) are StaticImages, which never take the cursor. Without the stock ring: our rings go first.
void PlaceRings(void* menu)
{
    if (g_ringsPlaced)
        return;
    g_ringsPlaced = true;
    std::vector<void*> children = gui::Children(menu);

    g_stockRing.clear();
    void* stock = gui::Find(menu, kStockRing);
    const void* imageVtbl = game::GUI_StaticImage_vftable.Address();
    if (stock && gui::IsA(stock, imageVtbl) && game::RenderObject2D_texture.Resolved()) {
        unsigned texture = TextureOf(stock);
        for (void* c : children)
            if (gui::IsA(c, imageVtbl) && TextureOf(c) == texture)
                g_stockRing.push_back(c); // #menu_ring itself too
    }

    void* after = g_stockRing.empty() ? nullptr : g_stockRing.back(); // the last stock ring image, in drawing order
    auto addRings = [menu] {
        for (void* ring : {g_outer, g_inner})
            if (ring)
                gui::AddChild(menu, ring);
    };
    if (!after)
        addRings();
    for (void* c : children) {
        if (c == g_outer || c == g_inner)
            continue;
        gui::AddChild(menu, c);
        if (c == after)
            addRings();
    }
    if ((!g_inner || g_stockRing.empty()) && !g_loggedNoRing) {
        game::api->Log("action wheel: %s; the stock inner ring stays", !g_inner ? "no #dk2ml_wm_inner (gui/action_wheel.xml)"
                                                                              : "the stock ring's images (#menu_ring) not found");
        g_loggedNoRing = true;
    }
}

// Our dotted inner ring instead of the stock one (setting on), or the stock one back (off). The game never shows or
// hides the stock ring's images itself (it only reads #menu_ring's texture size for its radius).
void UseOurInnerRing(void* menu, bool ours)
{
    if (ours)
        PlaceRings(menu);
    bool swap = ours && g_inner && !g_stockRing.empty();
    for (void* image : g_stockRing)
        gui::ShowIf(image, !swap);
    gui::ShowIf(g_inner, swap);
}

float Clamp(float v, float limit)
{
    return limit <= 0 ? 0 : std::clamp(v, -limit, limit);
}

// The merged wheel is about 760 across: keep it on screen (the game centres it on the waypoint, partly off screen
// near an edge). Its origin is relative to the GUI's centre, as the game computes it.
void KeepOnScreen(void* menu, float reach)
{
    void* manager = *game::g_pGUIManager;
    if (!manager || dk2ml::At<int>(menu, game::GUI_Item_m_alignFlags.Offset()) != 0)
        return;
    float w = static_cast<float>(game::GUIManager_m_width(manager)), h = static_cast<float>(game::GUIManager_m_height(manager));
    Vec o = Origin(menu);
    float x = Clamp(o.x, w / 2 - reach), y = Clamp(o.y, h / 2 - reach);
    if (x != o.x || y != o.y)
        gui::SetOrigin(menu, x, y);
}

} // namespace

void Wheel_OnOpened(const void* wp, int type)
{
    (void)type;
    if (g_building)
        return;
    g_targetButtons.clear();
    void* menu = Menu();
    if (!menu)
        return;
    Ready();
    gui::ShowIf(g_outer, false);
    UseOurInnerRing(menu, g_settings.wheel);
    if (!g_settings.wheel || !g_hold)
        return;
    void* toPath = gui::Find(menu, kToPath);
    void* toTarget = gui::Find(menu, kToTarget);
    bool targetPage = toPath && !gui::IsHidden(toPath);       // the path page is a click away
    bool pathPage = !targetPage && toTarget && !gui::IsHidden(toTarget); // the target page is
    void* menuParent = gui::Find(menu, kMenuParent);
    unsigned* slot = TargetSlot(menu);
    if ((!targetPage && !pathPage) || !menuParent || !slot)
        return;

    if (pathPage)
        Build(wp, game::OPEN_WAYPOINTS_TARGETED.Get()); // what the "more" button would show
    unsigned target = *slot;
    std::vector<Kept> kept;
    for (void* c : gui::Children(menuParent)) {
        Vec o = Origin(c);
        kept.push_back({c, o, IsEquipment(c, o), ClickOf(c)});
        gui::AddChild(g_hold, c);
    }
    Build(wp, game::OPEN_WAYPOINTS_PATH.Get());

    float inner = InnerRadius(menuParent);
    float outer = inner + kRingGap;
    std::vector<Twin> twins = InnerTwins(menuParent);
    std::vector<Kept*> top, bottom;
    for (Kept& k : kept)
        (k.equipment ? bottom : top).push_back(&k);
    int repeats = HideRepeats(menuParent, top); // (#menu_parent holds only the path page's buttons yet)
    for (Kept& k : kept) {
        gui::AddChild(menuParent, k.item); // the hidden repeats too: the game deletes them on the next open
        g_targetButtons.push_back(k.item);
    }
    if (top.empty() && bottom.empty())
        return; // nothing on the target page after all: the path page as the game built it
    Arc(top, kPi / 2, outer);
    int paired = PairedArc(bottom, twins, outer);
    g_target = target;

    gui::ShowIf(gui::Find(menu, kToPath), false);
    gui::ShowIf(gui::Find(menu, kToTarget), false);
    gui::ShowIf(g_outer, true);
    KeepOnScreen(menu, outer + kButton / 2 + 8);
    if (!g_loggedMerge) {
        game::api->Log("action wheel: one page (%zu target actions on a ring of radius %.0f, %zu at the top, target %u; "
                       "%d paired with their inner twin, %d repeats of path actions hidden)",
                       kept.size(), outer, top.size(), target, paired, repeats);
        g_loggedMerge = true;
    }
}

void Wheel_OnEvent(int eventId, const void* params)
{
    if (eventId != game::GUI_GAME_WAYPOINT_CLICKED.Get() || g_targetButtons.empty() || !params)
        return;
    void* owner = dk2ml::At<void*>(const_cast<void*>(params), game::GUI_sEventParams_owner.Offset());
    if (std::find(g_targetButtons.begin(), g_targetButtons.end(), owner) == g_targetButtons.end())
        return;
    void* menu = Menu();
    if (unsigned* slot = menu ? TargetSlot(menu) : nullptr)
        *slot = g_target; // the path page, built last, left its own (0)
}

void Wheel_OnGuiLoaded()
{
    g_stale = true;
    g_hold = nullptr;
    g_outer = nullptr;
    g_inner = nullptr;
    g_ringsPlaced = false;
    g_stockRing.clear();
    g_loggedMerge = false;
    g_targetButtons.clear();
}
