// The campaign selection screen: a badge on each card of a campaign some squad has finished, and the list of those
// squads while the card is hovered.
#pragma once

// GUI::Item::Show is about to show `item` (the Item::Show pre hook): refreshes the cards when it's the screen.
void Campaigns_OnShow(const void* item);
void Campaigns_OnGuiLoaded();
// the setting changed
void Campaigns_SetEnabled(bool on);
