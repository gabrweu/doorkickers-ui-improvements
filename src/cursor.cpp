// The mouse cursor. The game registers its window class with LoadCursorA(NULL, IDC_ARROW) and never calls SetCursor
// (it doesn't import it) or draws a cursor of its own, so Windows shows the class cursor (DefWindowProc's
// WM_SETCURSOR) in menus and missions alike. Replacing the window class's cursor (SetClassLongPtrW GCLP_HCURSOR)
// changes it everywhere, with no hooks.
//
// The window doesn't exist yet while plugins initialize, and its size changes with the resolution / window mode, so
// a small background thread looks for it and picks the cursor size from the window's height (the .cur holds 32, 48,
// 64 and 96 px images, drawn by tools/make_art.ps1).
#include "cursor.h"

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "game.h"
#include "settings.h"

namespace {

std::wstring g_path;

struct Search {
    DWORD pid;
    HWND found;
    LONG bestArea;
};

// the game's main window: this process's largest visible, unowned top-level window
BOOL CALLBACK OnWindow(HWND hwnd, LPARAM param)
{
    auto* s = reinterpret_cast<Search*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != s->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
        return TRUE;
    RECT r;
    if (!GetClientRect(hwnd, &r))
        return TRUE;
    LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area > s->bestArea) {
        s->bestArea = area;
        s->found = hwnd;
    }
    return TRUE;
}

HWND GameWindow()
{
    Search s{GetCurrentProcessId(), nullptr, 0};
    EnumWindows(OnWindow, reinterpret_cast<LPARAM>(&s));
    return s.found;
}

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

// Sets the window class's cursor and shows it now rather than on the next mouse move: WM_SETCURSOR makes the window's
// own thread pick it up (SetCursor from this thread wouldn't), and only matters while the cursor is over the game.
void Apply(HWND hwnd, HCURSOR cursor)
{
    SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
    POINT p;
    if (GetCursorPos(&p) && WindowFromPoint(p) == hwnd)
        PostMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

DWORD WINAPI Watch(LPVOID)
{
    HWND applied = nullptr; // where our cursor is set, null while the Windows arrow is
    int appliedSize = 0;
    HCURSOR ours = nullptr;
    for (;;) {
        Sleep(500);
        HWND hwnd = GameWindow();
        if (!hwnd)
            continue;
        if (!g_settings.cursor) {
            // switched off (settings panel): back to the arrow the game registered
            if (applied) {
                Apply(applied, LoadCursorW(nullptr, IDC_ARROW));
                if (ours)
                    DestroyCursor(ours);
                ours = nullptr;
                applied = nullptr;
                appliedSize = 0;
                game::api->Log("cursor: off, Windows arrow");
            }
            continue;
        }
        RECT r;
        GetClientRect(hwnd, &r);
        int size = SizeFor(r.bottom - r.top);
        if (hwnd == applied && size == appliedSize)
            continue;
        auto cursor = static_cast<HCURSOR>(LoadImageW(nullptr, g_path.c_str(), IMAGE_CURSOR, size, size, LR_LOADFROMFILE));
        if (!cursor) {
            game::api->Log("cursor: couldn't load %ls (error %lu)", g_path.c_str(), GetLastError());
            return 0;
        }
        Apply(hwnd, cursor);
        if (ours)
            DestroyCursor(ours); // ours only, never the system arrow the class started with
        ours = cursor;
        applied = hwnd;
        appliedSize = size;
        game::api->Log("cursor: %d px (window height %ld)", size, r.bottom - r.top);
    }
}

} // namespace

void Cursor_Start(const wchar_t* modDir)
{
    if (!modDir || !*modDir)
        return; // loaded from mods_native: no mod folder with the cursor
    g_path = std::wstring(modDir) + L"cursor\\eras_cursor.cur";
    if (GetFileAttributesW(g_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        game::api->Log("cursor: %ls not found; keeping the Windows cursor", g_path.c_str());
        return;
    }
    if (HANDLE thread = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr))
        CloseHandle(thread);
}
