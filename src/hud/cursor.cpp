// The mouse cursor. The game registers its window class with LoadCursorA(NULL, IDC_ARROW) and never calls SetCursor
// (it doesn't import it) or draws a cursor of its own, so Windows shows the class cursor (DefWindowProc's
// WM_SETCURSOR) in menus and missions alike. Replacing the window class's cursor (SetClassLongPtrW GCLP_HCURSOR)
// changes it everywhere, with no hooks.
//
// The window doesn't exist yet while plugins initialize, and its size changes with the resolution / window mode: the
// cursor is set on the first frame and again when the loader says the window was resized (WINDOW_RESIZED), its size
// picked from the window's height (the .cur holds 32, 48, 64 and 96 px images, drawn by tools/make_art.ps1). The
// window comes from the loader (GetGameWindow). This used to be a thread of our own polling twice a second.
#include "hud/cursor.h"

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "core/game.h"
#include "core/settings.h"

namespace {

std::wstring g_path;   // empty: no cursor file, nothing to do
HWND g_applied = nullptr; // where our cursor is set, null while the Windows arrow is
int g_appliedSize = 0;
HCURSOR g_ours = nullptr;
bool g_failed = false;   // the file didn't load: stop trying
bool g_resized = false;  // WINDOW_RESIZED since the cursor was set: check its size

// about as visible as the UI, which scales with the screen: 1080p -> 32, 1440p -> 48, 4K -> 64
int SizeFor(int height)
{
    if (height < 1300)
        return 32;
    if (height < 1900)
        return 48;
    if (height < 2600)
        return 64;
    return 96;
}

// Sets the window class's cursor and shows it now rather than on the next mouse move (WM_SETCURSOR makes the window
// pick it up), which only matters while the cursor is over the game.
void Apply(HWND hwnd, HCURSOR cursor)
{
    SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
    POINT p;
    if (GetCursorPos(&p) && WindowFromPoint(p) == hwnd)
        PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

} // namespace

void Cursor_Start(const wchar_t* modDir)
{
    if (!modDir || !*modDir)
        return; // loaded from mods_native: no mod folder with the cursor
    std::wstring path = std::wstring(modDir) + L"cursor\\eras_cursor.cur";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        game::api->Log("cursor: %ls not found; keeping the Windows cursor", path.c_str());
        return;
    }
    g_path = path;
}

void Cursor_OnResized()
{
    g_resized = true;
}

void Cursor_Tick()
{
    if (g_path.empty() || g_failed)
        return;
    HWND hwnd = static_cast<HWND>(game::api->GetGameWindow()); // the loader keeps it; cheap to ask every frame
    if (!hwnd)
        return;
    if (!g_settings.cursor) {
        // switched off (settings panel): back to the arrow the game registered
        if (g_applied) {
            Apply(g_applied, LoadCursorW(nullptr, IDC_ARROW));
            if (g_ours)
                DestroyCursor(g_ours);
            g_ours = nullptr;
            g_applied = nullptr;
            g_appliedSize = 0;
            game::api->Log("cursor: off, Windows arrow");
        }
        return;
    }
    if (hwnd == g_applied && !g_resized)
        return; // nothing changed: the common case, every frame
    g_resized = false;
    RECT r;
    GetClientRect(hwnd, &r);
    int size = SizeFor(r.bottom - r.top);
    if (hwnd == g_applied && size == g_appliedSize)
        return;
    auto cursor = static_cast<HCURSOR>(LoadImageW(nullptr, g_path.c_str(), IMAGE_CURSOR, size, size, LR_LOADFROMFILE));
    if (!cursor) {
        game::api->Log("cursor: couldn't load %ls (error %lu)", g_path.c_str(), GetLastError());
        g_failed = true;
        return;
    }
    Apply(hwnd, cursor);
    if (g_ours)
        DestroyCursor(g_ours); // ours only, never the system arrow the class started with
    g_ours = cursor;
    g_applied = hwnd;
    g_appliedSize = size;
    game::api->Log("cursor: %d px (window height %ld)", size, r.bottom - r.top);
}
