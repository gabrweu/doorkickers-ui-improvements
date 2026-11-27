// Trooper Toolbox's main-page background while its pages are merged (ttmerge.cpp): our own art without TT's baked-in
// slot frames, and a frame on each slot instead, so a hidden slot leaves no empty box.
#pragma once

// The pages are merged (every TtMerge_Apply, after the merged slots got their main-page size): swaps the background
// and frames the slots (once per GUI), then keeps them shown. screen is the Customization screen, inventory TT's
// #inventory_panel. Leaves TT's background alone if anything it needs is missing.
void TtBg_Apply(void* screen, void* inventory);
// The merge was switched off: TT's own background back, our frames hidden.
void TtBg_Remove();
// GUI::Item::Show is about to run on item: true for TT's background image while ours replaces it.
bool TtBg_SkipShow(const void* item);
// The game reloaded its GUI: forgets the old items.
void TtBg_OnGuiLoaded();
