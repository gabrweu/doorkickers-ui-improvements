// The mouse cursor: our own (mod/cursor/eras_cursor.cur) instead of the Windows arrow, sized to the screen.
#pragma once

// Finds the cursor file (modDir: the mod folder, with a trailing backslash). The game window doesn't exist yet while
// plugins initialize; Cursor_Tick takes it from there.
void Cursor_Start(const wchar_t* modDir);
// Every frame (the loader's FRAME event): sets our cursor on the game window the first time, re-sizes it after a
// WINDOW_RESIZED, and puts the Windows arrow back when the setting is off. Does nothing when nothing changed.
void Cursor_Tick();
void Cursor_OnResized(); // the loader's WINDOW_RESIZED event
