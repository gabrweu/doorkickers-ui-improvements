// The mouse cursor: our own (mod/cursor/eras_cursor.cur) instead of the Windows arrow, sized to the screen.
#pragma once

// Starts watching for the game window (it doesn't exist yet while plugins initialize). modDir: the mod folder, with
// a trailing backslash.
void Cursor_Start(const wchar_t* modDir);
