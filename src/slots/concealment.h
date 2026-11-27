// The Customization screen's Concealment bar, hidden for operators whose class can't have concealment.
#pragma once

// The customized trooper was (re)selected (Customize_OnTrooperSelected ran): show or hide the Concealment section.
void Concealment_Refresh();
// Settings switch: off shows the section for everyone again.
void Concealment_SetEnabled(bool on);
// The game reloaded its GUI (the loader's GUI_LOADED event): the section is looked up again on the next refresh.
void Concealment_OnGuiLoaded();
