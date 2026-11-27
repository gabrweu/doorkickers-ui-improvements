// The mission action wheel (right-click on a trooper or a waypoint) on one page: the target page's actions (door,
// window, enemy, hostage, ...) on an outer ring around the path page (reload, sprint, grenades, go-codes, ...).
#pragma once

// GameGUI::OpenWaypointMenu(wp, type) just ran. After an open with a second page (a "more" button showing), builds
// the other page too and merges both onto the wheel.
void Wheel_OnOpened(const void* wp, int type);
// GameGUI::WaypointMenu_OnEvent(eventId, params) is about to run: a click on a target-page button gets the target
// that page was built for.
void Wheel_OnEvent(int eventId, const void* params);
// The game reloaded its GUI (the loader's GUI_LOADED event): our items are found again on the next open.
void Wheel_OnGuiLoaded();
