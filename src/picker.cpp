// gui/portrait_selector.xml defines the open button and the picker as top-level items, so the mod doesn't replace
// customization.xml (other UI mods do). They're moved into the Customization screen with the game's AddChild action:
// the button onto the large portrait, the picker into the screen itself, which captures input while it's open (a
// top-level picker would never get clicks).
//
// Clicks reach us through XML <Action type="Callback">: the game calls the action's pCallback, which we point at our
// functions. Cloning a cell copies its actions (with owner = the clone), so a cell's callback knows which cell it was.
// That needs no hook on GUI::sAction::Execute, which the freecam plugin already hooks (MinHook can't hook a function
// twice).
//
// The list is virtualized: the content is a list of rows, each either a run of one group's portraits or that group's
// header (All units groups the pool by unit, see portraits.h), but only one screen of row slots exists. Each slot has
// a cell per column plus a header item, cloned from hidden templates the first time the picker opens (like the stock
// equipment list clones its items) and reused from then on. Scrolling moves a window over the rows and re-skins the
// slots (textures, marks, tooltips), so a pool of thousands of mod portraits costs no more than one screen. It
// scrolls in whole rows: nothing clips a partly visible row. Cells are never deleted, which also keeps them valid
// while their own click or scroll event is still running.
#include "picker.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "game.h"
#include "gui.h"
#include "scroll.h"
#include "portraits.h"
#include "settings.h"

namespace {

constexpr const char* kOpenButton = "#dk2ml_ps_open"; // container moved onto the large portrait
constexpr const char* kOpenButtonInner = "#dk2ml_ps_open_btn";
constexpr const char* kPicker = "#dk2ml_ps_picker";
constexpr const char* kGrid = "#dk2ml_ps_grid";
constexpr const char* kTemplate = "#dk2ml_ps_cell_template";
constexpr const char* kCellImage = "#dk2ml_ps_cell_img";
constexpr const char* kCellSelected = "#dk2ml_ps_cell_sel";
constexpr const char* kCellUsed = "#dk2ml_ps_cell_used"; // worn by a squadmate
constexpr const char* kCellUsedOther = "#dk2ml_ps_cell_used_other"; // worn by someone in another squad
constexpr const char* kTrack = "#dk2ml_ps_track";
constexpr const char* kThumb = "#dk2ml_ps_thumb";
constexpr const char* kPreview = "#dk2ml_ps_preview"; // hover pop-up
constexpr const char* kPreviewImage = "#dk2ml_ps_preview_img";            // for _large textures (scaled down)
constexpr const char* kPreviewImageSmall = "#dk2ml_ps_preview_img_small"; // fallback: the cell texture, scaled up
constexpr const char* kPreviewName = "#dk2ml_ps_preview_name";
constexpr const char* kPreviewUsed = "#dk2ml_ps_preview_used";
constexpr const char* kPreviewCurrent = "#dk2ml_ps_preview_current"; // badge on the trooper's own portrait
constexpr const char* kStatusCount = "#dk2ml_ps_status_count";
constexpr const char* kStatusUsed = "#dk2ml_ps_status_used";
constexpr const char* kHeaderTemplate = "#dk2ml_ps_header_template";
constexpr const char* kHeaderText = "#dk2ml_ps_header_text";
constexpr const char* kTitle = "#dk2ml_ps_title";
constexpr const char* kHideUsed = "#dk2ml_ps_f_hideused"; // toggle
constexpr const char* kHideUsedMarker = "#dk2ml_ps_f_hideused_on";
constexpr const char* kRandom = "#dk2ml_ps_random";
constexpr float kCellGap = 8;
constexpr int kFallbackCellSize = 88;
constexpr int kWheelRows = 2; // card rows per mouse wheel notch (headers don't count: scroll.h)
constexpr int32_t kSizeY = sizeof(int); // GUI::Item::m_size is int[2] {x, y}

struct FilterButton {
    const char* item;
    const char* marker; // child shown while this filter is active
    void* ptr;
};

FilterButton g_filters[] = {
    {"#dk2ml_ps_f_unit", "#dk2ml_ps_f_unit_on", nullptr},
    {"#dk2ml_ps_f_all", "#dk2ml_ps_f_all_on", nullptr},
};

void* g_picker = nullptr;
void* g_grid = nullptr;
void* g_template = nullptr;
void* g_headerTemplate = nullptr; // optional: without it there are no group headers
void* g_screen = nullptr;
void* g_track = nullptr; // scrollbar (optional)
void* g_thumb = nullptr;
void* g_preview = nullptr; // hover pop-up (optional) and its parts
void* g_previewImage = nullptr;
void* g_previewImageSmall = nullptr;
void* g_title = nullptr;
void* g_hideUsedButton = nullptr;
void* g_randomButton = nullptr;
void* g_previewName = nullptr;
void* g_previewUsed = nullptr;
void* g_previewCurrent = nullptr;
void* g_statusCount = nullptr; // status line between the button groups (optional)
void* g_statusUsed = nullptr;
bool g_loggedNoPortrait = false;

// One screen of items, cloned once per GUI load: g_cols cells per row slot, row by row, and a header per row slot.
std::vector<void*> g_cells;
std::vector<void*> g_headers;
int g_cols = 0;
int g_visibleRows = 0;
std::vector<int> g_cellEntry; // entry index each cell shows, -1 if hidden
struct Pos {
    float x, y;
};
std::vector<Pos> g_cellPos;   // each cell's origin in the grid (center-relative, +y up)
float g_cellW = 0, g_cellH = 0;
int g_hoverCell = -1;         // cell the pop-up is showing, -1 if none
int g_previewEntry = -1;      // entry it shows

// A row of the list: header == true -> the group's header, else the group's entries [first, first + count)
struct Row {
    bool header;
    size_t group;
    size_t first, count;
};

std::vector<portraits::Entry> g_entries;  // the filtered pool for g_builtFor, grouped
std::vector<portraits::Group> g_groups;
std::vector<Row> g_rows;
std::vector<portraits::User> g_used;      // portraits everyone else in the roster wears
bool g_hasHeaders = false;                // g_rows has header rows (All units)
int g_top = 0;          // first visible row
float g_dragStartY = 0; // thumb drag: cursor y and g_top when it was pressed
int g_dragStartTop = 0;
const void* g_builtFor = nullptr;
portraits::UnitFilter g_unit = portraits::UnitFilter::ThisUnit;
bool g_hideUsed = false;
std::unordered_map<std::string, bool> g_hasLarge; // texture path -> its _large version exists
std::mt19937 g_rng(static_cast<unsigned>(GetTickCount64()));
bool g_dirty = true;
bool g_applying = false;

void* CurrentTrooper()
{
    void* gameGUI = game::GameGUI();
    return gameGUI ? game::Field<void*>(gameGUI, game::off.customizeSelection) : nullptr;
}

void ShowIf(void* item, bool show)
{
    if (!item)
        return;
    if (show)
        game::fn.itemShow(item);
    else
        game::fn.itemHide(item);
}

int MaxTop()
{
    // with headers, a sticky header can take the first slot, so allow one more row of scrolling to reach the end
    int top = std::max(0, static_cast<int>(g_rows.size()) - g_visibleRows + (g_hasHeaders ? 1 : 0));
    return std::min(scroll::Stop(g_rows, top), std::max(0, static_cast<int>(g_rows.size()) - 1));
}

// how far the thumb can travel along the track
float ThumbSpan()
{
    if (!g_track || !g_thumb)
        return 0;
    return static_cast<float>(game::Field<int>(g_track, game::off.itemSize + kSizeY) - game::Field<int>(g_thumb, game::off.itemSize + kSizeY));
}

void UpdateFilterMarkers()
{
    bool active[] = {g_unit == portraits::UnitFilter::ThisUnit, g_unit == portraits::UnitFilter::All};
    for (size_t i = 0; i < std::size(g_filters); i++)
        ShowIf(gui::Find(g_filters[i].ptr, g_filters[i].marker), active[i]);
    ShowIf(gui::Find(g_hideUsedButton, kHideUsedMarker), g_hideUsed);
}

// Clones the page's cells into the grid and places them, once (and again after a GUI reload).
void EnsureCells()
{
    if (!g_cells.empty())
        return;
    const int* gridSize = &game::Field<int>(g_grid, game::off.itemSize);
    const int* cellSize = &game::Field<int>(g_template, game::off.itemSize);
    float cellW = static_cast<float>(cellSize[0] > 0 ? cellSize[0] : kFallbackCellSize);
    float cellH = static_cast<float>(cellSize[1] > 0 ? cellSize[1] : kFallbackCellSize);
    float pitchX = cellW + kCellGap, pitchY = cellH + kCellGap;
    int cols = std::max(1, static_cast<int>(gridSize[0] / pitchX));
    int rows = std::max(1, static_cast<int>(gridSize[1] / pitchY));
    // origins are relative to the grid's center, +y up; the cells are centered as a block
    float x0 = -cols * pitchX / 2 + pitchX / 2;
    float y0 = rows * pitchY / 2 - pitchY / 2;

    void* root = game::GuiRoot();
    g_headers.clear();
    for (int row = 0; row < rows; row++) {
        if (g_headerTemplate) {
            void* header = game::fn.itemClone(g_headerTemplate);
            if (header) {
                game::fn.itemUpdatePointerReferences(header, header, root);
                game::fn.itemSetName(header, nullptr);
                gui::AddChild(g_grid, header);
                gui::SetOrigin(header, 0, y0 - row * pitchY);
            }
            g_headers.push_back(header);
        }
        for (int col = 0; col < cols; col++) {
            void* cell = game::fn.buttonClone(g_template);
            if (!cell) {
                g_cells.clear(); // an incomplete grid would misplace rows; try again next time
                g_cellPos.clear();
                return;
            }
            // what the game does for its own cloned list items: re-resolve action targets, drop the template's name
            game::fn.buttonUpdatePointerReferences(cell, cell, root);
            game::fn.itemSetName(cell, nullptr);
            gui::AddChild(g_grid, cell);
            gui::SetOrigin(cell, x0 + col * pitchX, y0 - row * pitchY);
            g_cells.push_back(cell); // stays hidden (cloned from the hidden template) until ShowRows
            g_cellPos.push_back({x0 + col * pitchX, y0 - row * pitchY});
        }
    }
    g_cols = cols;
    g_visibleRows = rows;
    g_cellW = cellW;
    g_cellH = cellH;
    g_cellEntry.assign(g_cells.size(), -1);
    // the pop-up lives in the grid too (same coordinates as the cells), added last so it draws above them
    if (g_preview)
        gui::AddChild(g_grid, g_preview);
    game::api->Log("grid: %d x %d cells", cols, rows);
}

enum class Usage { None, OtherSquad, Squad };

Usage UsageOf(const char* tex)
{
    Usage usage = Usage::None;
    for (const portraits::User& u : g_used)
        if (_stricmp(u.tex, tex) == 0)
            usage = u.sameSquad ? Usage::Squad : std::max(usage, Usage::OtherSquad);
    return usage;
}

// "Used by APOLLO, ZEUS (BRAVO TWO)": squadmates first, then other squads' troopers with their squad
std::string UsedBy(const char* tex)
{
    std::string names;
    for (bool sameSquad : {true, false}) {
        for (const portraits::User& u : g_used) {
            if (u.sameSquad != sameSquad || _stricmp(u.tex, tex) != 0)
                continue;
            names += names.empty() ? "Used by " : ", ";
            names += u.name;
            if (!sameSquad)
                names += std::string(" (") + u.squad + ")";
        }
    }
    return names;
}

void FillCell(int cellIndex, size_t entry, const char* current, bool force)
{
    void* cell = g_cells[cellIndex];
    if (!force && g_cellEntry[cellIndex] == static_cast<int>(entry))
        return; // already showing it (rows that stay on screen while scrolling)
    const portraits::Entry& e = g_entries[entry];
    void* image = gui::Find(cell, kCellImage);
    if (gui::IsA(image, game::globals.staticImageVtbl))
        game::fn.setImage(image, e.tex);
    ShowIf(gui::Find(cell, kCellSelected), _stricmp(e.tex, current) == 0);
    // dimmed (strongly for squadmates); the pop-up says who
    Usage usage = UsageOf(e.tex);
    ShowIf(gui::Find(cell, kCellUsed), usage == Usage::Squad);
    ShowIf(gui::Find(cell, kCellUsedOther), usage == Usage::OtherSquad);
    game::fn.itemShow(cell);
    g_cellEntry[cellIndex] = static_cast<int>(entry);
}

// data/textures/portraits/ranger10.dds -> data/textures/portraits/ranger10_large.dds, the name the game itself uses
// for the Customization screen's big portrait
std::string LargeTex(const char* tex)
{
    std::string s(tex);
    size_t dot = s.find_last_of('.');
    size_t slash = s.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return s + "_large";
    return s.substr(0, dot) + "_large" + s.substr(dot);
}

const char* FileName(const char* tex)
{
    const char* name = tex;
    for (const char* c = tex; *c; c++)
        if (*c == '/' || *c == '\\')
            name = c + 1;
    return name;
}

void HidePreview()
{
    g_hoverCell = -1;
    g_previewEntry = -1;
    ShowIf(g_preview, false);
}

// The hover pop-up for a cell: large portrait, file name, and who in the roster wears it. Placed beside the cell, on
// the side with more room, so it never covers the cell being hovered (that would end the hover).
void ShowPreview(int cellIndex)
{
    int entry = cellIndex >= 0 ? g_cellEntry[cellIndex] : -1;
    if (!g_preview || entry < 0) {
        HidePreview();
        return;
    }
    g_hoverCell = cellIndex;
    if (entry == g_previewEntry)
        return; // OnHover may repeat while the cursor stays on the cell
    g_previewEntry = entry;

    const portraits::Entry& e = g_entries[entry];
    // mod portraits may come without a _large version: show the small one scaled up instead of a missing texture
    std::string large = LargeTex(e.tex);
    auto known = g_hasLarge.find(large);
    bool hasLarge = known != g_hasLarge.end()
                        ? known->second
                        : g_hasLarge.emplace(large, game::fn.fileExists(*game::globals.fileManager, large.c_str())).first->second;
    void* image = hasLarge || !g_previewImageSmall ? g_previewImage : g_previewImageSmall;
    if (image)
        game::fn.setImage(image, hasLarge ? large.c_str() : e.tex);
    ShowIf(g_previewImage, image == g_previewImage);
    ShowIf(g_previewImageSmall, image == g_previewImageSmall);
    if (g_previewName)
        game::fn.staticTextChangeText(g_previewName, FileName(e.tex)); // copies it
    std::string usedBy = UsedBy(e.tex);
    if (g_previewUsed && !usedBy.empty())
        game::fn.staticTextChangeText(g_previewUsed, usedBy.c_str());
    ShowIf(g_previewUsed, !usedBy.empty());
    ShowIf(g_previewCurrent, g_builtFor && _stricmp(e.tex, portraits::TrooperTex(g_builtFor)) == 0);

    const int* gridSize = &game::Field<int>(g_grid, game::off.itemSize);
    const int* size = &game::Field<int>(g_preview, game::off.itemSize);
    const Pos& cell = g_cellPos[cellIndex];
    float dx = g_cellW / 2 + kCellGap + size[0] / 2.0f;
    float x = cell.x < 0 ? cell.x + dx : cell.x - dx;
    float yMax = std::max(0.0f, (gridSize[1] - size[1]) / 2.0f);
    gui::SetOrigin(g_preview, x, std::clamp(cell.y, -yMax, yMax));
    game::fn.itemShow(g_preview);
}

void ShowHeader(int slot, size_t group)
{
    void* header = g_headers[slot];
    if (!header)
        return;
    void* text = gui::Find(header, kHeaderText);
    if (gui::IsA(text, game::globals.staticTextVtbl)) {
        // ChangeText copies it (up to the placeholder's length)
        const portraits::Group& g = g_groups[group];
        std::string title = g.title + " (" + std::to_string(g.count) + ")";
        game::fn.staticTextChangeText(text, title.c_str());
    }
    game::fn.itemShow(header);
}

// Shows rows from g_top in the slots. When g_top is in the middle of a titled group, its header sticks to the first
// slot and the rows follow below it. force: re-skin every cell (the entries, squad or current portrait changed), not
// just the ones that now show a different entry.
void ShowRows(bool force)
{
    g_top = scroll::Stop(g_rows, std::clamp(g_top, 0, MaxTop()));
    const char* current = g_builtFor ? portraits::TrooperTex(g_builtFor) : "";
    const Row* topRow = static_cast<size_t>(g_top) < g_rows.size() ? &g_rows[g_top] : nullptr;
    bool sticky = g_hasHeaders && g_visibleRows >= 2 && topRow && !topRow->header && !g_groups[topRow->group].title.empty();
    for (int slot = 0; slot < g_visibleRows; slot++) {
        bool stickyHeader = sticky && slot == 0;
        size_t r = static_cast<size_t>(g_top) + slot - (sticky ? 1 : 0);
        const Row* row = stickyHeader ? topRow : r < g_rows.size() ? &g_rows[r] : nullptr;
        if (slot < static_cast<int>(g_headers.size())) {
            if (row && (stickyHeader || row->header))
                ShowHeader(slot, row->group);
            else
                ShowIf(g_headers[slot], false);
        }
        for (int col = 0; col < g_cols; col++) {
            int cellIndex = slot * g_cols + col;
            if (!stickyHeader && row && !row->header && static_cast<size_t>(col) < row->count) {
                FillCell(cellIndex, row->first + col, current, force);
            } else {
                game::fn.itemHide(g_cells[cellIndex]);
                g_cellEntry[cellIndex] = -1;
            }
        }
    }

    // scrollbar: thumb at the top for row 0, at the bottom for MaxTop()
    bool scrolls = MaxTop() > 0;
    ShowIf(g_track, scrolls);
    if (scrolls && g_thumb) {
        float span = ThumbSpan();
        gui::SetOrigin(g_thumb, 0, span / 2 - span * g_top / MaxTop());
    }
}

void ScrollTo(int top)
{
    top = scroll::Stop(g_rows, std::clamp(top, 0, MaxTop()));
    if (top != g_top) {
        g_top = top;
        ShowRows(false);
        if (g_hoverCell >= 0) {
            // the hovered cell now shows another portrait (or none)
            g_previewEntry = -1;
            ShowPreview(g_hoverCell);
        }
    }
}

// Every group starts on a new row, under its header when it has a title.
void BuildRows()
{
    g_rows.clear();
    g_hasHeaders = false;
    for (size_t g = 0; g < g_groups.size(); g++) {
        const portraits::Group& group = g_groups[g];
        if (!g_headers.empty() && !group.title.empty()) {
            g_rows.push_back({true, g, 0, 0});
            g_hasHeaders = true;
        }
        for (size_t first = 0; first < group.count; first += g_cols)
            g_rows.push_back({false, g, group.first + first, std::min<size_t>(g_cols, group.count - first)});
    }
}

int RowOfEntry(size_t entry)
{
    for (size_t r = 0; r < g_rows.size(); r++) {
        const Row& row = g_rows[r];
        if (!row.header && entry >= row.first && entry < row.first + row.count)
            return static_cast<int>(r);
    }
    return 0;
}

// Re-reads the pool and the squad for a trooper, then scrolls to its current portrait.
void Refresh(const void* trooper)
{
    EnsureCells();
    HidePreview();
    g_builtFor = trooper;
    g_dirty = false;
    g_used = portraits::UsedInRoster(trooper);
    portraits::Filtered filtered = portraits::ForTrooper(trooper, g_unit);
    size_t unfiltered = filtered.entries.size();
    if (g_hideUsed) {
        // keep the trooper's own portrait even if someone else wears it too
        const char* own = portraits::TrooperTex(trooper);
        filtered = portraits::Without(filtered, [&](const portraits::Entry& e) {
            return _stricmp(e.tex, own) == 0 || UsageOf(e.tex) == Usage::None;
        });
    }
    g_entries = std::move(filtered.entries);
    g_groups = std::move(filtered.groups);
    if (g_statusCount) {
        std::string count = std::to_string(g_entries.size()) + (g_entries.size() == 1 ? " PORTRAIT" : " PORTRAITS");
        game::fn.staticTextChangeText(g_statusCount, count.c_str());
    }
    if (g_statusUsed) {
        size_t used = 0;
        for (const portraits::Entry& e : g_entries)
            used += UsageOf(e.tex) != Usage::None;
        std::string line = g_hideUsed ? std::to_string(unfiltered - g_entries.size()) + " HIDDEN" : std::to_string(used) + " IN USE";
        game::fn.staticTextChangeText(g_statusUsed, line.c_str());
    }
    if (g_title) {
        std::string title = std::string("CHOOSE PORTRAIT: ") + portraits::TrooperName(trooper);
        game::fn.staticTextChangeText(g_title, title.c_str()); // copies it
    }
    if (g_cells.empty())
        return; // the grid couldn't be built; nothing to show
    BuildRows();

    // the current portrait's row near the top, with its group header just above it
    g_top = 0;
    const char* current = portraits::TrooperTex(trooper);
    for (size_t i = 0; i < g_entries.size(); i++) {
        if (_stricmp(g_entries[i].tex, current) == 0) {
            g_top = RowOfEntry(i) - 1;
            break;
        }
    }
    ShowRows(true);
}

void Apply(void* trooper, const portraits::Entry& e)
{
    portraits::Assign(trooper, e);
    game::fn.rosterSave();

    // the same refreshes as the stock PREV/NEXT arrows: the whole profile panel (large portrait, class icon)...
    void* gameGUI = game::GameGUI();
    g_applying = true;
    game::fn.customizeOnTrooperSelected(gameGUI, trooper);
    g_applying = false;

    // ...and the deploy screen's slots, which Deploy_Update draws from the selected slot's trooper
    if (game::off.deployRosterTrooper >= 0) {
        void*& slotTrooper = game::Field<void*>(gameGUI, game::off.deploySelection + game::off.deployRoster + game::off.deployRosterTrooper);
        void* saved = slotTrooper;
        slotTrooper = trooper;
        game::fn.deployUpdate(gameGUI);
        slotTrooper = saved;
    }

    ShowRows(true); // moves the current-portrait border
    game::api->Log("portrait set to %s", e.tex);
}

// --- callbacks from our XML actions: void (const GUI::sAction&) ---

void OnOpenClicked(const void*)
{
    void* trooper = CurrentTrooper();
    Picker_Attach();
    if (!trooper || !g_picker || !g_grid || !g_template)
        return;
    gui::AddChild(g_screen, g_picker); // last child: drawn and hit-tested above everything else on the screen
    // always: the portrait may have changed since last time (the stock PREV/NEXT arrows, squadmates); one screen of
    // re-skinning with cached textures is cheap
    Refresh(trooper);
    UpdateFilterMarkers();
    game::fn.itemShow(g_picker);
}

void OnFilterClicked(const void* action)
{
    void* owner = gui::ActionOwner(action);
    if (owner == g_filters[0].ptr)
        g_unit = portraits::UnitFilter::ThisUnit;
    else if (owner == g_filters[1].ptr)
        g_unit = portraits::UnitFilter::All;
    else
        return;
    if (void* trooper = CurrentTrooper())
        Refresh(trooper);
    UpdateFilterMarkers();
}

void OnHideUsedClicked(const void*)
{
    g_hideUsed = !g_hideUsed;
    if (void* trooper = CurrentTrooper())
        Refresh(trooper);
    UpdateFilterMarkers();
}

// A random portrait from the current list that nobody else wears. The picker stays open (scrolled to it), so another
// click re-rolls.
void OnRandomClicked(const void*)
{
    void* trooper = CurrentTrooper();
    if (!trooper || trooper != g_builtFor)
        return;
    const char* own = portraits::TrooperTex(trooper);
    std::vector<size_t> candidates;
    for (size_t i = 0; i < g_entries.size(); i++)
        if (_stricmp(g_entries[i].tex, own) != 0 && UsageOf(g_entries[i].tex) == Usage::None)
            candidates.push_back(i);
    if (candidates.empty())
        return;
    portraits::Entry e = g_entries[candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(g_rng)]];
    HidePreview();
    Apply(trooper, e);
    Refresh(trooper);
}

// mouse wheel over the list; scrolling inside a cell's own event is fine, cells are only re-skinned
void OnScrollDown(const void*)
{
    ScrollTo(scroll::Step(g_rows, g_top, kWheelRows));
}

void OnScrollUp(const void*)
{
    ScrollTo(scroll::Step(g_rows, g_top, -kWheelRows));
}

// Dragging the thumb: the rows follow the cursor's movement since it was pressed (GUI space is +y up, so dragging
// down is a negative delta and scrolls forward).
void OnThumbDown(const void* action)
{
    g_dragStartY = gui::ActionCursorY(action);
    g_dragStartTop = g_top;
}

void OnThumbDrag(const void* action)
{
    float span = ThumbSpan();
    if (span <= 0 || MaxTop() == 0)
        return;
    float dy = g_dragStartY - gui::ActionCursorY(action);
    ScrollTo(g_dragStartTop + static_cast<int>(dy / span * MaxTop() + (dy >= 0 ? 0.5f : -0.5f)));
}

int CellOf(const void* action)
{
    auto it = std::find(g_cells.begin(), g_cells.end(), gui::ActionOwner(action));
    return it == g_cells.end() ? -1 : static_cast<int>(it - g_cells.begin());
}

void OnCellHover(const void* action)
{
    int cell = CellOf(action);
    if (cell >= 0)
        ShowPreview(cell);
}

void OnCellHoverEnd(const void* action)
{
    if (CellOf(action) == g_hoverCell)
        HidePreview();
}

void OnCellClicked(const void* action)
{
    void* owner = gui::ActionOwner(action);
    auto it = std::find(g_cells.begin(), g_cells.end(), owner);
    void* trooper = CurrentTrooper();
    if (it == g_cells.end() || !trooper || trooper != g_builtFor)
        return;
    int idx = g_cellEntry[it - g_cells.begin()];
    if (idx < 0 || static_cast<size_t>(idx) >= g_entries.size())
        return;
    portraits::Entry e = g_entries[idx]; // Apply re-enters the hooks; don't hold a reference into g_entries
    HidePreview();
    Apply(trooper, e);
    Picker_Close();
}

} // namespace

void Picker_Attach()
{
    void* gameGUI = game::GameGUI();
    void* root = game::GuiRoot();
    if (!gameGUI || !root)
        return;
    void* screen = game::Field<void*>(gameGUI, game::off.customizationScreen);
    void* button = gui::Find(root, kOpenButton);
    void* picker = gui::Find(root, kPicker);
    if (!screen || !button || !picker)
        return; // our gui xml isn't loaded (yet)

    // other screens have a #portrait too, so look inside this one
    void* portrait = gui::Find(gui::Find(screen, "#player_profile_panel"), "#portrait");
    if (!portrait) {
        if (!g_loggedNoPortrait)
            game::api->Log("Customization screen has no #player_profile_panel > #portrait (changed by a UI mod?); picker button not added");
        g_loggedNoPortrait = true;
        return;
    }
    if (gui::Parent(button) != portrait)
        gui::AddChild(portrait, button);
    ShowIf(button, g_settings.portraits); // the settings switch
    if (gui::Parent(picker) != screen)
        gui::AddChild(screen, picker); // stays hidden until opened

    void* grid = gui::Find(picker, kGrid);
    void* tmpl = gui::Find(picker, kTemplate);
    if (!grid || !gui::IsA(tmpl, game::globals.buttonVtbl)) {
        game::api->Log("gui/portrait_selector.xml: needs %s and a Button %s", kGrid, kTemplate);
        return;
    }
    void* headerTemplate = gui::Find(picker, kHeaderTemplate);
    if (grid != g_grid || tmpl != g_template || headerTemplate != g_headerTemplate) {
        // first attach, or the GUI was reloaded: the old cells went with the old items
        g_cells.clear();
        g_cellPos.clear();
        g_headers.clear();
        g_hoverCell = -1;
        g_previewEntry = -1;
        g_dirty = true;
    }
    g_headerTemplate = headerTemplate;
    g_picker = picker;
    g_grid = grid;
    g_template = tmpl;
    g_screen = screen;
    g_track = gui::Find(picker, kTrack);
    g_thumb = gui::Find(g_track, kThumb);
    g_preview = gui::Find(picker, kPreview);
    g_previewImage = gui::Find(g_preview, kPreviewImage);
    g_previewName = gui::Find(g_preview, kPreviewName);
    g_previewUsed = gui::Find(g_preview, kPreviewUsed);
    g_previewImageSmall = gui::Find(g_preview, kPreviewImageSmall);
    if (!gui::IsA(g_previewImage, game::globals.staticImageVtbl))
        g_previewImage = nullptr;
    if (!gui::IsA(g_previewImageSmall, game::globals.staticImageVtbl))
        g_previewImageSmall = nullptr;
    g_previewCurrent = gui::Find(g_preview, kPreviewCurrent);
    g_statusCount = gui::Find(picker, kStatusCount);
    g_statusUsed = gui::Find(picker, kStatusUsed);
    if (!gui::IsA(g_statusCount, game::globals.staticTextVtbl))
        g_statusCount = nullptr;
    if (!gui::IsA(g_statusUsed, game::globals.staticTextVtbl))
        g_statusUsed = nullptr;
    g_title = gui::Find(picker, kTitle);
    if (!gui::IsA(g_title, game::globals.staticTextVtbl))
        g_title = nullptr;
    g_hideUsedButton = gui::Find(picker, kHideUsed);
    g_randomButton = gui::Find(picker, kRandom);
    if (g_hideUsedButton)
        gui::SetCallback(g_hideUsedButton, game::off.eventClick, OnHideUsedClicked);
    if (g_randomButton)
        gui::SetCallback(g_randomButton, game::off.eventClick, OnRandomClicked);
    if (!gui::IsA(g_previewName, game::globals.staticTextVtbl))
        g_previewName = nullptr;
    if (!gui::IsA(g_previewUsed, game::globals.staticTextVtbl))
        g_previewUsed = nullptr;

    if (void* b = gui::Find(button, kOpenButtonInner))
        gui::SetCallback(b, game::off.eventClick, OnOpenClicked);
    // the template's callbacks are copied into every clone
    gui::SetCallback(tmpl, game::off.eventClick, OnCellClicked);
    gui::SetCallback(tmpl, game::off.eventCursorHover, OnCellHover);
    gui::SetCallback(tmpl, game::off.eventCursorHoverEnd, OnCellHoverEnd);
    for (void* item : {tmpl, headerTemplate, grid, g_track, g_thumb}) {
        if (item) {
            gui::SetCallback(item, game::off.eventScrollDown, OnScrollDown);
            gui::SetCallback(item, game::off.eventScrollUp, OnScrollUp);
        }
    }
    if (g_thumb) {
        gui::SetCallback(g_thumb, game::off.eventCursorDown, OnThumbDown);
        gui::SetCallback(g_thumb, game::off.eventDrag, OnThumbDrag);
    }
    for (FilterButton& f : g_filters) {
        f.ptr = gui::Find(picker, f.item);
        if (f.ptr)
            gui::SetCallback(f.ptr, game::off.eventClick, OnFilterClicked);
    }
}

void Picker_OnTrooperSelected(const void* trooper)
{
    Picker_Attach();
    if (trooper != g_builtFor)
        g_dirty = true;
    if (Picker_IsOpen() && g_dirty && trooper)
        Refresh(trooper);
}

bool Picker_IsOpen()
{
    // looked up again rather than trusting g_picker, which a GUI reload would leave dangling
    void* picker = gui::Find(game::GuiRoot(), kPicker);
    return picker && !gui::IsHidden(picker);
}

void Picker_Close()
{
    HidePreview();
    void* picker = gui::Find(game::GuiRoot(), kPicker);
    if (picker && !gui::IsHidden(picker))
        game::fn.itemHide(picker);
}

bool Picker_IsApplying()
{
    return g_applying;
}

bool Picker_StepPortrait(bool next)
{
    if (!g_settings.portraits)
        return false; // switched off: the stock arrows
    // The stock arrows go wrong when the trooper's portrait isn't in its class's pool (eg. a mod's class-tagged
    // portraits, with the trooper wearing one from another class): they fall back to library entry 0, then accept any
    // portrait of the unit, so every press lands on the same first/last one (see CLAUDE.md).
    void* trooper = CurrentTrooper();
    if (!trooper)
        return false;
    std::vector<portraits::Entry> pool = portraits::ForTrooper(trooper, portraits::UnitFilter::ThisUnit).entries;
    if (pool.empty())
        return false;
    const char* current = portraits::TrooperTex(trooper);
    auto it = std::find_if(pool.begin(), pool.end(), [&](const portraits::Entry& e) { return _stricmp(e.tex, current) == 0; });
    size_t n = pool.size();
    size_t idx;
    if (it == pool.end())
        idx = next ? 0 : n - 1; // outside the pool: back into it
    else
        idx = (static_cast<size_t>(it - pool.begin()) + (next ? 1 : n - 1)) % n;
    Apply(trooper, pool[idx]);
    return true;
}

void Picker_SetEnabled(bool on)
{
    if (!on)
        Picker_Close();
    // once on the Customization screen's portrait the button follows the switch right away; before that,
    // Picker_Attach shows it or not
    void* button = gui::Find(game::GuiRoot(), kOpenButton);
    if (button && gui::Parent(button) != game::GuiRoot())
        ShowIf(button, on);
}
