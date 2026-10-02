// The equipment grid. gui/equipment_picker.xml defines it as a hidden top-level item, so the mod doesn't replace
// customization.xml (other UI mods do). The first time the stock list is built, it's moved next to the stock list
// (into #equipment_list_panel) with the game's AddChild action, at the same place on screen.
//
// The game still builds its own list: on GUI_GAME_CUSTOMIZE_INVENTORY_SELECT it clones #equipment_item_template once
// per item the trooper's class may use, fills them (#id, #name, unlock cost, equipped mark), then calls
// ItemList::Init. Our hook runs after that, reads the items back (entries.cpp), hides the stock list and shows the
// same items as a grid. Nothing about equipping is ours: a grid cell has its own hidden #id StaticText and the stock
// list item's XML actions (TriggerEvent HOVER_LIST_ITEM / _END and ITEM_TRY_EQUIP). The game takes the item from the
// event's owner, which for a cloned cell is the cell, and reads the template name from its #id, so hover details,
// unlock costs, the unlock pop-up and equipping are all the stock code.
//
// The list is virtualized like the portrait selector's: the content is a list of rows, each either a group header or
// up to `cols` entries of one group, and only one screen of row slots exists. Headers are shorter than cell rows, so
// every visible row is placed from the top as it's shown. Cells and headers are cloned from hidden templates once
// per GUI load and re-skinned while scrolling, and never deleted (an item must outlive its own events). It scrolls
// in whole rows: nothing clips a partly visible row.
//
// Two cell layouts, each with its own template and pool of cells: wide cells (3 columns) for lists that are mostly
// firearms, whose icons are long and flat, and square tiles (4 columns) for everything else (armor, grenades,
// helmets, attachments, cosmetics). The header pool is shared.
#include "grid.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

#include "entries.h"
#include "game.h"
#include "gui.h"
#include "scroll.h"
#include "settings.h"

namespace {

constexpr const char* kRoot = "#dk2ml_ep_root";
constexpr const char* kGrid = "#dk2ml_ep_grid";
constexpr const char* kCellTemplate = "#dk2ml_ep_cell_template"; // wide (weapons)
constexpr const char* kTileTemplate = "#dk2ml_ep_tile_template"; // square (everything else); optional
constexpr const char* kCellIconBox = "#dk2ml_ep_cell_iconbox"; // the area the icon is fitted into
constexpr const char* kCellImage = "#dk2ml_ep_cell_img";
constexpr const char* kCellName = "#dk2ml_ep_cell_name";
constexpr const char* kCellId = "#id"; // the name the game's own list code looks for
constexpr const char* kCellEquipped = "#dk2ml_ep_cell_equipped";
constexpr const char* kCellLocked = "#dk2ml_ep_cell_locked";
constexpr const char* kCellStars = "#dk2ml_ep_cell_stars";
constexpr const char* kCellHonors = "#dk2ml_ep_cell_bh";
constexpr const char* kCellCost = "#dk2ml_ep_cell_cost";
constexpr const char* kCellUsed = "#dk2ml_ep_cell_used"; // a squadmate has it equipped
constexpr const char* kUsedBy = "#dk2ml_ep_usedby";       // hover pop-up: who
constexpr const char* kUsedByText = "#dk2ml_ep_usedby_text";
constexpr const char* kUsedByTitle = "#dk2ml_ep_usedby_title";
constexpr const char* kUsedByImage = "#dk2ml_ep_usedby_img";
constexpr const char* kUsedByBorder = "#dk2ml_ep_usedby_border";
constexpr const char* kUsedByBg = "#dk2ml_ep_usedby_bg";
constexpr const char* kMore = "#dk2ml_ep_more"; // "N MORE BELOW" under the last row when rows didn't fit
constexpr const char* kMoreText = "#dk2ml_ep_more_text";
constexpr const char* kHeaderTemplate = "#dk2ml_ep_header_template";
constexpr const char* kHeaderText = "#dk2ml_ep_header_text";
constexpr const char* kTrack = "#dk2ml_ep_track";
constexpr const char* kThumb = "#dk2ml_ep_thumb";
constexpr const char* kStockPanel = "#equipment_list_panel";
constexpr const char* kStockList = "#equipment_list_parent";
constexpr const char* kStockDetails = "#equipment_details_panel";
constexpr float kCellGap = 8;
constexpr float kHeaderGap = 4;
constexpr int kWheelRows = 1; // card rows per mouse wheel notch (headers don't count: scroll.h)
constexpr int32_t kSizeY = sizeof(int); // GUI::Item::m_size is int[2] {x, y}

// A cell layout: its template, and one screen of cells cloned from it (once per GUI load, on first use).
struct Layout {
    const char* templateName;
    void* tmpl = nullptr;
    gui::TextFit fit;           // the name label's measured line heights (gui::SetTextFitted)
    float iconMaxScale = 0;     // the template icon's XML scale: icons are never drawn larger than that
    std::vector<void*> cells;   // cols per cell-row slot, row by row
    std::vector<int> cellEntry; // entry index each cell shows, -1 if hidden
    std::vector<float> cellY;   // each cell's current y origin (only re-set when it changes)
    std::vector<float> colX;
    int cols = 0;
    float cellPitch = 0;        // a cell row's height, gap included
};

Layout g_layouts[] = {
    {kCellTemplate},
    {kTileTemplate},
};
Layout* g_layout = &g_layouts[0]; // the one showing

void* g_root = nullptr;
void* g_grid = nullptr;
void* g_headerTemplate = nullptr;
void* g_track = nullptr;
void* g_thumb = nullptr;
void* g_stockList = nullptr;          // the stock list we sit next to
std::vector<void*> g_hiddenStock;     // stock items we hid (the list and its scroll arrows), shown again when inactive
void* g_usedBy = nullptr; // the "used by" pop-up (optional), moved into the grid above the cells
void* g_more = nullptr;   // the "more below" hint (optional), moved into the grid
size_t g_moreCount = 0;  // the count its text shows
bool g_active = false;
bool g_loggedNoTemplates = false;

std::vector<void*> g_headers; // header slots, shared by the layouts
std::vector<float> g_headerY;
float g_gridH = 0;
float g_headerPitch = 0;

// A row of the list: header == true -> the group's header, else the group's entries [first, first + count)
struct Row {
    bool header;
    size_t group;
    size_t first, count;
};

entries::List g_list;
std::vector<Row> g_rows;
int g_top = 0;          // first visible row
float g_dragStartY = 0; // thumb drag: cursor y and g_top when it was pressed
int g_dragStartTop = 0;

float Height(const Row& row)
{
    return row.header ? g_headerPitch : g_layout->cellPitch;
}

// The slot's Empty choice (the NONE group, always first) gets no header: just its cell.
bool Headerless(size_t group)
{
    const entries::Group& g = g_list.groups[group];
    return g.count > 0 && g_list.entries[g.first].type == game::off.eqpEmpty;
}

// When the top row is in the middle of a group, that group's header sticks to the top of the grid.
bool Sticky(int top)
{
    return !g_headers.empty() && static_cast<size_t>(top) < g_rows.size() && !g_rows[top].header &&
           !Headerless(g_rows[top].group);
}

// Whether everything from row `top` to the end fits on one screen.
bool FitsFrom(int top)
{
    float h = Sticky(top) ? g_headerPitch : 0;
    for (size_t r = static_cast<size_t>(top); r < g_rows.size(); r++)
        h += Height(g_rows[r]);
    return h <= g_gridH + kCellGap; // the last row's gap may hang over the bottom
}

int MaxTop()
{
    int top = static_cast<int>(g_rows.size());
    while (top > 0 && FitsFrom(top - 1))
        top--;
    return scroll::Stop(g_rows, std::min(top, std::max(0, static_cast<int>(g_rows.size()) - 1)));
}

// how far the thumb can travel along the track
float ThumbSpan()
{
    if (!g_track || !g_thumb)
        return 0;
    return static_cast<float>(game::Field<int>(g_track, game::off.itemSize + kSizeY) - game::Field<int>(g_thumb, game::off.itemSize + kSizeY));
}

// Clones the header slots into the grid, once (and again after a GUI reload). Enough for a screen of nothing but
// headers, so any layout's rows fit.
bool EnsureHeaders()
{
    if (!g_headers.empty())
        return true;
    const int* gridSize = &game::Field<int>(g_grid, game::off.itemSize);
    const int* headerSize = &game::Field<int>(g_headerTemplate, game::off.itemSize);
    if (gridSize[0] <= 0 || gridSize[1] <= 0 || headerSize[1] <= 0) {
        game::api->Log("gui/equipment_picker.xml: the grid and header template need a size");
        return false;
    }
    g_gridH = static_cast<float>(gridSize[1]);
    g_headerPitch = headerSize[1] + kHeaderGap;
    int headers = static_cast<int>(g_gridH / g_headerPitch) + 1;
    void* root = game::GuiRoot();
    for (int i = 0; i < headers; i++) {
        void* header = game::fn.itemClone(g_headerTemplate);
        if (!header)
            break;
        game::fn.itemUpdatePointerReferences(header, header, root);
        game::fn.itemSetName(header, nullptr);
        gui::AddChild(g_grid, header);
        g_headers.push_back(header);
    }
    g_headerY.assign(g_headers.size(), NAN);
    return !g_headers.empty();
}

// Clones one screen of a layout's cells into the grid, the first time the layout is used (and again after a GUI
// reload). Cells get their x origin here; y is set as rows are shown.
bool EnsureCells(Layout& layout)
{
    if (!layout.cells.empty())
        return true;
    if (!layout.tmpl)
        return false;
    const int* gridSize = &game::Field<int>(g_grid, game::off.itemSize);
    const int* cellSize = &game::Field<int>(layout.tmpl, game::off.itemSize);
    if (cellSize[0] <= 0 || cellSize[1] <= 0) {
        game::api->Log("gui/equipment_picker.xml: %s needs a size", layout.templateName);
        return false;
    }
    float pitchX = cellSize[0] + kCellGap;
    float pitchY = cellSize[1] + kCellGap;
    int cols = std::max(1, static_cast<int>((gridSize[0] + kCellGap) / pitchX));
    int cellRows = std::max(1, static_cast<int>((g_gridH + kCellGap) / pitchY));

    void* root = game::GuiRoot();
    std::vector<void*> cells;
    for (int i = 0; i < cols * cellRows; i++) {
        void* cell = game::fn.buttonClone(layout.tmpl);
        if (!cell)
            break;
        // what the game does for its own cloned list items: re-resolve action targets, drop the template's name
        game::fn.buttonUpdatePointerReferences(cell, cell, root);
        game::fn.itemSetName(cell, nullptr);
        gui::AddChild(g_grid, cell);
        cells.push_back(cell); // stays hidden (cloned from the hidden template) until ShowRows
    }
    if (static_cast<int>(cells.size()) < cols) {
        game::api->Log("couldn't clone %s", layout.templateName);
        return false; // the clones stay in the grid, hidden; try again next time
    }
    cells.resize(cells.size() / cols * cols); // whole rows only
    layout.cells = std::move(cells);
    layout.cols = cols;
    layout.cellPitch = pitchY;
    layout.colX.clear();
    for (int col = 0; col < cols; col++)
        layout.colX.push_back(-cols * pitchX / 2 + pitchX / 2 + col * pitchX); // centered as a block
    layout.cellEntry.assign(layout.cells.size(), -1);
    layout.cellY.assign(layout.cells.size(), NAN);
    game::api->Log("%s: %d columns, %d cell rows", layout.templateName, cols, static_cast<int>(layout.cells.size()) / cols);
    return true;
}

void HideCells(Layout& layout)
{
    for (size_t i = 0; i < layout.cells.size(); i++) {
        if (layout.cellEntry[i] >= 0)
            game::fn.itemHide(layout.cells[i]);
        layout.cellEntry[i] = -1;
    }
}

void Place(void* item, float x, float y, float& current)
{
    if (current != y) { // NaN (never placed) compares unequal too
        gui::SetOrigin(item, x, y);
        current = y;
    }
}

// An item name in at most two lines of the label (gui::SetTextFitted); the full name is in the details panel on hover.
void SetName(Layout& l, void* label, const std::string& name)
{
    bool measured = l.fit.oneLine != 0;
    gui::SetTextFitted(label, name, l.fit);
    if (!measured)
        game::api->Log("%s names: one line %d, two lines %d", l.templateName, l.fit.oneLine, l.fit.twoLines);
}

// Scales an icon to fit the cell's icon box, never above the XML scale.
void FitIcon(void* cell, void* image)
{
    void* box = gui::Find(cell, kCellIconBox);
    if (!box)
        return;
    const int* boxSize = &game::Field<int>(box, game::off.itemSize);
    gui::FitImage(image, static_cast<float>(boxSize[0]), static_cast<float>(boxSize[1]), g_layout->iconMaxScale);
}

void FillCell(int cellIndex, size_t entry, bool force)
{
    Layout& l = *g_layout;
    void* cell = l.cells[cellIndex];
    if (force || l.cellEntry[cellIndex] != static_cast<int>(entry)) {
        const entries::Entry& e = g_list.entries[entry];
        void* image = gui::Find(cell, kCellImage);
        bool hasImage = gui::IsA(image, game::globals.staticImageVtbl) && e.image && *e.image;
        if (hasImage) {
            game::fn.setImage(image, e.image);
            FitIcon(cell, image);
        }
        gui::ShowIf(image, hasImage);
        // ChangeText copies the text (up to the placeholder's length)
        if (void* id = gui::Find(cell, kCellId); gui::IsA(id, game::globals.staticTextVtbl))
            game::fn.staticTextChangeText(id, e.id.c_str());
        if (void* name = gui::Find(cell, kCellName); gui::IsA(name, game::globals.staticTextVtbl))
            SetName(l, name, e.name);
        gui::ShowIf(gui::Find(cell, kCellEquipped), e.equipped);
        gui::ShowIf(gui::Find(cell, kCellUsed), !e.usedBy.empty());
        gui::ShowIf(gui::Find(cell, kCellLocked), e.locked);
        if (e.locked) {
            gui::ShowIf(gui::Find(cell, kCellStars), !e.battleHonors);
            gui::ShowIf(gui::Find(cell, kCellHonors), e.battleHonors);
            if (void* cost = gui::Find(cell, kCellCost); gui::IsA(cost, game::globals.staticTextVtbl))
                game::fn.staticTextChangeText(cost, e.cost.c_str());
        }
        l.cellEntry[cellIndex] = static_cast<int>(entry);
    }
    game::fn.itemShow(cell);
}

void ShowHeader(int slot, size_t group, float y)
{
    void* header = g_headers[slot];
    void* text = gui::Find(header, kHeaderText);
    if (gui::IsA(text, game::globals.staticTextVtbl)) {
        const entries::Group& g = g_list.groups[group];
        std::string title = g.title + " (" + std::to_string(g.count) + ")";
        game::fn.staticTextChangeText(text, title.c_str());
    }
    Place(header, 0, y, g_headerY[slot]);
    game::fn.itemShow(header);
}

float MoreHeight()
{
    return g_more ? static_cast<float>(game::Field<int>(g_more, game::off.itemSize + kSizeY)) : 0;
}

// The first row from `top` that doesn't fit in `height` (from the grid's top): a sticky header first, then whole rows
// (the last row's gap may hang over the bottom), within the cloned header and cell pools.
size_t VisibleEnd(int top, float height)
{
    const Layout& l = *g_layout;
    int headers = static_cast<int>(g_headers.size());
    int cellRows = static_cast<int>(l.cells.size()) / l.cols;
    float used = 0;
    if (Sticky(top)) {
        used += g_headerPitch;
        headers--;
    }
    size_t r = static_cast<size_t>(top);
    for (; r < g_rows.size(); r++) {
        const Row& row = g_rows[r];
        if (used + Height(row) > height + kCellGap)
            break;
        if (row.header ? headers-- <= 0 : cellRows-- <= 0)
            break;
        used += Height(row);
    }
    return r;
}

// "N MORE BELOW" pinned to the grid's bottom while rows from `end` on aren't shown (ShowRows keeps its space free).
// Only whole rows are shown: nothing would clip a cut-off one.
void ShowMore(size_t end)
{
    if (!g_more)
        return;
    size_t hidden = 0;
    for (size_t r = end; r < g_rows.size(); r++)
        hidden += g_rows[r].header ? 0 : g_rows[r].count;
    float h = MoreHeight();
    if (hidden == 0 || h <= 0) {
        gui::ShowIf(g_more, false);
        return;
    }
    if (hidden != g_moreCount) {
        if (void* text = gui::Find(g_more, kMoreText); gui::IsA(text, game::globals.staticTextVtbl)) {
            std::string s = std::to_string(hidden) + " MORE BELOW";
            game::fn.staticTextChangeText(text, s.c_str());
        }
        g_moreCount = hidden;
    }
    gui::SetOrigin(g_more, 0, -g_gridH / 2 + h / 2);
    gui::ShowIf(g_more, true);
}

// Shows rows from g_top, placed top to bottom. force: re-skin every cell (the entries changed), not just the ones
// that now show a different entry.
void ShowRows(bool force)
{
    g_top = scroll::Stop(g_rows, std::clamp(g_top, 0, MaxTop()));
    Layout& l = *g_layout;
    // which rows: if some don't fit, keep the bottom free for "MORE BELOW", and don't end on a header whose cards
    // didn't fit (its group waits for the scroll)
    size_t end = VisibleEnd(g_top, g_gridH);
    if (end < g_rows.size() && MoreHeight() > 0)
        end = VisibleEnd(g_top, g_gridH - MoreHeight() - kCellGap);
    while (end > static_cast<size_t>(g_top) + 1 && g_rows[end - 1].header)
        end--;

    float y = g_gridH / 2; // top edge; origins are relative to the grid's center, +y up
    int header = 0, cellRow = 0;
    int cellRows = static_cast<int>(l.cells.size()) / l.cols;
    if (Sticky(g_top)) {
        ShowHeader(header++, g_rows[g_top].group, y - g_headerPitch / 2);
        y -= g_headerPitch;
    }
    for (size_t r = static_cast<size_t>(g_top); r < end; r++) {
        const Row& row = g_rows[r];
        float h = Height(row);
        if (row.header) {
            if (header >= static_cast<int>(g_headers.size()))
                break;
            ShowHeader(header++, row.group, y - h / 2);
        } else {
            if (cellRow >= cellRows)
                break;
            float cy = y - (h - kCellGap) / 2;
            for (int col = 0; col < l.cols; col++) {
                int cellIndex = cellRow * l.cols + col;
                if (static_cast<size_t>(col) < row.count) {
                    Place(l.cells[cellIndex], l.colX[col], cy, l.cellY[cellIndex]);
                    FillCell(cellIndex, row.first + col, force);
                } else {
                    game::fn.itemHide(l.cells[cellIndex]);
                    l.cellEntry[cellIndex] = -1;
                }
            }
            cellRow++;
        }
        y -= h;
    }
    for (; header < static_cast<int>(g_headers.size()); header++)
        game::fn.itemHide(g_headers[header]);
    for (int i = cellRow * l.cols; i < static_cast<int>(l.cells.size()); i++) {
        game::fn.itemHide(l.cells[i]);
        l.cellEntry[i] = -1;
    }
    ShowMore(end);

    // scrollbar: thumb at the top for row 0, at the bottom for MaxTop()
    int maxTop = MaxTop();
    gui::ShowIf(g_track, maxTop > 0);
    if (maxTop > 0 && g_thumb) {
        float span = ThumbSpan();
        gui::SetOrigin(g_thumb, 0, span / 2 - span * g_top / maxTop);
    }
}

void ScrollTo(int top)
{
    top = scroll::Stop(g_rows, std::clamp(top, 0, MaxTop()));
    if (top != g_top) {
        g_top = top;
        gui::ShowIf(g_usedBy, false); // the hovered cell now shows something else
        ShowRows(false);
    }
}

// Every group starts on a new row, under its header (but the Empty choice has none).
void BuildRows()
{
    g_rows.clear();
    for (size_t g = 0; g < g_list.groups.size(); g++) {
        const entries::Group& group = g_list.groups[g];
        if (!Headerless(g))
            g_rows.push_back({true, g, 0, 0});
        size_t cols = static_cast<size_t>(g_layout->cols);
        for (size_t first = 0; first < group.count; first += cols)
            g_rows.push_back({false, g, group.first + first, std::min(cols, group.count - first)});
    }
}

// The first row: 0 if the equipped item is on the first screen, else its row (under its sticky header).
int InitialTop()
{
    for (size_t r = 0; r < g_rows.size(); r++) {
        const Row& row = g_rows[r];
        if (row.header)
            continue;
        for (size_t i = row.first; i < row.first + row.count; i++) {
            if (!g_list.entries[i].equipped)
                continue;
            float h = 0;
            for (size_t k = 0; k <= r; k++)
                h += Height(g_rows[k]);
            return h <= g_gridH + kCellGap ? 0 : static_cast<int>(r);
        }
    }
    return 0;
}

// Shows the stock list again (a slot the grid doesn't handle, or nothing readable).
void Deactivate()
{
    if (!g_active)
        return;
    g_active = false;
    gui::ShowIf(g_usedBy, false);
    gui::ShowIf(g_more, false);
    for (void* item : g_hiddenStock)
        game::fn.itemShow(item);
    g_hiddenStock.clear();
    gui::ShowIf(g_root, false);
}

void HideStock(void* item)
{
    if (item && !gui::IsHidden(item)) {
        game::fn.itemHide(item);
        g_hiddenStock.push_back(item);
    }
}

// Every time, in case something showed the stock items again; HideStock only records items it hid itself.
void Activate(void* stockList)
{
    g_active = true;
    HideStock(stockList);
    // the stock list's scroll arrows: unnamed stealFocus buttons beside it (they'd scroll the hidden list)
    for (void* sibling : gui::Children(gui::Parent(stockList))) {
        if (gui::IsA(sibling, game::globals.buttonVtbl) && game::Field<bool>(sibling, game::off.itemStealFocus) &&
            !*gui::Name(sibling))
            HideStock(sibling);
    }
    game::fn.itemShow(g_root);
}

// --- callbacks from our XML actions: void (const GUI::sAction&) ---

// the stock list item shows the details panel from XML (#equipment_details_panel lives in customization.xml, which
// another UI mod may replace, so it's looked up here rather than named in our XML)
void ShowUsedBy(const void* action);

void OnCellHover(const void* action)
{
    void* gameGUI = game::GameGUI();
    void* screen = gameGUI ? game::Field<void*>(gameGUI, game::off.customizationScreen) : nullptr;
    gui::ShowIf(gui::Find(screen, kStockDetails), true);
    ShowUsedBy(action);
}

void OnCellHoverEnd(const void*)
{
    gui::ShowIf(g_usedBy, false);
}

// The "used by" pop-up beside a hovered cell whose item squadmates have equipped: the item's icon, "USED BY N
// OPERATORS" and their names, its height fitted to the names. On the side with more room, never over the cell itself
// (that would end the hover), kept within the grid's height.
namespace usedby {

constexpr float kPad = 12;       // above the icon / title, below the names
constexpr float kIconH = 64;     // the icon's box (200 x 64)
constexpr float kIconW = 200;
constexpr float kIconMaxScale = 0.6f;
constexpr float kTitleH = 30;    // title line + gap to the names
constexpr float kBorder = 2;     // the orange frame around the fill

// ChangeText lays the text out at once; with a sizeX and no sizeY, m_size.y is then the wrapped height
float SetText(void* text, const std::string& s)
{
    game::fn.staticTextChangeText(text, s.c_str());
    return static_cast<float>(game::Field<int>(text, game::off.itemSize + static_cast<int32_t>(sizeof(int))));
}

// a background StaticImage stretched (y only) to h
void Stretch(void* image, float h)
{
    if (!gui::IsA(image, game::globals.staticImageVtbl))
        return;
    char* render = &game::Field<char>(image, game::off.staticImageRender);
    float size = game::Field<float>(render, game::off.renderSize + static_cast<int32_t>(sizeof(float)));
    if (size <= 0)
        return;
    game::Field<float>(render, game::off.renderScale + static_cast<int32_t>(sizeof(float))) = h / size;
    game::Field<bool>(render, game::off.renderNeedsUpdate) = true;
}

// The names, as many as fit in maxH, the rest as "+N MORE". Returns the text's height.
float SetNames(void* text, const std::vector<std::string>& names, float maxH)
{
    auto join = [&](size_t n) {
        std::string s;
        for (size_t i = 0; i < n; i++)
            s += (i ? ", " : "") + names[i];
        if (n < names.size())
            s += " +" + std::to_string(names.size() - n) + " MORE";
        return s;
    };
    float h = SetText(text, join(names.size()));
    for (size_t n = names.size(); h > maxH && n > 1;)
        h = SetText(text, join(--n));
    return h;
}

} // namespace usedby

void ShowUsedBy(const void* action)
{
    using namespace usedby;
    gui::ShowIf(g_usedBy, false);
    Layout& l = *g_layout;
    void* cell = gui::ActionOwner(action);
    auto it = std::find(l.cells.begin(), l.cells.end(), cell);
    if (!g_usedBy || it == l.cells.end())
        return;
    size_t index = static_cast<size_t>(it - l.cells.begin());
    int entry = l.cellEntry[index];
    if (entry < 0 || g_list.entries[entry].usedBy.empty())
        return;
    const entries::Entry& e = g_list.entries[entry];
    void* text = gui::Find(g_usedBy, kUsedByText);
    void* title = gui::Find(g_usedBy, kUsedByTitle);
    void* image = gui::Find(g_usedBy, kUsedByImage);
    if (!gui::IsA(text, game::globals.staticTextVtbl))
        return;

    // top down, y from the pop-up's top
    float y = kPad;
    bool icon = e.image && *e.image && gui::IsA(image, game::globals.staticImageVtbl);
    if (icon) {
        game::fn.setImage(image, e.image);
        gui::FitImage(image, kIconW, kIconH, kIconMaxScale);
        y += kIconH;
    }
    gui::ShowIf(image, icon);
    float titleY = y;
    if (gui::IsA(title, game::globals.staticTextVtbl)) {
        size_t n = e.usedBy.size();
        std::string s = "USED BY " + std::to_string(n) + (n == 1 ? " OPERATOR" : " OPERATORS");
        game::fn.staticTextChangeText(title, s.c_str());
        y += kTitleH;
    }
    float textY = y;
    float maxText = std::max(60.0f, g_gridH - textY - kPad - 2 * kBorder);
    float h = std::ceil(textY + SetNames(text, e.usedBy, maxText) + kPad);

    // the pop-up and its backgrounds at that height, then everything re-placed from the new top
    int* size = &game::Field<int>(g_usedBy, game::off.itemSize);
    size[1] = static_cast<int>(h);
    Stretch(gui::Find(g_usedBy, kUsedByBorder), h + 2 * kBorder);
    Stretch(gui::Find(g_usedBy, kUsedByBg), h);
    if (icon)
        gui::SetOrigin(image, 0, h / 2 - kPad - kIconH / 2); // not aligned: from the pop-up's center
    if (title)
        gui::SetOrigin(title, 0, -titleY);                   // align="t": from the top
    gui::SetOrigin(text, 0, -textY);

    float cellW = static_cast<float>(game::Field<int>(l.tmpl, game::off.itemSize));
    float x = l.colX[index % l.cols];
    float dx = cellW / 2 + kCellGap + size[0] / 2.0f;
    float yMax = std::max(0.0f, (g_gridH - size[1]) / 2.0f);
    gui::SetOrigin(g_usedBy, x < 0 ? x + dx : x - dx, std::clamp(l.cellY[index], -yMax, yMax));
    game::fn.itemShow(g_usedBy);
}

// mouse wheel over the grid; scrolling inside a cell's own event is fine, cells are only re-skinned
void OnScrollDown(const void*)
{
    ScrollTo(scroll::Step(g_rows, g_top, kWheelRows));
}

void OnScrollUp(const void*)
{
    ScrollTo(scroll::Step(g_rows, g_top, -kWheelRows));
}

// the "more below" hint: one wheel notch down
void OnMoreClick(const void*)
{
    ScrollTo(scroll::Step(g_rows, g_top, kWheelRows));
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
    int maxTop = MaxTop();
    if (span <= 0 || maxTop == 0)
        return;
    float dy = g_dragStartY - gui::ActionCursorY(action);
    ScrollTo(g_dragStartTop + static_cast<int>(std::lround(dy / span * maxTop)));
}

// Finds our items and moves the grid next to the stock list. False if our gui XML isn't loaded or is incomplete.
bool Attach(void* stockList)
{
    void* root = gui::Find(game::GuiRoot(), kRoot);
    void* grid = gui::Find(root, kGrid);
    void* templates[std::size(g_layouts)];
    for (size_t i = 0; i < std::size(g_layouts); i++) {
        templates[i] = gui::Find(root, g_layouts[i].templateName);
        if (!gui::IsA(templates[i], game::globals.buttonVtbl))
            templates[i] = nullptr;
    }
    void* headerTemplate = gui::Find(root, kHeaderTemplate);
    if (!grid || !templates[0] || !headerTemplate) {
        if (!g_loggedNoTemplates)
            game::api->Log("gui/equipment_picker.xml: needs %s with %s, a Button %s and %s", kRoot, kGrid, kCellTemplate, kHeaderTemplate);
        g_loggedNoTemplates = true;
        return false;
    }
    bool changed = root != g_root || grid != g_grid || headerTemplate != g_headerTemplate || stockList != g_stockList;
    for (size_t i = 0; i < std::size(g_layouts); i++)
        changed |= templates[i] != g_layouts[i].tmpl;
    if (changed) {
        // first attach, or the GUI was reloaded: the old cells and hidden stock items went with the old items
        g_headers.clear();
        g_hiddenStock.clear();
        g_active = false;
        g_root = root;
        g_grid = grid;
        g_headerTemplate = headerTemplate;
        g_stockList = stockList;
        g_track = gui::Find(root, kTrack);
        g_thumb = gui::Find(g_track, kThumb);
        g_usedBy = gui::Find(root, kUsedBy);
        g_more = gui::Find(root, kMore);
        g_moreCount = 0;
        if (g_more)
            gui::SetCallback(g_more, game::off.eventClick, OnMoreClick);
        for (size_t i = 0; i < std::size(g_layouts); i++) {
            Layout& l = g_layouts[i];
            l.tmpl = templates[i];
            l.cells.clear();
            l.cellEntry.clear();
            l.cellY.clear();
            l.fit = {}; // the XML may have changed the label
            if (!l.tmpl)
                continue;
            // the template's icon is never re-skinned, so its scale is still the one from the XML
            void* icon = gui::Find(l.tmpl, kCellImage);
            l.iconMaxScale = gui::IsA(icon, game::globals.staticImageVtbl)
                                 ? game::Field<float>(icon, game::off.staticImageRender + game::off.renderScale)
                                 : 0;
            // the template's callbacks are copied into every clone
            gui::SetCallback(l.tmpl, game::off.eventCursorHover, OnCellHover);
            gui::SetCallback(l.tmpl, game::off.eventCursorHoverEnd, OnCellHoverEnd);
            gui::SetCallback(l.tmpl, game::off.eventScrollDown, OnScrollDown);
            gui::SetCallback(l.tmpl, game::off.eventScrollUp, OnScrollUp);
        }
        g_layout = &g_layouts[0];
        for (void* item : {headerTemplate, grid, g_track, g_thumb, g_more}) {
            if (item) {
                gui::SetCallback(item, game::off.eventScrollDown, OnScrollDown);
                gui::SetCallback(item, game::off.eventScrollUp, OnScrollUp);
            }
        }
        if (g_thumb) {
            gui::SetCallback(g_thumb, game::off.eventCursorDown, OnThumbDown);
            gui::SetCallback(g_thumb, game::off.eventDrag, OnThumbDrag);
        }
    }

    void* parent = gui::Parent(stockList);
    if (gui::Parent(root) != parent) {
        gui::AddChild(parent, root); // last child: drawn and hit-tested above the panel's other items
        game::fn.itemHide(root);
        // our XML places the root exactly like the stock list (align="l" origin="4 124"); a UI mod that moved the
        // list gets the grid moved along with it
        const float* stockOrigin = &game::Field<float>(stockList, game::off.itemLocalOrigin);
        const float* origin = &game::Field<float>(root, game::off.itemLocalOrigin);
        bool sameAlign = game::Field<unsigned>(stockList, game::off.itemAlignFlags) == game::Field<unsigned>(root, game::off.itemAlignFlags);
        if (sameAlign && (stockOrigin[0] != origin[0] || stockOrigin[1] != origin[1])) {
            gui::SetOrigin(root, stockOrigin[0], stockOrigin[1]);
            game::api->Log("stock list moved (by a UI mod?): grid placed at %.0f %.0f", stockOrigin[0], stockOrigin[1]);
        }
    }
    return EnsureHeaders();
}

} // namespace

void Grid_OnListInit(void* list)
{
    if (!g_settings.grid) {
        Deactivate(); // the stock list, as without the mod
        return;
    }
    void* gameGUI = game::GameGUI();
    void* screen = gameGUI ? game::Field<void*>(gameGUI, game::off.customizationScreen) : nullptr;
    // ItemList::Init runs for every list in the game (mission select, mods, ...); only the equipment list matters
    void* stockList = gui::Find(gui::Find(screen, kStockPanel), kStockList);
    if (!list || list != stockList)
        return;
    if (!Attach(stockList))
        return;

    entries::List read;
    if (!entries::FromStockList(stockList, read)) {
        Deactivate();
        return;
    }
    // square tiles unless the list is mostly firearms (or there's no tile template)
    Layout* layout = &g_layouts[read.wide ? 0 : 1];
    if (!EnsureCells(*layout))
        layout = &g_layouts[0];
    if (!EnsureCells(*layout)) {
        Deactivate();
        return;
    }
    if (layout != g_layout)
        HideCells(*g_layout);
    g_layout = layout;
    if (g_settings.squadUsage)
        entries::MarkSquadUse(read, game::Field<void*>(gameGUI, game::off.customizeSelection));
    g_list = std::move(read);
    if (g_usedBy) {
        gui::AddChild(g_grid, g_usedBy); // last: drawn above the cells (cloned into the grid before it)
        game::fn.itemHide(g_usedBy);
    }
    if (g_more && gui::Parent(g_more) != g_grid) {
        gui::AddChild(g_grid, g_more);
        game::fn.itemHide(g_more);
        g_moreCount = 0;
    }
    BuildRows();
    Activate(stockList);
    g_top = InitialTop();
    ShowRows(true);
}

void Grid_SetEnabled(bool on)
{
    if (!on)
        Deactivate();
}
