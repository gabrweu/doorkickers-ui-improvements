// Game functions, globals, struct layouts and enum values used by the plugin, all resolved by name from the PDB
// through the loader. Declared once here as dk2ml.hpp bindings (defined in game.cpp); Resolve() resolves them with
// dk2ml::ResolveAll, which logs every name a game build doesn't have, then sanity-checks the layouts.
// Members of embedded structs are read at summed offsets: dk2ml::At<T>(obj, A.Offset() + B.Offset()).
#pragma once

#include <cstdint>

#include "dk2ml.hpp"

namespace game {

extern const DK2ML_API* api;

// --- hooked functions (safe hooks in plugin.cpp; the signatures say where the arguments are in DK2ML_Regs) ---
// void GUI::ItemList::Init(): lays out a list's children. The stock equipment list builder (on
// GUI_GAME_CUSTOMIZE_INVENTORY_SELECT) calls it right after cloning and filling every item.
extern dk2ml::Fn<void(void* itemList)> GUI_ItemList_Init;
// void GameGUI::Customize_OnTrooperSelected(const Roster::Trooper*): fills the Customization screen for a trooper.
// Called when the screen opens and when another trooper is picked; does nothing while the screen is hidden.
extern dk2ml::Fn<void(void* gameGUI, const void* trooper)> GameGUI_Customize_OnTrooperSelected;
// bool GameGUI::Customize_OnEvent(unsigned eventId, const GUI::sEventParams*)
extern dk2ml::Fn<bool(void* gameGUI, unsigned eventId, const void* params)> GameGUI_Customize_OnEvent;
// void GameGUI::Customize_OnChangePortrait(bool next): the stock PREV/NEXT arrows on the large portrait
extern dk2ml::Fn<void(void* gameGUI, bool next)> GameGUI_Customize_OnChangePortrait;
// void UpdateHumanTooltip(GUI::Item* root, bool, bool, bool, eHumanType, const sHumanId&, const StaticString<48>&,
//     const StaticString<48>&, const Equipment_Client* equipment, Inventory::eType, ...): fills the trooper hover
//     tooltip (HUD_Human_Tooltip) in missions and on the deploy screen. equipment is the 9th argument
//     (dk2ml::Arg(regs, 8) in a pre hook): Equipment_Client[Inventory::NUM_VALUES], indexed by slot.
extern dk2ml::Fn<void(void* root)> UpdateHumanTooltip;
// GUI::Item::Show() / Hide(): every XML Show/Hide of an inventory slot Checkbox goes through them (hooked too)

// The mission action wheel (hud/wheel.cpp); all optional, see WheelUsable.
// void GameGUI::OpenWaypointMenu(const ActionWP_Client* wp, eOpenWaypointsType): (re)builds #waypoint_menu for a
// trooper's waypoint (or ActionWP_Client::s_potential), on the target page (DEFAULT with a target, TARGETED) or the
// path page (PATH, DEFAULT without one). Hooked, and called again by us for the other page.
extern dk2ml::Fn<void(void* gameGUI, const void* wp, int type)> GameGUI_OpenWaypointMenu;
// bool GameGUI::WaypointMenu_OnEvent(int eventId, const GUI::sEventParams*): the wheel's GUI_GAME_WAYPOINT_* events
extern dk2ml::Fn<bool(void* gameGUI, int eventId, const void* params)> GameGUI_WaypointMenu_OnEvent;
extern dk2ml::Field<void*> GameGUI_m_pWaypointMenu;          // #waypoint_menu
// GUI::Item::m_userData, a GUI::sUserData {void* ptr; unsigned id}. On #waypoint_menu: ptr = the game's static
// unsigned holding the target entity id the open stored (0 on the path page), id = the waypoint's id.
extern dk2ml::Field<uint8_t> GUI_Item_m_userData;
extern dk2ml::Field<void*> GUI_sUserData_ptr;
extern dk2ml::Field<void*> GUI_sEventParams_owner;           // the item whose action raised the event
extern dk2ml::Field<int> GUIManager_m_width;                 // the GUI's size (what the wheel's placement uses)
extern dk2ml::Field<int> GUIManager_m_height;
extern dk2ml::Enum OPEN_WAYPOINTS_TARGETED;                  // GameGUI::eOpenWaypointsType
extern dk2ml::Enum OPEN_WAYPOINTS_PATH;
extern dk2ml::Enum GUI_GAME_WAYPOINT_CLICKED;                // an action button: OnChangeAction(type, slot, target)
// The action wheel's names resolved and their layout looks right (else the one-page wheel is off; logged in Resolve).
bool WheelUsable();
// A wheel button's click actions, to tell which action and item it's for (the game's button helper writes the action
// type and inventory slot into the eventParams of each action whose eventId is GUI_GAME_WAYPOINT_CLICKED). Optional,
// see WheelButtonSlotsUsable.
extern dk2ml::Field<uint8_t> GUI_Item_m_eventProperties;     // GUI::Item::sItemEventProperties[], by eItemEventType
extern dk2ml::TypeSize sizeof_ItemEventProperties;
extern dk2ml::Field<uint8_t> ItemEventProperties_actions;    // List<GUI::sAction *>
extern dk2ml::Field<void*> List_ActionPtr_m_list;
extern dk2ml::Field<int> List_ActionPtr_m_elements;
extern dk2ml::Field<int> GUI_sAction_eventId;
extern dk2ml::Field<uint8_t> GUI_sAction_eventParams;        // GUI::sEventParams
extern dk2ml::Field<int> GUI_sEventParams_iParam1;           // for a wheel button: the action type
extern dk2ml::Field<int> GUI_sEventParams_iParam2;           // for a wheel button: the inventory slot (0 = no item)
// Those names resolved and their layout looks right (else the merged wheel can't pair an item's two buttons or drop
// repeated actions).
bool WheelButtonSlotsUsable();

// --- called functions ---
extern dk2ml::Fn<void(void* item)> GUI_Item_Show;
extern dk2ml::Fn<void(void* item)> GUI_Item_Hide;
extern dk2ml::Fn<void(void* item, const char* name)> GUI_Item_SetName;
extern dk2ml::Fn<void*(const void* item)> GUI_Button_CloneInternal; // deep copy, no parent
extern dk2ml::Fn<void(void* item, void* localRoot, void* globalRoot)> GUI_Button_UpdatePointerReferences;
extern dk2ml::Fn<void*(const void* item)> GUI_Item_CloneInternal; // Item::CloneInternal (header template)
extern dk2ml::Fn<void(void* item, void* localRoot, void* globalRoot)> GUI_Item_UpdatePointerReferences;
extern dk2ml::Fn<void(void* staticImage, const char* texture)> GUI_StaticImage_SetImageAndImplicitSize; // also sizes it
extern dk2ml::Fn<bool(void* staticText, const char* text)> GUI_StaticText_ChangeText; // copies (up to the item's limit)
// const Equipment_Template* ObjectLibrary::GetEquipment(const char* name) const
extern dk2ml::Fn<const void*(const void* objectLibrary, const char* name)> ObjectLibrary_GetEquipment;
// bool ObjectLibrary::IsEquipmentBoundTo(const HashedString& eqp, const StaticString<48>& to) const: a <Bind> ties
// the equipment (or its type, see slots.cpp) to a unit or class
extern dk2ml::Fn<bool(const void* objectLibrary, const void* eqp, const void* to)> ObjectLibrary_IsEquipmentBoundTo;
extern dk2ml::Fn<const char*(int value)> Inventory_GetNameForEnum;     // eg. "PrimaryWeapon", "Custom3": a slot's name
extern dk2ml::Fn<const char*(int value)> EquipmentType_GetNameForEnum; // eg. "Grenade"
extern dk2ml::Fn<void(void* gameGUI)> GameGUI_Deploy_Update;
extern dk2ml::Fn<void()> Roster_Save;
extern dk2ml::Fn<const void*(const void* trooper)> Roster_GetSquadForTrooper;
// const ArmyOrg::Unit* ObjectLibrary::GetUnit(const StaticString<48>&) const: compares only the key's m_hash
extern dk2ml::Fn<const void*(const void* objectLibrary, const void* key)> ObjectLibrary_GetUnit;
// const char* Loca::GetTextForId(const char* id, bool, bool): the localized text for an "@id", else null
extern dk2ml::Fn<const char*(const char* id, bool a, bool b)> Loca_GetTextForId;
// bool FileManagerImpl::FileExists(const char* path) const: searches the data folders, packed archives and mods
extern dk2ml::Fn<bool(const void* fileManager, const char* path)> FileManagerImpl_FileExists;
extern dk2ml::Fn<void*(const void* item)> GUI_StaticImage_CloneInternal; // deep copy, no parent (optional)
// const CampaignDef* CampaignManager::GetDef(const char* fileName) (optional)
extern dk2ml::Fn<const void*(const char* fileName)> CampaignManager_GetDef;

// --- globals: the variables (Address(); dereference for the object) ---
extern dk2ml::Global<void*> g_pGUIManager;
extern dk2ml::Global<void*> g_pGameGUI;
extern dk2ml::Global<void*> g_pObjectLibrary;
extern dk2ml::Global<uint8_t> GUI_Button_vftable;      // to type-check items before class-specific calls
extern dk2ml::Global<uint8_t> GUI_StaticImage_vftable;
extern dk2ml::Global<uint8_t> GUI_StaticText_vftable;
extern dk2ml::Global<uint8_t> GUI_Checkbox_vftable;    // the inventory slots
extern dk2ml::Global<void*> g_fileManager;              // a FileManagerImpl
extern dk2ml::Global<uint8_t> Roster_m_squads;          // List<Roster::Squad*> (the list itself)

// --- fields ---
extern dk2ml::Field<void*> GUIManager_m_pRoot;
extern dk2ml::Field<void*> GameGUI_m_pCustomizationScreen;
extern dk2ml::Field<void*> GameGUI_m_pCustomizeSelection;    // the trooper being customized
extern dk2ml::Field<uint8_t> GameGUI_m_deploySelection;      // sDeploySlot
extern dk2ml::Field<uint8_t> GameGUI_sDeploySlot_roster;     // unnamed struct
extern dk2ml::Field<void*> GameGUI_sDeploySlot_roster_pTrooper; // optional: only used to refresh the deploy slot

extern dk2ml::Field<uint8_t> ObjectLibrary_m_equipmentTemplates; // std::unordered_map<unsigned, Equipment_Template*>
extern dk2ml::Field<uint8_t> Roster_Trooper_unit;            // StaticString<48>
extern dk2ml::Field<uint8_t> Roster_Trooper_className;       // StaticString<48>
extern dk2ml::Field<uint8_t> Roster_Trooper_inventory;       // StaticString<48>[Inventory::NUM_VALUES]: equipped names
extern dk2ml::TypeSize sizeof_StaticString48;
extern dk2ml::Field<uint32_t> StaticString48_m_hash;         // 0 = empty
extern dk2ml::Field<char> StaticString48_m_szString;         // char[]
extern dk2ml::Field<uint32_t> HashedString_m_hash;

// portraits (portraits.cpp, picker.cpp)
extern dk2ml::Field<uint8_t> Roster_Trooper_id;              // sHumanId
extern dk2ml::Field<char> sHumanId_tex;                      // char[]: the portrait texture path, saved to roster.xml
extern dk2ml::Field<uint8_t> sHumanId_texHash;               // only used to bound tex
extern dk2ml::Field<int> sHumanId_gender;
extern dk2ml::Field<uint8_t> sHumanId_name;                  // StaticString<48>
extern dk2ml::TypeSize sizeof_Roster_Trooper;                // stride of Squad::roster
extern dk2ml::Field<uint8_t> ArmyOrg_Unit_name;              // StaticString<48>, the internal name, eg. "Rangers"
extern dk2ml::Field<const char*> ArmyOrg_Unit_nameUI;        // localization id, eg. "@unit_rangers_name"
int32_t UnitNameUIOffset();                                  // ArmyOrg_Unit_nameUI, or -1 if unusable (see game.cpp)
extern dk2ml::Field<int> Roster_Squad_numInRoster;
extern dk2ml::Field<uint8_t> Roster_Squad_roster;            // inline Roster::Trooper[]
extern dk2ml::Field<uint8_t> Roster_Squad_diedInCampaign;    // the field after the array (bounds the count)
extern dk2ml::Field<uint8_t> Roster_Squad_name;              // StaticString<32>
extern dk2ml::Field<char> StaticString32_m_szString;
extern dk2ml::Field<void*> List_SquadPtr_m_list;             // List<Roster::Squad *>
extern dk2ml::Field<int> List_SquadPtr_m_elements;
extern dk2ml::Field<uint8_t> ObjectLibrary_m_humanPortraits; // List<sHumanPortrait>
extern dk2ml::Field<void*> List_HumanPortrait_m_list;
extern dk2ml::Field<int> List_HumanPortrait_m_elements;
extern dk2ml::TypeSize sizeof_sHumanPortrait;
extern dk2ml::Field<char> sHumanPortrait_tex;                // char[128]
extern dk2ml::Field<uint32_t> sHumanPortrait_unit;           // hash of the unit name, as StaticString<48>::m_hash
extern dk2ml::Field<uint32_t> sHumanPortrait_className;      // hash, 0 = any class
extern dk2ml::Field<int> sHumanPortrait_gender;

extern dk2ml::Field<int> GUI_Item_m_size;                    // int[2]
extern dk2ml::Field<float> GUI_Item_m_localOrigin;           // Vector2
extern dk2ml::Field<int> GUI_Item_m_alignFlags;
extern dk2ml::Field<float> GUI_Item_m_alignOffset;           // Vector2: the XML origin of an aligned item
extern dk2ml::Field<bool> GUI_Item_m_bStealFocus;
extern dk2ml::Field<const char*> HashedString_m_pString;
extern dk2ml::Field<char*> GUI_StaticText_m_text;
extern dk2ml::Field<uint8_t> GUI_StaticText_m_textRenderObject; // GUI::sTextRenderObject
extern dk2ml::Field<uint32_t> GUI_sTextRenderObject_color;   // 0xAARRGGBB
extern dk2ml::TypeSize sizeof_Equipment_Client;              // stride of the tooltip's equipment array
extern dk2ml::Field<const void*> Equipment_Client_pItem;     // const Equipment_Template*
extern dk2ml::Field<uint8_t> GUI_StaticImage_m_renderObject; // an inline RenderObject2D
extern dk2ml::Field<float> RenderObject2D_scale;             // Vector2: drawn size = size * scale
extern dk2ml::Field<float> RenderObject2D_size;              // Vector2: the texture's size in pixels
extern dk2ml::Field<bool> RenderObject2D_bNeedsUpdate;       // rebuild the quad from size/scale
extern dk2ml::Field<unsigned> RenderObject2D_texture;        // texture id (optional: the action wheel's stock ring)
extern dk2ml::TypeSize sizeof_RenderObject2D;
extern dk2ml::Field<void*> GUI_Checkbox_m_pButtons;          // GUI::Button*[2]: the unchecked / checked state
extern dk2ml::Field<uint8_t> GUI_Button_m_renderObjects;     // RenderObject2D[3]: normal / hover / click


extern dk2ml::Field<int> Equipment_Template_type;            // EquipmentType::eType
extern dk2ml::Field<uint8_t> Equipment_Template_name;        // HashedString: the template name binds refer to
extern dk2ml::Field<const char*> Equipment_Template_tooltip; // the game skips items without one
extern dk2ml::Field<const char*> Equipment_Template_guiImage; // the list icon, eg. data/models/weapons/m4_ui.dds
extern dk2ml::Field<uint8_t> Equipment_Template_category;    // HashedString, eg. "rifle"; firearms only
extern dk2ml::Field<const char*> Equipment_Template_inventoryBinding; // the slot, eg. "PrimaryWeapon"
// EqpFirearm_Template::suppressedSwitch, char*: the other half of a weapon's suppressed / unsuppressed pair (the
// muzzle slot equips the "_Sup" firearm). Optional.
extern dk2ml::Field<const char*> EqpFirearm_Template_suppressedSwitch;
int32_t SuppressedSwitchOffset();                            // it, or -1 if missing or unusable (see game.cpp)
// EqpFirearm_Template::szCaliberInfo, char*: the <Params caliberInfoText> localization key, eg.
// "@firearm_caliber_9x19_name". Splits pistol-calibre rifles into SMGS. Optional.
extern dk2ml::Field<const char*> EqpFirearm_Template_szCaliberInfo;
int32_t CaliberInfoOffset();                                 // it, or -1 if missing or unusable (see game.cpp)
// An Equipment_Template is a ModifiableParamsList (base at +0): its <ModifiableParams> attributes as a List of
// {StaticString<32> name, float val}, eg. roundsPerMagazine, accuracyStart. All optional, see ParamsUsable.
extern dk2ml::Field<uint8_t> ModifiableParamsList_params;    // List<ModifiableParamsList::sValue>
extern dk2ml::Field<void*> List_sValue_m_list;               // sValue[]
extern dk2ml::Field<int> List_sValue_m_elements;
extern dk2ml::TypeSize sizeof_sValue;
extern dk2ml::Field<uint8_t> sValue_name;                    // StaticString<32>
extern dk2ml::Field<float> sValue_val;
bool ParamsUsable();                                         // names resolved and the layout looks right (see Resolve)
// The template's <ModifiableParams> value called `name`, or `fallback` if it has none or ParamsUsable() is false.
float ModifiableParam(const void* tmpl, const char* name, float fallback);
// FNV-1a: the game's HashedString hash, and StaticString<48>::m_hash of a non-empty string (0 for "")
unsigned Hash(const char* s);
// The trooper may use the template (0x140112B40 in build 112): bound by name or by type, to its class or its unit.
bool BoundToTrooper(const void* library, const void* tmpl, const void* trooper);
// After changing the customized trooper (portrait, inventory): Roster::Save, then the stock refreshes, as the
// PREV/NEXT portrait arrows and Paste loadout do: Customize_OnTrooperSelected (the profile panel and #inventory_panel)
// and Deploy_Update (the deploy screen's slots, drawn from the selected slot's trooper).
void SaveAndRefresh(void* trooper);

// campaign cards (campaigns/cards.cpp); all optional, see CampaignCardsUsable
extern dk2ml::Global<uint8_t> g_campaignHistory;             // List<CampaignHistory> (the list itself): finished runs
extern dk2ml::Field<int> List_CampaignHistory_m_size;        // capacity
extern dk2ml::Field<void*> List_CampaignHistory_m_list;      // CampaignHistory[]
extern dk2ml::Field<int> List_CampaignHistory_m_elements;
extern dk2ml::TypeSize sizeof_CampaignHistory;
extern dk2ml::Field<char> CampaignHistory_fileName;          // char[128], eg. "data/campaigns/operation_kerak.xml"
extern dk2ml::Field<char> CampaignHistory_squadName;         // char[32], Roster::Squad::name when the run ended
extern dk2ml::Field<char> CampaignDef_medalImage;            // char[128], may be empty
// The campaign cards' names resolved and their layout looks right (else the feature is off; logged in Resolve).
bool CampaignCardsUsable();

// GUI::Editbox (XML <Editbox>, eg. the trooper name), for renaming loadouts (loadouts/panel.cpp); all optional, see
// EditboxUsable. SetState(FOCUSED) is how a box gets the keyboard (it captures input: keys and chars then go only to
// it); Enter, Esc or a click elsewhere set it back to UNFOCUSED.
extern dk2ml::Fn<void(void* box, int state)> GUI_Editbox_SetState;
extern dk2ml::Fn<void(void* box, const char* format, const char* arg)> GUI_Editbox_SetText_varg; // printf-style
extern dk2ml::Field<int> GUI_Editbox_m_state;                // eEditboxState
extern dk2ml::Field<char*> GUI_Editbox_m_text;               // the typed text (UTF-8)
extern dk2ml::Enum Editbox_STATE_FOCUSED;
extern dk2ml::Enum Editbox_STATE_UNFOCUSED;
// The Editbox names resolved and their layout looks right (else loadouts can't be renamed; logged in Resolve).
bool EditboxUsable();

// A class's human template and its base concealment (slots/concealment.cpp); optional, see ConcealmentUsable.
// const Human_Template* Human_GetTemplateForUnitClassGender(const StaticString<48>& unit, const StaticString<48>& cls,
//     int gender): what the Customization screen's stats refresh (0x1401157C0 in build 112) reads concealment from.
extern dk2ml::Fn<const void*(const void* unit, const void* cls, int gender)> Human_GetTemplateForUnitClassGender;
// <Concealment default="..."/> in data/entities/humans_goodguys*.xml; 0 (the constructor's default) for a class
// without one: every class but the CIA's
extern dk2ml::Field<float> Human_Template_concealment;
bool ConcealmentUsable();
// A class's starting loadout (loadouts/store.cpp); optional, see StartingLoadoutUsable. Human_Template::inventory is a
// List<StaticString<48>>: the <Equipment><Item name=.../> names, no slots. Inventory::MapRandomInventoryToSlots(list,
// StaticString<48> out[Inventory::NUM_VALUES]) puts each in its slot by its inventoryBinding (a name with no template
// goes to slot 0, not a real slot); Human_Client::Init (0x1400AF9E9 in build 112) calls it on a zeroed array.
extern dk2ml::Field<uint8_t> Human_Template_inventory;
extern dk2ml::Fn<void(const void* list, void* out)> Inventory_MapRandomInventoryToSlots;
bool StartingLoadoutUsable();

// --- enum values ---
extern dk2ml::Enum EVENT_SCROLL_DOWN;         // GUI::Item::eItemEventType: mouse wheel over the item
extern dk2ml::Enum EVENT_SCROLL_UP;
extern dk2ml::Enum EVENT_CURSOR_DOWN;
extern dk2ml::Enum EVENT_DRAG;                // cursor moved while pressed on the item
extern dk2ml::Enum EVENT_CURSOR_HOVER;        // XML OnHover
extern dk2ml::Enum EVENT_CURSOR_HOVER_END;    // XML OnHoverEnd
extern dk2ml::Enum EVENT_CLICK;               // XML OnClick
extern dk2ml::Enum GUI_GAME_CUSTOMIZE_BACK;   // GUI::Events::eEventType: Esc, background click, BACK
extern dk2ml::Enum GUI_GAME_CUSTOMIZE_OPENED;
extern dk2ml::Enum GUI_GAME_CUSTOMIZE_CLOSED;
extern dk2ml::Enum EquipmentType_FIREARM;
extern dk2ml::Enum EquipmentType_EMPTY;       // the "nothing" entry of a slot
extern dk2ml::Enum Inventory_NUM_VALUES;
extern dk2ml::Enum Inventory_UTILITY_POUCH1;  // UTILITY_POUCH1 .. SUPPORT_GEAR3: bindings matched as substrings
extern dk2ml::Enum Inventory_SUPPORT_GEAR3;   //   ("UtilityPouch" items go in UtilityPouch1 and 2)
// weapon scope/ammo/muzzle slots and HELMET_NVG: the game shows/hides those itself
constexpr int kGameManagedSlots = 7;
extern dk2ml::Enum Inventory_GameManaged[kGameManagedSlots];

// Resolves everything above (dk2ml::ResolveAll) and sanity-checks the layouts. Returns false (after logging what's
// missing or wrong) if the plugin can't work.
bool Resolve();

inline void* GameGUI() { return *g_pGameGUI; }
inline void* ObjectLibrary() { return *g_pObjectLibrary; }
inline void* GuiRoot()
{
    void* manager = *g_pGUIManager;
    return manager ? GUIManager_m_pRoot(manager) : nullptr;
}

} // namespace game
