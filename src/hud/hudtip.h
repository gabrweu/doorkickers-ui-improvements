// The trooper hover tooltip (HUD_Human_Tooltip): a row with Support Gear 2 / 3 (lockpick, crowbar, halligan, ...).
#pragma once

// UpdateHumanTooltip(root, ...) is about to fill the tooltip `root`: put our row into it first.
void HudTip_Before(void* root);
// It filled `root` from `equipment` (Equipment_Client[Inventory::NUM_VALUES]): names, icon fit, room for the row.
void HudTip_After(void* root, const void* equipment);
// The game reloaded its GUI (the loader's GUI_LOADED event): our items are found again on the next tooltip.
void HudTip_OnGuiLoaded();
