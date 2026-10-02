// Trooper Toolbox: the slots of its second page (Custom7-9, SupportGear2/3) on the main Customization page, and its
// "More Gear and Cosmetics" button gone.
#pragma once

// The customized trooper was (re)selected or the screen opened: merge the pages (once per GUI) and keep them merged.
// Does nothing without Trooper Toolbox or with the setting off.
void TtMerge_Apply();
// GUI::Item::Show / Hide are about to run on item (not from us or slots.cpp). True: skip it, it's Trooper Toolbox
// switching to or from its second page.
bool TtMerge_SkipShow(const void* item);
bool TtMerge_SkipHide(const void* item);
// After slots.cpp hid or showed slots: grouped merged slots (SupportGear2/3) close up, so a hidden one leaves no gap.
void TtMerge_Arrange();
// GUI::Item::Show is about to run on item: refits the merged slots' icons when the main page comes back.
void TtMerge_OnShow(const void* item);
// Settings switch: off puts the slots back on the second page and brings the button back.
void TtMerge_SetEnabled(bool on);
