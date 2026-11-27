// Loadouts panel (gui/loadout_panel.xml), left of the Customization screen: "MY LOADOUTS", the operator's own, and
// "TEAM LOADOUTS", the squad's, 5 profiles each. A profile is the whole loadout (every slot). A row shows its name, or
// its weapons when it has none; clicking it equips all of it (store.cpp's Write, as Paste loadout does, skipping what
// this operator's class can't use), its SAVE button stores everything the operator has equipped now. Team profiles are
// one set per squad, so a save on one operator shows on every squadmate's screen. Hovering a row shows a pop-up
// (#dk2ml_lo_preview) with everything in it by GUNS / GEAR / LOOKS: weapons with names, the rest as icons.
//
// After MY LOADOUTS' rows, a row that can't be saved: the class's starting loadout (store.cpp's StartingLoadout),
// loaded like a profile; what it doesn't name is emptied.
//
// The pencil renames a profile: one GUI::Editbox (#dk2ml_lo_rename_box) is moved onto the row, given the current name and
// focused with Editbox::SetState(FOCUSED), as the game focuses its own boxes (it captures input: keys and chars then
// go only to the box, so Space / Esc don't reach the screen's BACK). Editbox::OnKeyDown unfocuses on Enter and Esc
// without running XML actions, and a click elsewhere unfocuses it too; the frame tick takes the name when that
// happens, like the trooper name, which the game reads when its box is left.
//
// Slot locks: a padlock (a clone of #dk2ml_lo_lock_template) in the top-right corner of each main slot; clicking it
// locks the slot for this operator, and LOAD and UNDO (store.cpp's Write) then leave it and its attachments alone.
//
// The panel is moved into Customization_Screen (which captures input), outside its box on the left: the cursor
// hit-test, GUIManager::FindItemUnderCursor_Recursive, walks children whether or not the cursor is over their parent.
// Its stealFocus background keeps clicks on empty panel space from reaching the screen's click catcher (which would
// close the screen). It hides while an equipment list is open. A row's orange mark shows the loadout is what the
// operator has equipped; it's refreshed after every Customize_OnEvent (equips and Paste loadout don't reselect the
// trooper).
#include "loadouts/panel.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/gui.h"
#include "core/settings.h"
#include "loadouts/history.h"
#include "loadouts/store.h"
#include "portraits/portraits.h"

namespace {

using loadouts::kProfiles;

constexpr int kSections = 2; // mine, the team's
const char* const kSectionKeys[kSections] = {"my", "team"};
constexpr const char* kPanel = "#dk2ml_lo_panel";
constexpr unsigned kDimText = 0xff8c8272; // an empty profile's text
constexpr float kRenameDx = 30;           // the rename box starts this far into the row (XML: RENAME_DX)

struct Row {
    void* load;  // the row's Button
    void* num;   // its number's StaticText (fixed in the XML)
    void* text;  // its StaticText
    void* mark;  // the "equipped" bar
    void* save;  // the SAVE Button
    void* rename; // the pencil Button
    void* erase;  // the X Button
    std::string shown; // the text last set
};

void* g_panel = nullptr;
void* g_list = nullptr; // #equipment_list_panel
void* g_titles[kSections] = {};
void* g_status = nullptr;
void* g_undo = nullptr;    // the UNDO Button
void* g_undoOff = nullptr; // its "nothing to undo" overlay
Row g_rows[kSections][kProfiles];
bool g_listOpen = false;
unsigned g_text = 0xfff0e3cc; // a row's text colour, from the XML
gui::TextFit g_rowFit, g_titleFit, g_statusFit;
std::string g_titleShown[kSections], g_statusShown;
std::string g_statusText;
const void* g_statusFor = nullptr; // the trooper the status line is about

// renaming: one Editbox, moved onto the row being renamed and focused; the name is taken when it loses focus (Enter,
// Esc or a click elsewhere), checked on the frame tick
void* g_renameBox = nullptr; // null if the XML item is missing or the Editbox names didn't resolve: no pencils
struct Renaming {
    bool active = false;
    loadouts::Set* set = nullptr; // in store.cpp's std::map: stays valid
    int s = 0, i = 0;
} g_renaming;

// slot locks: a padlock button cloned into each lockable slot's Checkbox, its last child (on top, and hit-tested before
// the slot, so a click on it doesn't open the item list)
constexpr const char* kLockTemplate = "#dk2ml_lo_lock_template";
struct Lock {
    void* button;     // our clone, in the slot
    std::string slot; // Inventory::GetNameForEnum name, eg. "PrimaryWeapon"
};
std::vector<Lock> g_locks;
bool g_locksAttached = false; // tried for this GUI

// the hover pop-up (#dk2ml_lo_preview): everything a profile holds, right of the panel, level with the hovered row
constexpr int kPreviewGuns = 3;   // weapon rows: primary, secondary, mounted gun
constexpr int kPreviewIcons = 16; // GEAR and LOOKS icons: 5 + 11 main slots
struct PreviewGun {
    void* img;  // the icon (StaticImage, not aligned)
    void* name; // the weapon's name (StaticText, lt, no sizeX)
    void* att;  // its attachments, dim (StaticText, lt, no sizeX)
};
struct Preview {
    void* root = nullptr; // null without the XML item: no pop-up
    void* border = nullptr;
    void* bg = nullptr;
    void* title = nullptr;
    void* none = nullptr;                   // "Nothing equipped"
    void* hint = nullptr;                   // "Click to equip all of it", at the bottom
    void* headers[loadouts::kCategories] = {}; // GUNS, GEAR, LOOKS
    PreviewGun guns[kPreviewGuns] = {};
    void* icons[kPreviewIcons] = {};        // GEAR / LOOKS items (StaticImages, not aligned)
} g_preview;
struct PreviewShown { // what the pop-up shows now (OnHover repeats while the cursor rests on the row)
    bool shown = false;
    const void* row = nullptr; // the hovered row's load Button
    loadouts::Items items;
    std::string title;
} g_previewShown;

// the class's starting loadout, after MY LOADOUTS' rows: loads only (no rename, erase or save); optional
struct StartingRow {
    void* load = nullptr; // the row's Button (null without the XML item: no row)
    void* text = nullptr; // its StaticText (no sizeX: fitted to kStartingTextW)
    void* mark = nullptr; // the "equipped" bar
    std::string shown;    // the text last set
} g_starting;
constexpr float kStartingTextW = 364; // the 388 row less its text's 12 px inset each side

// "UtilityPouch1" -> "Utility Pouch 1"
std::string SlotTitle(const std::string& slot)
{
    std::string out;
    for (size_t k = 0; k < slot.size(); k++) {
        char ch = slot[k];
        bool upper = ch >= 'A' && ch <= 'Z', digit = ch >= '0' && ch <= '9';
        bool prevDigit = k && slot[k - 1] >= '0' && slot[k - 1] <= '9';
        if (k && (upper || (digit && !prevDigit)))
            out += ' ';
        out += ch;
    }
    return out;
}

// what a write left alone, for the status line: " (2 unavailable, 1 locked)", " (1 locked)" or ""
std::string Extras(int skipped, int locked)
{
    std::string out;
    if (skipped)
        out = std::to_string(skipped) + " unavailable";
    if (locked)
        out += (out.empty() ? "" : ", ") + std::to_string(locked) + " locked";
    return out.empty() ? out : " (" + out + ")";
}

void* Screen()
{
    void* gameGUI = game::GameGUI();
    return gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizationScreen.Offset()) : nullptr;
}

void* CurrentTrooper()
{
    void* gameGUI = game::GameGUI();
    return gameGUI ? dk2ml::At<void*>(gameGUI, game::GameGUI_m_pCustomizeSelection.Offset()) : nullptr;
}

std::string RowName(int s, int i)
{
    return std::string("#dk2ml_lo_") + kSectionKeys[s] + "_" + std::to_string(i + 1);
}

loadouts::Set* SetOf(int s, const void* trooper)
{
    return s == 0 ? loadouts::For(trooper) : loadouts::TeamFor(trooper);
}

// "team loadout 2" / "loadout 2" (the panel calls profiles loadouts)
std::string SlotLabel(int s, int i)
{
    return std::string(s ? "team " : "") + "loadout " + std::to_string(i + 1);
}

// the same, starting a sentence: "Team loadout 2"
std::string SlotLabelCap(int s, int i)
{
    std::string label = SlotLabel(s, i);
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    return label;
}

void SetFitted(void* label, std::string& shown, const std::string& text, gui::TextFit& fit, int lines)
{
    if (shown == text)
        return;
    shown = text;
    gui::SetTextFitted(label, text, fit, lines);
}

void SetStatus(const void* trooper, const std::string& text)
{
    g_statusFor = trooper;
    g_statusText = text;
    game::api->Log("loadouts: %s", text.c_str());
}

bool Visible()
{
    void* screen = Screen();
    return g_settings.loadoutTabs && screen && !gui::IsHidden(screen) && !g_listOpen && loadouts::For(CurrentTrooper());
}

// the rows' "equipped" marks: a saved profile that's what the operator has on now
void UpdateMarks(const void* trooper)
{
    loadouts::Items current = loadouts::Read(trooper, loadouts::kAll);
    for (int s = 0; s < kSections; s++) {
        const loadouts::Set* set = SetOf(s, trooper);
        for (int i = 0; i < kProfiles; i++) {
            const loadouts::Profile* p = set ? &set->profiles[i] : nullptr;
            gui::ShowIf(g_rows[s][i].mark, p && p->used && loadouts::Same(trooper, current, p->items));
        }
    }
}

// the starting loadout row: shown while the class has one, its weapons as its text, marked while it's what's equipped
void UpdateStarting(const void* trooper)
{
    if (!g_starting.load)
        return;
    loadouts::Items items = loadouts::StartingLoadout(trooper);
    gui::ShowIf(g_starting.load, !items.empty());
    if (items.empty())
        return;
    std::string guns = loadouts::Summary(loadouts::kGuns, items);
    std::string text = guns.empty() ? "Starting loadout" : "Starting loadout: " + guns;
    if (g_starting.shown != text) {
        g_starting.shown = text;
        gui::SetTextFittedWidth(g_starting.text, text, kStartingTextW);
    }
    gui::ShowIf(g_starting.mark, loadouts::Same(trooper, loadouts::Read(trooper, loadouts::kAll), items));
}

// the padlocks: shown (setting on, an operator with a key), bright on the operator's locked slots
void UpdateLocks()
{
    const void* trooper = g_settings.loadoutTabs ? CurrentTrooper() : nullptr;
    bool shown = trooper && !loadouts::KeyOf(trooper).empty();
    for (const Lock& l : g_locks) {
        gui::ShowIf(l.button, shown);
        if (!shown)
            continue;
        bool locked = loadouts::IsLocked(trooper, l.slot);
        gui::ShowIf(gui::Find(l.button, "#dk2ml_lo_lock_on"), locked);
        gui::ShowIf(gui::Find(l.button, "#dk2ml_lo_lock_off"), !locked);
    }
}

void HidePreview()
{
    gui::ShowIf(g_preview.root, false);
    g_previewShown = {};
}

// The hover pop-up's layout, top down, in pixels from its top left (the XML pop-up is 452 wide).
namespace preview {
constexpr float kPad = 12;
constexpr float kTextW = 428;         // the title's room
constexpr float kTitleH = 34;
constexpr float kHeaderH = 28;
constexpr float kSectionGap = 6;
constexpr float kHintH = 26;          // the "click to equip" line at the bottom
constexpr float kBorder = 2;          // the orange frame around the fill
constexpr float kGap = 12;            // between the panel and the pop-up
constexpr float kBelowPanel = 500;    // how far it may reach below the panel (still above Trooper Toolbox's BACK)
// a weapon row: the icon in a 150x46 box, the name and (below it) the attachments right of it
constexpr float kGunH = 56;
constexpr float kGunIconW = 150, kGunIconH = 46, kGunIconScale = 0.45f;
constexpr float kGunTextX = 172, kGunTextW = 268;
constexpr float kGunAttDy = 27;
// GEAR / LOOKS: icons only, 6 per row in 68x60 cells 4 apart, each fitted into 64x56
constexpr int kIconCols = 6;
constexpr float kCellW = 68, kCellH = 60, kCellGap = 4;
constexpr float kIconW = 64, kIconH = 56, kIconScale = 0.5f;
} // namespace preview

// An item's icon fitted into its box; hidden without one (a template that's gone).
void FillPreviewIcon(void* img, const char* image, float boxW, float boxH, float maxScale)
{
    bool icon = image && gui::IsA(img, game::GUI_StaticImage_vftable.Address());
    if (icon) {
        game::GUI_StaticImage_SetImageAndImplicitSize(img, image);
        gui::FitImage(img, boxW, boxH, maxScale);
    }
    gui::ShowIf(img, icon);
}

// The pop-up for a row (its load Button): the title, then GUNS (a row per weapon: icon, name, attachments), GEAR and
// LOOKS (icons only, 6 per row), each only if it holds something, and a "click to equip" hint. Its height is fitted to
// what it shows, as the equipment grid's "used by" pop-up: m_size.y set, the backgrounds stretched, then every child
// re-placed from the new top. Right of the panel (never over the row, which would end the hover), its top level with
// the row's, kept between the panel's top and a little below its bottom.
void ShowPreview(void* row, const std::string& title, const loadouts::Items& items)
{
    using namespace preview;
    const PreviewShown& now = g_previewShown;
    if (now.shown && now.row == row && now.items == items && now.title == title)
        return; // OnHover repeats while the cursor rests on the row
    g_previewShown = {true, row, items, title};

    const Preview& pv = g_preview;
    for (void* header : pv.headers)
        gui::ShowIf(header, false);
    gui::ShowIf(pv.none, false);
    for (const PreviewGun& g : pv.guns)
        for (void* item : {g.img, g.name, g.att})
            gui::ShowIf(item, false);
    for (void* icon : pv.icons)
        gui::ShowIf(icon, false);

    struct Place {
        void* item;
        float x, y;  // texts: their top left; icons: their box's centre
        bool centred;
    };
    std::vector<Place> places;
    float y = kPad;
    gui::SetTextFittedWidth(pv.title, title, kTextW);
    places.push_back({pv.title, kPad, y, false});
    y += kTitleH;

    bool any = false;
    int icon = 0;
    for (int c = 0; c < loadouts::kCategories; c++) {
        std::vector<loadouts::Shown> shown = loadouts::Contents(c, items);
        if (shown.empty())
            continue;
        any = true;
        gui::ShowIf(pv.headers[c], true);
        places.push_back({pv.headers[c], kPad, y, false});
        y += kHeaderH;
        if (c == loadouts::kGuns) {
            for (size_t k = 0; k < shown.size() && k < kPreviewGuns; k++) {
                const PreviewGun& g = pv.guns[k];
                const loadouts::Shown& e = shown[k];
                FillPreviewIcon(g.img, e.image, kGunIconW, kGunIconH, kGunIconScale);
                places.push_back({g.img, kPad + kGunIconW / 2, y + kGunH / 2, true});
                gui::SetTextFittedWidth(g.name, e.name, kGunTextW);
                gui::ShowIf(g.name, true);
                places.push_back({g.name, kGunTextX, y + 3, false});
                if (!e.attachments.empty()) {
                    gui::SetTextFittedWidth(g.att, e.attachments, kGunTextW);
                    gui::ShowIf(g.att, true);
                    places.push_back({g.att, kGunTextX, y + 3 + kGunAttDy, false});
                }
                y += kGunH;
            }
        } else {
            size_t k = 0;
            for (; k < shown.size() && icon < kPreviewIcons; k++, icon++) {
                float cx = kPad + static_cast<float>(k % kIconCols) * (kCellW + kCellGap) + kCellW / 2;
                float cy = y + static_cast<float>(k / kIconCols) * (kCellH + kCellGap) + kCellH / 2;
                FillPreviewIcon(pv.icons[icon], shown[k].image, kIconW, kIconH, kIconScale);
                places.push_back({pv.icons[icon], cx, cy, true});
            }
            size_t rows = (k + kIconCols - 1) / kIconCols;
            y += static_cast<float>(rows) * (kCellH + kCellGap) - kCellGap;
        }
        y += kSectionGap;
    }
    if (!any) {
        gui::ShowIf(pv.none, true);
        places.push_back({pv.none, kPad, y, false});
        y += kHeaderH;
    }
    places.push_back({pv.hint, kPad, y + 4, false});
    y += kHintH;
    float h = std::ceil(y + kPad);

    // the pop-up and its backgrounds at that height, then everything re-placed from the new top
    int* size = &dk2ml::At<int>(pv.root, game::GUI_Item_m_size.Offset());
    size[1] = static_cast<int>(h);
    float w = static_cast<float>(size[0]);
    gui::StretchTo(pv.border, 0, h + 2 * kBorder);
    gui::StretchTo(pv.bg, 0, h);
    for (const Place& pl : places) {
        if (pl.centred)
            gui::SetOrigin(pl.item, pl.x - w / 2, h / 2 - pl.y); // not aligned: from the pop-up's centre, +y up
        else
            gui::SetOrigin(pl.item, pl.x, -pl.y);                // align="lt", no sizeX: from its top left
    }

    // beside the panel; the row's m_alignOffset is its XML origin in #dk2ml_lo_box (lt: y = -its top), and the pop-up's
    // layer has the box's size and place
    void* box = gui::Parent(pv.root);
    const int* boxSize = &dk2ml::At<int>(box, game::GUI_Item_m_size.Offset());
    float boxW = static_cast<float>(boxSize[0]), boxH = static_cast<float>(boxSize[1]);
    float rowTop = -dk2ml::At<float>(row, game::GUI_Item_m_alignOffset.Offset() + static_cast<int32_t>(sizeof(float)));
    float top = std::max(0.0f, std::min(rowTop, boxH + kBelowPanel - h));
    gui::SetOrigin(pv.root, boxW / 2 + kGap + w / 2, boxH / 2 - top - h / 2);
    game::GUI_Item_Show(pv.root);
}

// The pop-up for the row whose load Button this is: a saved profile, or the starting loadout; hidden for an unsaved
// profile (or no pop-up in the XML).
void ShowRowPreview(void* row)
{
    const void* trooper = CurrentTrooper();
    if (!g_preview.root || !row) {
        HidePreview();
        return;
    }
    if (row == g_starting.load) {
        loadouts::Items items = loadouts::StartingLoadout(trooper);
        if (items.empty())
            HidePreview();
        else
            ShowPreview(row, "STARTING LOADOUT", items);
        return;
    }
    for (int s = 0; s < kSections; s++) {
        for (int i = 0; i < kProfiles; i++) {
            if (g_rows[s][i].load != row)
                continue;
            const loadouts::Set* set = SetOf(s, trooper);
            const loadouts::Profile* p = set ? &set->profiles[i] : nullptr;
            if (!p || !p->used) {
                HidePreview();
                return;
            }
            std::string title = std::string(s ? "TEAM LOADOUT " : "LOADOUT ") + std::to_string(i + 1) + (p->name.empty() ? "" : ": " + p->name);
            ShowPreview(row, title, p->items);
            return;
        }
    }
}

void Refresh()
{
    if (!g_panel)
        return;
    void* screen = Screen();
    if (g_settings.loadoutTabs && (!screen || gui::IsHidden(screen)))
        return; // hidden with the screen; the selection may be stale until it opens again
    if (g_settings.loadoutTabs)
        loadouts::history::Track(CurrentTrooper()); // a gear change since the last refresh becomes an undo step
    UpdateLocks();
    bool visible = Visible();
    gui::ShowIf(g_panel, visible);
    if (!visible) {
        HidePreview();
        return;
    }
    const void* trooper = CurrentTrooper();

    std::string squad = loadouts::SquadName(trooper);
    SetFitted(g_titles[0], g_titleShown[0], std::string("MY LOADOUTS: ") + portraits::TrooperName(trooper), g_titleFit, 1);
    SetFitted(g_titles[1], g_titleShown[1], "TEAM LOADOUTS: " + squad, g_titleFit, 1);

    for (int s = 0; s < kSections; s++) {
        const loadouts::Set* set = SetOf(s, trooper);
        for (int i = 0; i < kProfiles; i++) {
            Row& row = g_rows[s][i];
            const loadouts::Profile* p = set ? &set->profiles[i] : nullptr;
            // the number is its own label (_num, fixed in the XML); the text beside it: the name when named, else the
            // weapons ("MP7 / Glock 19"), or "empty" (the hover pop-up shows everything)
            std::string text;
            if (p && !p->name.empty()) {
                text = p->name;
            } else if (p && p->used) {
                std::string guns = loadouts::Summary(loadouts::kGuns, p->items);
                text = guns.empty() ? "no weapons" : guns;
            } else {
                text = "empty";
            }
            SetFitted(row.text, row.shown, text, g_rowFit, 1);
            unsigned color = p && p->used ? g_text : kDimText;
            gui::SetTextColor(row.text, color);
            gui::SetTextColor(row.num, color);
        }
    }
    UpdateMarks(trooper);
    UpdateStarting(trooper);
    gui::ShowIf(g_undoOff, loadouts::history::Steps(trooper) == 0);

    if (g_statusFor != trooper)
        g_statusText.clear(); // about another operator
    SetFitted(g_status, g_statusShown, g_statusText.empty() ? " " : g_statusText, g_statusFit, 1);
    if (g_previewShown.shown)
        ShowRowPreview(const_cast<void*>(g_previewShown.row)); // eg. the hovered profile was loaded into (re-laid out only if it changed)
}

// The starting loadout row: the class's starting loadout, every slot (what it doesn't name is emptied), as LOAD does.
void LoadStarting()
{
    void* trooper = CurrentTrooper();
    loadouts::Items items = loadouts::StartingLoadout(trooper);
    if (items.empty())
        return;
    loadouts::history::Track(trooper); // changes not seen yet are their own step
    loadouts::WriteResult r = loadouts::Write(trooper, loadouts::kAll, items);
    loadouts::history::Track(trooper, "Loaded starting loadout"); // undo goes back to before the load
    if (!r.changed && !r.skipped && !r.locked)
        SetStatus(trooper, "Starting loadout already equipped.");
    else
        SetStatus(trooper, "Loaded starting loadout" + Extras(r.skipped, r.locked));
    if (r.changed)
        game::SaveAndRefresh(trooper); // runs Loadouts_Refresh (Customize_OnTrooperSelected)
    Refresh();
}

void Load(int s, int i)
{
    void* trooper = CurrentTrooper();
    loadouts::Set* set = SetOf(s, trooper);
    if (!set)
        return;
    const loadouts::Profile& p = set->profiles[i];
    std::string label = SlotLabel(s, i);
    if (!p.used) {
        SetStatus(trooper, SlotLabelCap(s, i) + " is empty.");
        Refresh();
        return;
    }
    loadouts::history::Track(trooper); // changes not seen yet are their own step
    loadouts::WriteResult r = loadouts::Write(trooper, loadouts::kAll, p.items);
    loadouts::history::Track(trooper, "Loaded " + label); // undo goes back to before the load
    if (!r.changed && !r.skipped && !r.locked)
        SetStatus(trooper, SlotLabelCap(s, i) + " already equipped.");
    else
        SetStatus(trooper, "Loaded " + label + Extras(r.skipped, r.locked));
    if (r.changed)
        game::SaveAndRefresh(trooper); // runs Loadouts_Refresh (Customize_OnTrooperSelected)
    Refresh();
}

void Save(int s, int i)
{
    void* trooper = CurrentTrooper();
    loadouts::Set* set = SetOf(s, trooper);
    if (!set)
        return;
    loadouts::Profile& p = set->profiles[i];
    p.used = true;
    p.items = loadouts::Read(trooper, loadouts::kAll); // the name stays
    loadouts::Save();
    SetStatus(trooper, "Saved " + SlotLabel(s, i) + ".");
    Refresh();
}

// UNDO: the gear from before the last change (a loaded profile, single items, Paste loadout...)
void Undo()
{
    void* trooper = CurrentTrooper();
    if (!trooper)
        return;
    loadouts::history::Track(trooper); // a change not seen yet is the step to undo
    loadouts::history::Undone u = loadouts::history::Undo(trooper);
    if (!u.done) {
        SetStatus(trooper, "Nothing to undo.");
    } else {
        if (!u.restored.empty())
            game::api->Log("loadouts: undo restored %s", u.restored.c_str()); // too long for the one-line status
        SetStatus(trooper, "Undid: " + u.label + Extras(u.skipped, u.locked) + (u.restored.empty() ? " (no change)" : ""));
    }
    if (u.changed)
        game::SaveAndRefresh(trooper); // runs Loadouts_Refresh (Customize_OnTrooperSelected)
    Refresh();
}

// The rename box lost focus (Enter, Esc or a click elsewhere): its text, trimmed, is the profile's new name.
void FinishRename()
{
    g_renaming.active = false;
    const char* typed = dk2ml::At<char*>(g_renameBox, game::GUI_Editbox_m_text.Offset());
    std::string name = typed ? typed : "";
    size_t first = name.find_first_not_of(' '), last = name.find_last_not_of(' ');
    name = first == std::string::npos ? std::string() : name.substr(first, last - first + 1);
    gui::ShowIf(g_renameBox, false);

    loadouts::Profile& p = g_renaming.set->profiles[g_renaming.i];
    std::string label = SlotLabel(g_renaming.s, g_renaming.i);
    if (name != p.name) {
        p.name = name;
        loadouts::Save();
        SetStatus(CurrentTrooper(), name.empty() ? "Unnamed " + label + "." : "Named " + label + " \"" + name + "\".");
    }
    Refresh();
}

// The pencil: the rename box over the row's first line, holding the current name, focused (it then gets the keyboard).
void StartRename(int s, int i)
{
    if (g_renaming.active)
        FinishRename(); // another row's rename still open
    loadouts::Set* set = SetOf(s, CurrentTrooper());
    if (!set || !g_renameBox)
        return;
    HidePreview(); // the box goes over the row
    g_renaming = {true, set, s, i};
    const float* row = &dk2ml::At<float>(g_rows[s][i].load, game::GUI_Item_m_alignOffset.Offset()); // its XML origin
    gui::SetOrigin(g_renameBox, row[0] + kRenameDx, row[1]); // beside the row's number, which stays visible
    game::GUI_Editbox_SetText_varg(g_renameBox, "%s", set->profiles[i].name.c_str());
    gui::ShowIf(g_renameBox, true);
    game::GUI_Editbox_SetState(g_renameBox, static_cast<int>(game::Editbox_STATE_FOCUSED.Get()));
}

// X: the profile's saved items and name gone (the operator's gear isn't touched)
void Erase(int s, int i)
{
    void* trooper = CurrentTrooper();
    loadouts::Set* set = SetOf(s, trooper);
    if (!set)
        return;
    loadouts::Profile& p = set->profiles[i];
    std::string label = SlotLabel(s, i);
    if (g_renaming.active && g_renaming.set == set && g_renaming.i == i) {
        g_renaming.active = false; // a rename open on this profile would put the name back
        gui::ShowIf(g_renameBox, false);
    }
    if (!p.used && p.name.empty()) {
        SetStatus(trooper, SlotLabelCap(s, i) + " is already empty.");
    } else {
        p = {};
        loadouts::Save();
        SetStatus(trooper, "Erased " + label + ".");
    }
    Refresh();
}

// Callback on a row's, a SAVE, a pencil or an X button's OnClick: the item is the Button
void OnClicked(void* item, float, float, void*)
{
    if (!g_settings.loadoutTabs)
        return;
    if (item == g_undo)
        return Undo();
    if (item == g_starting.load)
        return LoadStarting();
    for (int s = 0; s < kSections; s++) {
        for (int i = 0; i < kProfiles; i++) {
            const Row& r = g_rows[s][i];
            if (r.load == item)
                return Load(s, i);
            if (r.save == item)
                return Save(s, i);
            if (r.rename == item)
                return StartRename(s, i);
            if (r.erase == item)
                return Erase(s, i);
        }
    }
}

// Callbacks on a row's OnHover / OnHoverEnd: the pop-up with everything in that profile (or the starting loadout)
void OnRowHover(void* item, float, float, void*)
{
    if (!g_settings.loadoutTabs || g_renaming.active)
        return;
    ShowRowPreview(item);
}

void OnRowHoverEnd(void*, float, float, void*)
{
    HidePreview();
}

// The hover pop-up and its lines, all or nothing (without them, no pop-up).
Preview FindPreview(void* panel)
{
    Preview pv;
    void* root = gui::Find(panel, "#dk2ml_lo_preview");
    if (!root)
        return pv;
    auto find = [&](const std::string& suffix) { return gui::Find(root, ("#dk2ml_lo_preview" + suffix).c_str()); };
    pv.border = find("_border");
    pv.bg = find("_bg");
    pv.title = find("_title");
    pv.none = find("_none");
    pv.hint = find("_hint");
    pv.headers[loadouts::kGuns] = find("_guns");
    pv.headers[loadouts::kGear] = find("_gear");
    pv.headers[loadouts::kLooks] = find("_looks");
    bool ok = pv.border && pv.bg && pv.title && pv.none && pv.hint && pv.headers[0] && pv.headers[1] && pv.headers[2];
    for (int k = 0; k < kPreviewGuns; k++) {
        std::string prefix = "_gun_" + std::to_string(k + 1);
        PreviewGun& g = pv.guns[k];
        g = {find(prefix + "_img"), find(prefix + "_name"), find(prefix + "_att")};
        ok = ok && g.img && g.name && g.att;
    }
    for (int k = 0; k < kPreviewIcons; k++) {
        pv.icons[k] = find("_tile_" + std::to_string(k + 1) + "_img");
        ok = ok && pv.icons[k];
    }
    if (ok)
        pv.root = root;
    return pv;
}

// Callback on a padlock's OnClick: the item is our clone in the slot
void OnLockClicked(void* item, float, float, void*)
{
    void* trooper = CurrentTrooper();
    if (!g_settings.loadoutTabs || !trooper)
        return;
    for (const Lock& l : g_locks) {
        if (l.button != item)
            continue;
        bool locked = loadouts::ToggleLock(trooper, l.slot);
        loadouts::Save();
        SetStatus(trooper, (locked ? "Locked " : "Unlocked ") + SlotTitle(l.slot) + ".");
        Refresh();
        return;
    }
}

// A padlock in each lockable slot on the screen, cloned from our template the way the picker clones its cells. Once
// per GUI; without the template (or a slot), that slot just has none.
void AttachLocks(void* screen)
{
    if (g_locksAttached)
        return;
    g_locksAttached = true;
    void* tmpl = gui::Find(game::GuiRoot(), kLockTemplate);
    if (!tmpl) {
        game::api->Log("loadouts: no %s (gui/loadout_panel.xml), slots can't be locked", kLockTemplate);
        return;
    }
    gui::SetCallback(tmpl, game::EVENT_CLICK.Get(), OnLockClicked); // clones copy it, with themselves as owner
    for (const std::string& slot : loadouts::LockableSlots()) {
        void* checkbox = gui::Find(screen, slot.c_str());
        if (!checkbox)
            continue; // eg. no Custom slots without Trooper Toolbox
        void* clone = game::GUI_Button_CloneInternal(tmpl);
        if (!clone)
            continue;
        game::GUI_Button_UpdatePointerReferences(clone, clone, game::GuiRoot());
        game::GUI_Item_SetName(clone, nullptr);
        gui::AddChild(checkbox, clone);
        g_locks.push_back({clone, slot});
    }
    game::api->Log("loadouts: %d slot padlock(s)", static_cast<int>(g_locks.size()));
}

// Moves the panel into the screen, just before the screen's child holding #player_profile_panel: after the click
// catcher and backgrounds (hit-tested before them, so a click on the panel never closes the screen), under the profile
// panel, whose (i) STATS pop-up (#player_profile_info, origin -430 from the portrait) opens over ours. Children are
// drawn in order and AddChild appends, so the anchor and everything after it are re-added behind the panel, in order.
// Without the anchor (another customization.xml), the panel goes last.
void PlacePanel(void* screen, void* panel)
{
    void* anchor = gui::Find(screen, "#player_profile_panel");
    while (anchor && gui::Parent(anchor) != screen)
        anchor = gui::Parent(anchor);
    if (!anchor) {
        game::api->Log("loadouts: no #player_profile_panel, the panel goes last on the screen");
        gui::AddChild(screen, panel);
        return;
    }
    bool moving = false;
    for (void* c : gui::Children(screen)) {
        if (c == anchor) {
            moving = true;
            gui::AddChild(screen, panel);
        }
        if (moving && c != panel)
            gui::AddChild(screen, c);
    }
}

// Finds the panel and every row, all or nothing (logged once per GUI), and moves the panel into the screen.
bool FindPanel(void* screen)
{
    static bool logged = false;
    auto fail = [](const std::string& what) {
        if (!logged)
            game::api->Log("loadouts: no %s, no loadouts panel", what.c_str());
        logged = true;
        return false;
    };
    void* panel = gui::Find(game::GuiRoot(), kPanel);
    if (!panel)
        return fail(std::string(kPanel) + " (gui/loadout_panel.xml)");
    void* titles[kSections];
    for (int s = 0; s < kSections; s++) {
        std::string name = std::string("#dk2ml_lo_") + kSectionKeys[s] + "_title";
        if (!(titles[s] = gui::Find(panel, name.c_str())))
            return fail(name);
    }
    void* status = gui::Find(panel, "#dk2ml_lo_status");
    if (!status)
        return fail("#dk2ml_lo_status");
    void* undo = gui::Find(panel, "#dk2ml_lo_undo");
    void* undoOff = gui::Find(undo, "#dk2ml_lo_undo_off");
    if (!undo || !undoOff)
        return fail("#dk2ml_lo_undo (or its _off)");
    Row rows[kSections][kProfiles];
    for (int s = 0; s < kSections; s++) {
        for (int i = 0; i < kProfiles; i++) {
            std::string name = RowName(s, i);
            Row& r = rows[s][i];
            r.load = gui::Find(panel, name.c_str());
            r.num = gui::Find(r.load, (name + "_num").c_str());
            r.text = gui::Find(r.load, (name + "_text").c_str());
            r.mark = gui::Find(r.load, (name + "_on").c_str());
            r.save = gui::Find(panel, (name + "_save").c_str());
            r.rename = gui::Find(panel, (name + "_rename").c_str());
            r.erase = gui::Find(panel, (name + "_erase").c_str());
            if (!r.load || !r.num || !r.text || !r.mark || !r.save || !r.rename || !r.erase)
                return fail(name + " (or its _num / _text / _on / _save / _rename / _erase)");
        }
    }

    for (int s = 0; s < kSections; s++) {
        g_titles[s] = titles[s];
        for (int i = 0; i < kProfiles; i++) {
            g_rows[s][i] = rows[s][i];
            gui::SetCallback(rows[s][i].load, game::EVENT_CLICK.Get(), OnClicked);
            gui::SetCallback(rows[s][i].save, game::EVENT_CLICK.Get(), OnClicked);
            gui::SetCallback(rows[s][i].rename, game::EVENT_CLICK.Get(), OnClicked);
            gui::SetCallback(rows[s][i].erase, game::EVENT_CLICK.Get(), OnClicked);
            gui::SetCallback(rows[s][i].load, game::EVENT_CURSOR_HOVER.Get(), OnRowHover);
            gui::SetCallback(rows[s][i].load, game::EVENT_CURSOR_HOVER_END.Get(), OnRowHoverEnd);
        }
    }
    // the hover pop-up lives in its own layer (placed like the panel), the screen's last child: drawn over the profile
    // panel, which the panel itself sits under
    void* layer = gui::Find(game::GuiRoot(), "#dk2ml_lo_preview_layer");
    g_preview = layer ? FindPreview(layer) : Preview{};
    if (!g_preview.root)
        game::api->Log("loadouts: no #dk2ml_lo_preview_layer > #dk2ml_lo_preview (or one of its lines), no hover pop-up");
    gui::ShowIf(gui::Find(layer, "#dk2ml_lo_preview"), false);
    // renaming needs our Editbox and the game's Editbox functions; without them the pencils go
    g_renameBox = game::EditboxUsable() ? gui::Find(panel, "#dk2ml_lo_rename_box") : nullptr;
    if (!g_renameBox)
        game::api->Log("loadouts: %s, loadouts can't be renamed", game::EditboxUsable() ? "no #dk2ml_lo_rename_box" : "no Editbox functions");
    gui::ShowIf(gui::Find(panel, "#dk2ml_lo_rename_box"), false);
    for (auto& section : g_rows)
        for (Row& r : section)
            gui::ShowIf(r.rename, g_renameBox != nullptr);
    g_status = status;
    g_undo = undo;
    g_undoOff = undoOff;
    gui::SetCallback(undo, game::EVENT_CLICK.Get(), OnClicked);
    g_text = gui::TextColor(rows[0][0].text);
    // the starting loadout row: optional (without it, or without the bindings, no row)
    g_starting = {};
    void* starting = gui::Find(panel, "#dk2ml_lo_my_default");
    void* startingText = gui::Find(starting, "#dk2ml_lo_my_default_text");
    void* startingMark = gui::Find(starting, "#dk2ml_lo_my_default_on");
    if (starting && startingText && startingMark && game::StartingLoadoutUsable()) {
        g_starting = {starting, startingText, startingMark, {}};
        gui::SetCallback(starting, game::EVENT_CLICK.Get(), OnClicked);
        gui::SetCallback(starting, game::EVENT_CURSOR_HOVER.Get(), OnRowHover);
        gui::SetCallback(starting, game::EVENT_CURSOR_HOVER_END.Get(), OnRowHoverEnd);
    } else {
        game::api->Log("loadouts: %s, no starting loadout row", game::StartingLoadoutUsable() ? "no #dk2ml_lo_my_default (or its _text / _on)"
                                                                                              : "starting loadout bindings off");
        gui::ShowIf(starting, false);
    }
    if (gui::Parent(panel) != screen)
        PlacePanel(screen, panel);
    if (layer && gui::Parent(layer) != screen) {
        gui::AddChild(screen, layer); // last: drawn on top
        gui::ShowIf(layer, true);     // transparent, and StaticImages never take the cursor
    }
    g_list = gui::Find(screen, "#equipment_list_panel");
    g_listOpen = g_list && !gui::IsHidden(g_list);
    g_panel = panel;
    logged = false;
    game::api->Log("loadouts: panel attached");
    return true;
}

} // namespace

void Loadouts_Attach()
{
    void* screen = Screen();
    if (!screen)
        return;
    if (!g_panel && !FindPanel(screen))
        return;
    AttachLocks(screen);
    Refresh();
}

void Loadouts_Refresh()
{
    Loadouts_Attach();
}

void Loadouts_OnCustomizeEvent()
{
    if (!g_panel)
        return;
    if (g_list)
        g_listOpen = !gui::IsHidden(g_list); // eg. equipping hides the list in code
    Refresh();
}

void Loadouts_OnShow(const void* item)
{
    if (item && item == g_list) {
        g_listOpen = true;
        gui::ShowIf(g_panel, false);
        HidePreview();
    }
}

void Loadouts_OnHide(const void* item)
{
    if (item && item == g_list) {
        g_listOpen = false;
        Refresh();
    }
}

void Loadouts_SetEnabled(bool)
{
    if (!g_settings.loadoutTabs) {
        gui::ShowIf(g_panel, false);
        HidePreview();
        UpdateLocks(); // hides the padlocks
    } else {
        Refresh();
    }
}

void Loadouts_Tick()
{
    if (!g_renaming.active || !g_renameBox)
        return;
    if (dk2ml::At<int>(g_renameBox, game::GUI_Editbox_m_state.Offset()) == game::Editbox_STATE_UNFOCUSED.Get())
        FinishRename();
}

void Loadouts_OnGuiLoaded()
{
    g_panel = g_list = g_status = g_renameBox = g_undo = g_undoOff = nullptr;
    g_renaming = {}; // a rename open across the reload is dropped
    g_preview = {};
    g_previewShown = {};
    g_starting = {};
    g_locks.clear(); // deleted with their slots
    g_locksAttached = false;
    g_listOpen = false;
    g_rowFit = g_titleFit = g_statusFit = {};
    g_statusShown.clear();
    for (int s = 0; s < kSections; s++) {
        g_titles[s] = nullptr;
        g_titleShown[s].clear();
        for (Row& r : g_rows[s])
            r = {};
    }
}
