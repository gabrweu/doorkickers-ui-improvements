#include "core/game.h"

#include <cstring>

namespace game {

const DK2ML_API* api = nullptr;

// hooked functions
dk2ml::Fn<void(void*)> GUI_ItemList_Init{"?Init@ItemList@GUI@@QEAAXXZ"};
dk2ml::Fn<void(void*, const void*)> GameGUI_Customize_OnTrooperSelected{"GameGUI::Customize_OnTrooperSelected"};
dk2ml::Fn<bool(void*, unsigned, const void*)> GameGUI_Customize_OnEvent{"GameGUI::Customize_OnEvent"};
dk2ml::Fn<void(void*, bool)> GameGUI_Customize_OnChangePortrait{"GameGUI::Customize_OnChangePortrait"};
dk2ml::Fn<void(void*)> UpdateHumanTooltip{
    "?UpdateHumanTooltip@@YAXPEAVItem@GUI@@_N11W4eHumanType@@AEBUsHumanId@@AEBV?$StaticString@$0DA@@@4QEBUEquipment_Client@@W4eType@Inventory@@PEBV?$List@I@@HHHHIPEBD@Z"};

// called functions
dk2ml::Fn<void(void*)> GUI_Item_Show{"GUI::Item::Show"};
dk2ml::Fn<void(void*)> GUI_Item_Hide{"GUI::Item::Hide"};
dk2ml::Fn<void(void*, const char*)> GUI_Item_SetName{"GUI::Item::SetName"};
dk2ml::Fn<void*(const void*)> GUI_Button_CloneInternal{"GUI::Button::CloneInternal"};
dk2ml::Fn<void(void*, void*, void*)> GUI_Button_UpdatePointerReferences{"GUI::Button::UpdatePointerReferences"};
dk2ml::Fn<void*(const void*)> GUI_Item_CloneInternal{"?CloneInternal@Item@GUI@@UEBAPEAV12@XZ"};
dk2ml::Fn<void(void*, void*, void*)> GUI_Item_UpdatePointerReferences{"?UpdatePointerReferences@Item@GUI@@UEAAXPEAV12@0@Z"};
dk2ml::Fn<void(void*, const char*)> GUI_StaticImage_SetImageAndImplicitSize{"GUI::StaticImage::SetImageAndImplicitSize"};
dk2ml::Fn<bool(void*, const char*)> GUI_StaticText_ChangeText{"?ChangeText@StaticText@GUI@@QEAA_NPEBD@Z"};
// overloaded (also by HashedString); this one takes the template name
dk2ml::Fn<const void*(const void*, const char*)> ObjectLibrary_GetEquipment{"?GetEquipment@ObjectLibrary@@QEBAPEBVEquipment_Template@@PEBD@Z"};
dk2ml::Fn<bool(const void*, const void*, const void*)> ObjectLibrary_IsEquipmentBoundTo{
    "?IsEquipmentBoundTo@ObjectLibrary@@QEBA_NAEBVHashedString@@AEBV?$StaticString@$0DA@@@@Z"};
dk2ml::Fn<const char*(int)> Inventory_GetNameForEnum{"?GetNameForEnum@Inventory@@YAPEBDW4eType@1@@Z"};
dk2ml::Fn<const char*(int)> EquipmentType_GetNameForEnum{"?GetNameForEnum@EquipmentType@@YAPEBDW4eType@1@@Z"};
dk2ml::Fn<void(void*)> GameGUI_Deploy_Update{"GameGUI::Deploy_Update"};
dk2ml::Fn<void()> Roster_Save{"Roster::Save"};
dk2ml::Fn<const void*(const void*)> Roster_GetSquadForTrooper{"?GetSquadForTrooper@Roster@@YAPEBUSquad@1@PEBUTrooper@1@@Z"};
dk2ml::Fn<const void*(const void*, const void*)> ObjectLibrary_GetUnit{
    "?GetUnit@ObjectLibrary@@QEBAPEBUUnit@ArmyOrg@@AEBV?$StaticString@$0DA@@@@Z"};
dk2ml::Fn<const char*(const char*, bool, bool)> Loca_GetTextForId{"?GetTextForId@Loca@@YAPEBDPEBD_N1@Z"};
dk2ml::Fn<bool(const void*, const char*)> FileManagerImpl_FileExists{"?FileExists@FileManagerImpl@@UEBA_NPEBD@Z"};
dk2ml::Fn<void*(const void*)> GUI_StaticImage_CloneInternal{"GUI::StaticImage::CloneInternal", dk2ml::Optional};
// overloaded (also by index); this one takes the campaign's file name
dk2ml::Fn<const void*(const char*)> CampaignManager_GetDef{"?GetDef@CampaignManager@@YAPEBUCampaignDef@@PEBD@Z", dk2ml::Optional};
dk2ml::Fn<void(void*, int)> GUI_Editbox_SetState{"?SetState@Editbox@GUI@@QEAAXW4eEditboxState@12@@Z", dk2ml::Optional};
dk2ml::Fn<void(void*, const char*, const char*)> GUI_Editbox_SetText_varg{"?SetText_varg@Editbox@GUI@@QEAAXPEBDZZ", dk2ml::Optional};
dk2ml::Field<int> GUI_Editbox_m_state{"GUI::Editbox", "m_state", dk2ml::Optional};
dk2ml::Field<char*> GUI_Editbox_m_text{"GUI::Editbox", "m_text", dk2ml::Optional};
dk2ml::Enum Editbox_STATE_FOCUSED{"GUI::Editbox::eEditboxState", "STATE_FOCUSED", dk2ml::Optional};
dk2ml::Enum Editbox_STATE_UNFOCUSED{"GUI::Editbox::eEditboxState", "STATE_UNFOCUSED", dk2ml::Optional};
dk2ml::Fn<const void*(const void*, const void*, int)> Human_GetTemplateForUnitClassGender{
    "?Human_GetTemplateForUnitClassGender@@YAPEBVHuman_Template@@AEBV?$StaticString@$0DA@@@0H@Z", dk2ml::Optional};
dk2ml::Field<float> Human_Template_concealment{"Human_Template", "concealment", dk2ml::Optional};
dk2ml::Field<uint8_t> Human_Template_inventory{"Human_Template", "inventory", dk2ml::Optional};
dk2ml::Fn<void(const void*, void*)> Inventory_MapRandomInventoryToSlots{
    "?MapRandomInventoryToSlots@Inventory@@YAXAEBV?$List@V?$StaticString@$0DA@@@@@QEAV?$StaticString@$0DA@@@@Z", dk2ml::Optional};

// the action wheel: optional, the rest of the plugin works without it
dk2ml::Fn<void(void*, const void*, int)> GameGUI_OpenWaypointMenu{
    "?OpenWaypointMenu@GameGUI@@QEAAXPEBVActionWP_Client@@W4eOpenWaypointsType@1@@Z", dk2ml::Optional};
dk2ml::Fn<bool(void*, int, const void*)> GameGUI_WaypointMenu_OnEvent{
    "?WaypointMenu_OnEvent@GameGUI@@AEAA_NHPEBUsEventParams@GUI@@@Z", dk2ml::Optional};
dk2ml::Field<void*> GameGUI_m_pWaypointMenu{"GameGUI", "m_pWaypointMenu", dk2ml::Optional};
dk2ml::Field<uint8_t> GUI_Item_m_userData{"GUI::Item", "m_userData", dk2ml::Optional};
dk2ml::Field<void*> GUI_sUserData_ptr{"GUI::sUserData", "ptr", dk2ml::Optional};
dk2ml::Field<void*> GUI_sEventParams_owner{"GUI::sEventParams", "owner", dk2ml::Optional};
dk2ml::Field<int> GUIManager_m_width{"GUIManager", "m_width", dk2ml::Optional};
dk2ml::Field<int> GUIManager_m_height{"GUIManager", "m_height", dk2ml::Optional};
dk2ml::Enum OPEN_WAYPOINTS_TARGETED{"GameGUI::eOpenWaypointsType", "OPEN_WAYPOINTS_TARGETED", dk2ml::Optional};
dk2ml::Enum OPEN_WAYPOINTS_PATH{"GameGUI::eOpenWaypointsType", "OPEN_WAYPOINTS_PATH", dk2ml::Optional};
dk2ml::Enum GUI_GAME_WAYPOINT_CLICKED{"GUI::Events::eEventType", "GUI_GAME_WAYPOINT_CLICKED", dk2ml::Optional};
dk2ml::Field<uint8_t> GUI_Item_m_eventProperties{"GUI::Item", "m_eventProperties", dk2ml::Optional};
dk2ml::TypeSize sizeof_ItemEventProperties{"GUI::Item::sItemEventProperties", dk2ml::Optional};
dk2ml::Field<uint8_t> ItemEventProperties_actions{"GUI::Item::sItemEventProperties", "actions", dk2ml::Optional};
dk2ml::Field<void*> List_ActionPtr_m_list{"List<GUI::sAction *>", "m_list", dk2ml::Optional};
dk2ml::Field<int> List_ActionPtr_m_elements{"List<GUI::sAction *>", "m_elements", dk2ml::Optional};
dk2ml::Field<int> GUI_sAction_eventId{"GUI::sAction", "eventId", dk2ml::Optional};
dk2ml::Field<uint8_t> GUI_sAction_eventParams{"GUI::sAction", "eventParams", dk2ml::Optional};
dk2ml::Field<int> GUI_sEventParams_iParam1{"GUI::sEventParams", "iParam1", dk2ml::Optional};
dk2ml::Field<int> GUI_sEventParams_iParam2{"GUI::sEventParams", "iParam2", dk2ml::Optional};

// globals
dk2ml::Global<void*> g_pGUIManager{"g_pGUIManager"};
dk2ml::Global<void*> g_pGameGUI{"g_pGameGUI"};
dk2ml::Global<void*> g_pObjectLibrary{"?g_pObjectLibrary@@3PEAVObjectLibrary@@EA"};
dk2ml::Global<uint8_t> GUI_Button_vftable{"??_7Button@GUI@@6B@"};
dk2ml::Global<uint8_t> GUI_StaticImage_vftable{"??_7StaticImage@GUI@@6B@"};
dk2ml::Global<uint8_t> GUI_StaticText_vftable{"??_7StaticText@GUI@@6B@"};
dk2ml::Global<uint8_t> GUI_Checkbox_vftable{"??_7Checkbox@GUI@@6B@"};
dk2ml::Global<void*> g_fileManager{"?g_fileManager@@3PEAVFileManager@@EA"};
dk2ml::Global<uint8_t> Roster_m_squads{"?m_squads@Roster@@3V?$List@PEAUSquad@Roster@@@@A"};

// fields
dk2ml::Field<void*> GUIManager_m_pRoot{"GUIManager", "m_pRoot"};
dk2ml::Field<void*> GameGUI_m_pCustomizationScreen{"GameGUI", "m_pCustomizationScreen"};
dk2ml::Field<void*> GameGUI_m_pCustomizeSelection{"GameGUI", "m_pCustomizeSelection"};
dk2ml::Field<uint8_t> GameGUI_m_deploySelection{"GameGUI", "m_deploySelection"};
dk2ml::Field<uint8_t> GameGUI_sDeploySlot_roster{"GameGUI::sDeploySlot", "roster"};
// optional: without it only the deploy slot's small portrait waits for the screen's next refresh
dk2ml::Field<void*> GameGUI_sDeploySlot_roster_pTrooper{"GameGUI::sDeploySlot::<unnamed-tag>::<unnamed-type-roster>", "pTrooper",
                                                       dk2ml::Optional};
dk2ml::Field<uint8_t> ObjectLibrary_m_equipmentTemplates{"ObjectLibrary", "m_equipmentTemplates"};
dk2ml::Field<uint8_t> Roster_Trooper_unit{"Roster::Trooper", "unit"};
dk2ml::Field<uint8_t> Roster_Trooper_className{"Roster::Trooper", "className"};
dk2ml::Field<uint8_t> Roster_Trooper_inventory{"Roster::Trooper", "inventory"};
dk2ml::TypeSize sizeof_StaticString48{"StaticString<48>"};
dk2ml::Field<uint32_t> StaticString48_m_hash{"StaticString<48>", "m_hash"};
dk2ml::Field<char> StaticString48_m_szString{"StaticString<48>", "m_szString"};
dk2ml::Field<uint32_t> HashedString_m_hash{"HashedString", "m_hash"};

dk2ml::Field<uint8_t> Roster_Trooper_id{"Roster::Trooper", "id"};
dk2ml::Field<char> sHumanId_tex{"sHumanId", "tex"};
dk2ml::Field<uint8_t> sHumanId_texHash{"sHumanId", "texHash"};
dk2ml::Field<int> sHumanId_gender{"sHumanId", "gender"};
dk2ml::Field<uint8_t> sHumanId_name{"sHumanId", "name"};
dk2ml::TypeSize sizeof_Roster_Trooper{"Roster::Trooper"};
dk2ml::Field<uint8_t> ArmyOrg_Unit_name{"ArmyOrg::Unit", "name"};
dk2ml::Field<const char*> ArmyOrg_Unit_nameUI{"ArmyOrg::Unit", "nameUI"};
dk2ml::Field<int> Roster_Squad_numInRoster{"Roster::Squad", "numInRoster"};
dk2ml::Field<uint8_t> Roster_Squad_roster{"Roster::Squad", "roster"};
dk2ml::Field<uint8_t> Roster_Squad_diedInCampaign{"Roster::Squad", "diedInCampaign"};
dk2ml::Field<uint8_t> Roster_Squad_name{"Roster::Squad", "name"};
dk2ml::Field<char> StaticString32_m_szString{"StaticString<32>", "m_szString"};
dk2ml::Field<void*> List_SquadPtr_m_list{"List<Roster::Squad *>", "m_list"};
dk2ml::Field<int> List_SquadPtr_m_elements{"List<Roster::Squad *>", "m_elements"};
dk2ml::Field<uint8_t> ObjectLibrary_m_humanPortraits{"ObjectLibrary", "m_humanPortraits"};
dk2ml::Field<void*> List_HumanPortrait_m_list{"List<sHumanPortrait>", "m_list"};
dk2ml::Field<int> List_HumanPortrait_m_elements{"List<sHumanPortrait>", "m_elements"};
dk2ml::TypeSize sizeof_sHumanPortrait{"sHumanPortrait"};
dk2ml::Field<char> sHumanPortrait_tex{"sHumanPortrait", "tex"};
dk2ml::Field<uint32_t> sHumanPortrait_unit{"sHumanPortrait", "unit"};
dk2ml::Field<uint32_t> sHumanPortrait_className{"sHumanPortrait", "className"};
dk2ml::Field<int> sHumanPortrait_gender{"sHumanPortrait", "gender"};

dk2ml::Field<int> GUI_Item_m_size{"GUI::Item", "m_size"};
dk2ml::Field<float> GUI_Item_m_localOrigin{"GUI::Item", "m_localOrigin"};
dk2ml::Field<int> GUI_Item_m_alignFlags{"GUI::Item", "m_alignFlags"};
dk2ml::Field<float> GUI_Item_m_alignOffset{"GUI::Item", "m_alignOffset"};
dk2ml::Field<bool> GUI_Item_m_bStealFocus{"GUI::Item", "m_bStealFocus"};
dk2ml::Field<const char*> HashedString_m_pString{"HashedString", "m_pString"};
dk2ml::Field<char*> GUI_StaticText_m_text{"GUI::StaticText", "m_text"};
dk2ml::Field<uint8_t> GUI_StaticText_m_textRenderObject{"GUI::StaticText", "m_textRenderObject"};
dk2ml::Field<uint32_t> GUI_sTextRenderObject_color{"GUI::sTextRenderObject", "color"};
dk2ml::TypeSize sizeof_Equipment_Client{"Equipment_Client"};
dk2ml::Field<const void*> Equipment_Client_pItem{"Equipment_Client", "pItem"};
dk2ml::Field<uint8_t> GUI_StaticImage_m_renderObject{"GUI::StaticImage", "m_renderObject"};
dk2ml::Field<float> RenderObject2D_scale{"RenderObject2D", "scale"};
dk2ml::Field<float> RenderObject2D_size{"RenderObject2D", "size"};
dk2ml::Field<bool> RenderObject2D_bNeedsUpdate{"RenderObject2D", "bNeedsUpdate"};
dk2ml::Field<unsigned> RenderObject2D_texture{"RenderObject2D", "texture", dk2ml::Optional};
dk2ml::TypeSize sizeof_RenderObject2D{"RenderObject2D"};
dk2ml::Field<void*> GUI_Checkbox_m_pButtons{"GUI::Checkbox", "m_pButtons"};
dk2ml::Field<uint8_t> GUI_Button_m_renderObjects{"GUI::Button", "m_renderObjects"};


dk2ml::Field<int> Equipment_Template_type{"Equipment_Template", "type"};
dk2ml::Field<uint8_t> Equipment_Template_name{"Equipment_Template", "name"};
dk2ml::Field<const char*> Equipment_Template_tooltip{"Equipment_Template", "tooltip"};
dk2ml::Field<const char*> Equipment_Template_guiImage{"Equipment_Template", "guiImage"};
dk2ml::Field<uint8_t> Equipment_Template_category{"Equipment_Template", "category"};
dk2ml::Field<const char*> Equipment_Template_inventoryBinding{"Equipment_Template", "inventoryBinding"};
// optional: without it, squadmates with a suppressed weapon just don't count for the unsuppressed one
dk2ml::Field<const char*> EqpFirearm_Template_suppressedSwitch{"EqpFirearm_Template", "suppressedSwitch", dk2ml::Optional};
// optional: without it, SMGs stay in RIFLES
dk2ml::Field<const char*> EqpFirearm_Template_szCaliberInfo{"EqpFirearm_Template", "szCaliberInfo", dk2ml::Optional};
// optional: without them, rifles a mod misfiles (belt-fed MGs, marksman rifles) stay in RIFLES
dk2ml::Field<uint8_t> ModifiableParamsList_params{"ModifiableParamsList", "params", dk2ml::Optional};
dk2ml::Field<void*> List_sValue_m_list{"List<ModifiableParamsList::sValue>", "m_list", dk2ml::Optional};
dk2ml::Field<int> List_sValue_m_elements{"List<ModifiableParamsList::sValue>", "m_elements", dk2ml::Optional};
dk2ml::TypeSize sizeof_sValue{"ModifiableParamsList::sValue", dk2ml::Optional};
dk2ml::Field<uint8_t> sValue_name{"ModifiableParamsList::sValue", "name", dk2ml::Optional};
dk2ml::Field<float> sValue_val{"ModifiableParamsList::sValue", "val", dk2ml::Optional};

// campaign cards: optional, the rest of the plugin works without them
// (g_campaignHistory is module-local in the game: not in the PDB's publics, but found by name)
dk2ml::Global<uint8_t> g_campaignHistory{"g_campaignHistory", dk2ml::Optional};
dk2ml::Field<int> List_CampaignHistory_m_size{"List<CampaignHistory>", "m_size", dk2ml::Optional};
dk2ml::Field<void*> List_CampaignHistory_m_list{"List<CampaignHistory>", "m_list", dk2ml::Optional};
dk2ml::Field<int> List_CampaignHistory_m_elements{"List<CampaignHistory>", "m_elements", dk2ml::Optional};
dk2ml::TypeSize sizeof_CampaignHistory{"CampaignHistory", dk2ml::Optional};
dk2ml::Field<char> CampaignHistory_fileName{"CampaignHistory", "fileName", dk2ml::Optional};
dk2ml::Field<char> CampaignHistory_squadName{"CampaignHistory", "squadName", dk2ml::Optional};
dk2ml::Field<char> CampaignDef_medalImage{"CampaignDef", "medalImage", dk2ml::Optional};

// enum values
dk2ml::Enum EVENT_SCROLL_DOWN{"GUI::Item::eItemEventType", "EVENT_SCROLL_DOWN"};
dk2ml::Enum EVENT_SCROLL_UP{"GUI::Item::eItemEventType", "EVENT_SCROLL_UP"};
dk2ml::Enum EVENT_CURSOR_DOWN{"GUI::Item::eItemEventType", "EVENT_CURSOR_DOWN"};
dk2ml::Enum EVENT_DRAG{"GUI::Item::eItemEventType", "EVENT_DRAG"};
dk2ml::Enum EVENT_CURSOR_HOVER{"GUI::Item::eItemEventType", "EVENT_CURSOR_HOVER"};
dk2ml::Enum EVENT_CURSOR_HOVER_END{"GUI::Item::eItemEventType", "EVENT_CURSOR_HOVER_END"};
dk2ml::Enum EVENT_CLICK{"GUI::Item::eItemEventType", "EVENT_CLICK"};
dk2ml::Enum GUI_GAME_CUSTOMIZE_BACK{"GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_BACK"};
dk2ml::Enum GUI_GAME_CUSTOMIZE_OPENED{"GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_OPENED"};
dk2ml::Enum GUI_GAME_CUSTOMIZE_CLOSED{"GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_CLOSED"};
dk2ml::Enum EquipmentType_FIREARM{"EquipmentType::eType", "FIREARM"};
dk2ml::Enum EquipmentType_EMPTY{"EquipmentType::eType", "EMPTY"};
dk2ml::Enum Inventory_NUM_VALUES{"Inventory::eType", "NUM_VALUES"};
dk2ml::Enum Inventory_UTILITY_POUCH1{"Inventory::eType", "UTILITY_POUCH1"};
dk2ml::Enum Inventory_SUPPORT_GEAR3{"Inventory::eType", "SUPPORT_GEAR3"};
dk2ml::Enum Inventory_GameManaged[kGameManagedSlots] = {
    {"Inventory::eType", "PRIMARY_WEAPON_SCOPE"},   {"Inventory::eType", "PRIMARY_WEAPON_AMMO"},
    {"Inventory::eType", "PRIMARY_WEAPON_MUZZLE"},  {"Inventory::eType", "SECONDARY_WEAPON_SCOPE"},
    {"Inventory::eType", "SECONDARY_WEAPON_AMMO"},  {"Inventory::eType", "SECONDARY_WEAPON_MUZZLE"},
    {"Inventory::eType", "HELMET_NVG"}};

namespace {
bool g_unitNameUIUsable = true;
bool g_suppressedSwitchUsable = true;
bool g_caliberInfoUsable = true;
bool g_paramsUsable = false;

// The <ModifiableParams> list's names, and its layout: the PDB also has 32-bit layouts of these types, so a misaligned
// pointer, or a val that overlaps the 32-char name, means the wrong one was picked.
bool CheckParams()
{
    const dk2ml::Binding* needed[] = {&ModifiableParamsList_params, &List_sValue_m_list, &List_sValue_m_elements,
                                      &sizeof_sValue, &sValue_name, &sValue_val};
    for (const dk2ml::Binding* b : needed)
        if (!b->Resolved()) {
            api->Log("misfiled-rifle regrouping off: a name it needs is missing (listed above)");
            return false;
        }
    int32_t params = ModifiableParamsList_params.Offset(), list = List_sValue_m_list.Offset(), val = sValue_val.Offset();
    uint32_t size = sizeof_sValue.Get();
    if (params % 8 != 0 || params >= 32 || list % 8 != 0 || List_sValue_m_elements.Offset() < 0 || sValue_name.Offset() != 0 ||
        val < 32 || static_cast<uint32_t>(val) + 4 > size) {
        api->Log("misfiled-rifle regrouping off: ModifiableParamsList layout looks wrong (params=%d list=%d,%d sValue size=%u val=%d)",
                 params, list, List_sValue_m_elements.Offset(), size, val);
        return false;
    }
    return true;
}
bool g_campaignCardsUsable = false;
bool g_editboxUsable = false;
bool g_wheelUsable = false;
bool g_wheelButtonSlotsUsable = false;

// The wheel buttons' click actions: the PDB also has 32-bit layouts of these types, so misaligned pointer fields, or
// an event-properties entry too small for its action list, mean the wrong one was picked.
bool CheckWheelButtonSlots()
{
    const dk2ml::Binding* needed[] = {&GUI_Item_m_eventProperties, &sizeof_ItemEventProperties, &ItemEventProperties_actions,
                                      &List_ActionPtr_m_list, &List_ActionPtr_m_elements, &GUI_sAction_eventId,
                                      &GUI_sAction_eventParams, &GUI_sEventParams_iParam1, &GUI_sEventParams_iParam2,
                                      &EVENT_CLICK};
    for (const dk2ml::Binding* b : needed)
        if (!b->Resolved()) {
            api->Log("action wheel: an item's two buttons aren't paired, repeated actions stay (a name it needs is missing, listed above)");
            return false;
        }
    int32_t props = GUI_Item_m_eventProperties.Offset(), actions = ItemEventProperties_actions.Offset(),
            list = List_ActionPtr_m_list.Offset(), elements = List_ActionPtr_m_elements.Offset();
    int32_t param1 = GUI_sEventParams_iParam1.Offset(), param2 = GUI_sEventParams_iParam2.Offset();
    uint32_t stride = sizeof_ItemEventProperties.Get();
    if (props % 8 != 0 || stride % 8 != 0 || stride < 24 || actions % 8 != 0 || list % 8 != 0 || elements < 0 ||
        static_cast<uint32_t>(actions + elements) + 4 > stride || GUI_sAction_eventParams.Offset() % 8 != 0 ||
        param1 < 16 || param2 < 16 || param1 == param2) {
        api->Log("action wheel: an item's two buttons aren't paired, repeated actions stay (layout looks wrong: "
                 "eventProperties=%d stride=%u actions=%d list=%d,%d eventParams=%d iParam1=%d iParam2=%d)",
                 props, stride, actions, list, elements, GUI_sAction_eventParams.Offset(), param1, param2);
        return false;
    }
    return true;
}

// The action wheel's names, and their layout: the PDB also has 32-bit layouts of GameGUI and GUI::Item (m_pWaypointMenu
// at 4, m_userData at 556), so misaligned pointer fields mean the wrong one was picked.
bool CheckWheel()
{
    const dk2ml::Binding* needed[] = {&GameGUI_OpenWaypointMenu, &GameGUI_WaypointMenu_OnEvent, &GameGUI_m_pWaypointMenu,
                                      &GUI_Item_m_userData, &GUI_sUserData_ptr, &GUI_sEventParams_owner,
                                      &GUIManager_m_width, &GUIManager_m_height, &OPEN_WAYPOINTS_TARGETED,
                                      &OPEN_WAYPOINTS_PATH, &GUI_GAME_WAYPOINT_CLICKED};
    for (const dk2ml::Binding* b : needed)
        if (!b->Resolved()) {
            api->Log("one-page action wheel off: a name it needs is missing (listed above)");
            return false;
        }
    int32_t menu = GameGUI_m_pWaypointMenu.Offset(), userData = GUI_Item_m_userData.Offset();
    if (menu % 8 != 0 || userData % 8 != 0 || userData <= 0 || GUI_sUserData_ptr.Offset() != 0 ||
        GUI_sEventParams_owner.Offset() != 0) {
        api->Log("one-page action wheel off: layout looks wrong (m_pWaypointMenu=%d m_userData=%d ptr=%d owner=%d)", menu,
                 userData, GUI_sUserData_ptr.Offset(), GUI_sEventParams_owner.Offset());
        return false;
    }
    return true;
}

// The Editbox names, and its layout: the text pointer must be 8-aligned and after the Item base (m_state).
bool CheckEditbox()
{
    const dk2ml::Binding* needed[] = {&GUI_Editbox_SetState, &GUI_Editbox_SetText_varg, &GUI_Editbox_m_state,
                                      &GUI_Editbox_m_text, &Editbox_STATE_FOCUSED, &Editbox_STATE_UNFOCUSED};
    for (const dk2ml::Binding* b : needed)
        if (!b->Resolved()) {
            api->Log("renaming loadouts off: a name it needs is missing (listed above)");
            return false;
        }
    int32_t state = GUI_Editbox_m_state.Offset(), text = GUI_Editbox_m_text.Offset();
    if (text % 8 != 0 || state < 0 || text <= state) {
        api->Log("renaming loadouts off: GUI::Editbox layout looks wrong (m_state=%d m_text=%d)", state, text);
        return false;
    }
    return true;
}

// The campaign cards' names, and their layouts: the PDB also has 32-bit layouts of these types, so a misaligned
// pointer field or a squadName that doesn't fit in the entry means the wrong one was picked.
bool CheckCampaignCards()
{
    const dk2ml::Binding* needed[] = {&GUI_StaticImage_CloneInternal, &CampaignManager_GetDef,
                                      &g_campaignHistory, &List_CampaignHistory_m_size, &List_CampaignHistory_m_list,
                                      &List_CampaignHistory_m_elements, &sizeof_CampaignHistory, &CampaignHistory_fileName,
                                      &CampaignHistory_squadName, &CampaignDef_medalImage};
    for (const dk2ml::Binding* b : needed)
        if (!b->Resolved()) {
            api->Log("campaign cards off: a name they need is missing (listed above)");
            return false;
        }
    int32_t list = List_CampaignHistory_m_list.Offset(), elements = List_CampaignHistory_m_elements.Offset(),
            squad = CampaignHistory_squadName.Offset();
    uint32_t size = sizeof_CampaignHistory.Get();
    if (list % 8 != 0 || elements < 0 || CampaignHistory_fileName.Offset() != 0 || squad < 128 ||
        static_cast<uint32_t>(squad) + 32 > size || CampaignDef_medalImage.Offset() < 0) {
        api->Log("campaign cards off: layout looks wrong (history list=%d,%d entry size=%u squadName=%d)", list, elements,
                 size, squad);
        return false;
    }
    return true;
}
}

bool CampaignCardsUsable()
{
    return g_campaignCardsUsable;
}

bool WheelUsable()
{
    return g_wheelUsable;
}

bool WheelButtonSlotsUsable()
{
    return g_wheelButtonSlotsUsable;
}

bool EditboxUsable()
{
    return g_editboxUsable;
}

bool ConcealmentUsable()
{
    return Human_GetTemplateForUnitClassGender.Resolved() && Human_Template_concealment.Resolved() &&
           Human_Template_concealment.Offset() > 0 && Human_Template_concealment.Offset() % 4 == 0;
}

bool StartingLoadoutUsable()
{
    // 936 in build 112; the PDB's other layouts have 928 and an 888-byte template
    return Human_GetTemplateForUnitClassGender.Resolved() && Inventory_MapRandomInventoryToSlots.Resolved() &&
           Human_Template_inventory.Resolved() && Human_Template_inventory.Offset() >= 256 &&
           Human_Template_inventory.Offset() % 8 == 0;
}

int32_t UnitNameUIOffset()
{
    return g_unitNameUIUsable ? ArmyOrg_Unit_nameUI.Offset() : -1;
}

int32_t SuppressedSwitchOffset()
{
    return g_suppressedSwitchUsable ? EqpFirearm_Template_suppressedSwitch.Offset() : -1;
}

int32_t CaliberInfoOffset()
{
    return g_caliberInfoUsable ? EqpFirearm_Template_szCaliberInfo.Offset() : -1;
}

bool ParamsUsable()
{
    return g_paramsUsable;
}

float ModifiableParam(const void* tmpl, const char* name, float fallback)
{
    if (!g_paramsUsable || !tmpl)
        return fallback;
    const uint8_t* list = static_cast<const uint8_t*>(tmpl) + ModifiableParamsList_params.Offset();
    const uint8_t* values = dk2ml::At<const uint8_t*>(list, List_sValue_m_list.Offset());
    int count = dk2ml::At<int>(list, List_sValue_m_elements.Offset());
    if (!values || count <= 0 || count > 256) // a firearm has ~20; more means we're not looking at the list
        return fallback;
    uint32_t stride = sizeof_sValue.Get();
    for (int i = 0; i < count; i++) {
        const uint8_t* v = values + static_cast<size_t>(i) * stride;
        const char* key = reinterpret_cast<const char*>(v + sValue_name.Offset() + StaticString32_m_szString.Offset());
        if (std::strncmp(key, name, 32) == 0)
            return dk2ml::At<float>(v, sValue_val.Offset());
    }
    return fallback;
}

unsigned Hash(const char* s)
{
    if (!s || !*s)
        return 0;
    unsigned h = 0x811c9dc5;
    for (; *s; s++)
        h = (h ^ static_cast<unsigned char>(*s)) * 0x1000193;
    return h;
}

bool BoundToTrooper(const void* library, const void* tmpl, const void* trooper)
{
    const void* name = &dk2ml::At<char>(tmpl, Equipment_Template_name.Offset());
    const void* cls = &dk2ml::At<char>(trooper, Roster_Trooper_className.Offset());
    const void* unit = &dk2ml::At<char>(trooper, Roster_Trooper_unit.Offset());
    if (ObjectLibrary_IsEquipmentBoundTo(library, name, cls) || ObjectLibrary_IsEquipmentBoundTo(library, name, unit))
        return true;
    // a HashedString with only the type name's hash, as the game builds it
    char typeKey[64] = {};
    dk2ml::At<unsigned>(typeKey, HashedString_m_hash.Offset()) = Hash(EquipmentType_GetNameForEnum(dk2ml::At<int>(tmpl, Equipment_Template_type.Offset())));
    return ObjectLibrary_IsEquipmentBoundTo(library, typeKey, cls) || ObjectLibrary_IsEquipmentBoundTo(library, typeKey, unit);
}

void SaveAndRefresh(void* trooper)
{
    Roster_Save();
    void* gameGUI = GameGUI();
    GameGUI_Customize_OnTrooperSelected(gameGUI, trooper);
    if (GameGUI_sDeploySlot_roster_pTrooper.Offset() >= 0) {
        void*& slotTrooper = dk2ml::At<void*>(gameGUI, GameGUI_m_deploySelection.Offset() + GameGUI_sDeploySlot_roster.Offset() + GameGUI_sDeploySlot_roster_pTrooper.Offset());
        void* saved = slotTrooper;
        slotTrooper = trooper;
        GameGUI_Deploy_Update(gameGUI);
        slotTrooper = saved;
    }
}

bool Resolve()
{
    if (!dk2ml::ResolveAll(api))
        return false;

    int32_t suppressed = EqpFirearm_Template_suppressedSwitch.Offset();
    if (suppressed % 8 != 0 || suppressed < 264) { // -1 (missing) too: ResolveAll logged it
        if (suppressed >= 0)
            api->Log("EqpFirearm_Template::suppressedSwitch offset %d looks wrong, not used", suppressed);
        g_suppressedSwitchUsable = false;
    }
    // szCaliberInfo is EqpFirearm_Template's first member (+264 in build 112, right after the Equipment_Template base),
    // before suppressedSwitch; without a usable suppressedSwitch to check it against, it isn't trusted either
    int32_t caliber = EqpFirearm_Template_szCaliberInfo.Offset();
    if (caliber % 8 != 0 || caliber < 264 || !g_suppressedSwitchUsable || caliber >= suppressed) {
        if (caliber >= 0)
            api->Log("EqpFirearm_Template::szCaliberInfo offset %d looks wrong, not used", caliber);
        g_caliberInfoUsable = false;
    }

    // The PDB also holds a 32-bit layout of ArmyOrg::Unit (nameUI at 52 instead of 56); a pointer field that isn't
    // 8-aligned, or overlaps name, came from that one. Portrait headers then use the internal unit name instead.
    int32_t nameUI = ArmyOrg_Unit_nameUI.Offset();
    if (nameUI % 8 != 0 || nameUI < ArmyOrg_Unit_name.Offset() + static_cast<int32_t>(sizeof_StaticString48.Get())) {
        api->Log("ArmyOrg::Unit::nameUI offset %d looks wrong, using internal unit names", nameUI);
        g_unitNameUIUsable = false;
    }

    // The PDB also holds a 32-bit layout of Equipment_Template (guiImage at 44 instead of 80); pointer fields that
    // aren't 8-aligned came from that one, and reading through them would crash.
    int32_t guiImage = Equipment_Template_guiImage.Offset(), category = Equipment_Template_category.Offset(),
            binding = Equipment_Template_inventoryBinding.Offset();
    if (guiImage % 8 != 0 || category % 8 != 0 || binding % 8 != 0) {
        api->Log("Equipment_Template offsets look wrong (guiImage=%d category=%d inventoryBinding=%d)", guiImage, category, binding);
        return false;
    }

    // Same for ObjectLibrary (m_equipmentTemplates at 64 in a stale 32-bit layout, 128 in the real one) and
    // Roster::Trooper (inventory at 928/936 in unrelated types of the same name; 328 here).
    int32_t templates = ObjectLibrary_m_equipmentTemplates.Offset(), inventory = Roster_Trooper_inventory.Offset();
    int64_t slots = Inventory_NUM_VALUES.Get();
    if (templates % 8 != 0 || templates < 100 || inventory % 4 != 0 || sizeof_StaticString48.Get() < 48 || slots <= 0 || slots > 64) {
        api->Log("ObjectLibrary / Roster::Trooper offsets look wrong (equipmentTemplates=%d inventory=%d string size=%u)",
                 templates, inventory, sizeof_StaticString48.Get());
        return false;
    }

    api->Log("offsets (portraits): deploy pTrooper=%d+%d+%d humanPortraits=%d tex=%d+%d gender=%d portrait size=%u "
             "squad count=%d roster=%d..%d trooper size=%u name=%d+%d unit name=%d nameUI=%d squad name=%d+%d squads list=%d,%d",
             GameGUI_m_deploySelection.Offset(), GameGUI_sDeploySlot_roster.Offset(), GameGUI_sDeploySlot_roster_pTrooper.Offset(),
             ObjectLibrary_m_humanPortraits.Offset(), Roster_Trooper_id.Offset(), sHumanId_tex.Offset(),
             Roster_Trooper_id.Offset() + sHumanId_gender.Offset(), sizeof_sHumanPortrait.Get(), Roster_Squad_numInRoster.Offset(),
             Roster_Squad_roster.Offset(), Roster_Squad_diedInCampaign.Offset(), sizeof_Roster_Trooper.Get(), sHumanId_name.Offset(),
             StaticString48_m_szString.Offset(), ArmyOrg_Unit_name.Offset(), UnitNameUIOffset(), Roster_Squad_name.Offset(),
             StaticString32_m_szString.Offset(), List_SquadPtr_m_list.Offset(), List_SquadPtr_m_elements.Offset());
    api->Log("offsets: customizeSelection=%d equipmentTemplates=%d trooper unit/class/inventory=%d/%d/%d string size=%u "
             "hash=%d eqp name/tooltip=%d/%d inventory slots=%lld text color=%d+%d Equipment_Client size=%u alignOffset=%d",
             GameGUI_m_pCustomizeSelection.Offset(), templates, Roster_Trooper_unit.Offset(), Roster_Trooper_className.Offset(),
             inventory, sizeof_StaticString48.Get(), StaticString48_m_hash.Offset(), Equipment_Template_name.Offset(),
             Equipment_Template_tooltip.Offset(), slots, GUI_StaticText_m_textRenderObject.Offset(),
             GUI_sTextRenderObject_color.Offset(), sizeof_Equipment_Client.Get(), GUI_Item_m_alignOffset.Offset());
    // The PDB holds several layouts of some types (eg. GUI::Item, GameGUI); log what was picked so a wrong one shows
    // up in dk2ml.log / the symtest dry-run.
    // (item lookups, the child list, actions and Callback wiring are the loader's GUI kit now)
    api->Log("offsets: customizationScreen=%d size=%d origin=%d align=%d stealFocus=%d text=%d "
             "eqp type=%d guiImage=%d category=%d binding=%d image render=%d scale/size/needsUpdate=%d/%d/%d",
             GameGUI_m_pCustomizationScreen.Offset(), GUI_Item_m_size.Offset(), GUI_Item_m_localOrigin.Offset(),
             GUI_Item_m_alignFlags.Offset(), GUI_Item_m_bStealFocus.Offset(), GUI_StaticText_m_text.Offset(),
             Equipment_Template_type.Offset(),
             guiImage, category, binding, GUI_StaticImage_m_renderObject.Offset(), RenderObject2D_scale.Offset(),
             RenderObject2D_size.Offset(), RenderObject2D_bNeedsUpdate.Offset());
    g_campaignCardsUsable = CheckCampaignCards();
    g_editboxUsable = CheckEditbox();
    g_wheelUsable = CheckWheel();
    api->Log("offsets (action wheel): m_pWaypointMenu=%d m_userData=%d+%d owner=%d gui size=%d,%d types=%lld/%lld clicked=%lld%s",
             GameGUI_m_pWaypointMenu.Offset(), GUI_Item_m_userData.Offset(), GUI_sUserData_ptr.Offset(),
             GUI_sEventParams_owner.Offset(), GUIManager_m_width.Offset(), GUIManager_m_height.Offset(),
             OPEN_WAYPOINTS_TARGETED.Get(), OPEN_WAYPOINTS_PATH.Get(), GUI_GAME_WAYPOINT_CLICKED.Get(),
             g_wheelUsable ? "" : " (off)");
    g_wheelButtonSlotsUsable = g_wheelUsable && CheckWheelButtonSlots();
    api->Log("offsets (action wheel buttons): eventProperties=%d stride=%u actions=%d list=%d,%d eventId=%d "
             "eventParams=%d iParam1=%d iParam2=%d%s",
             GUI_Item_m_eventProperties.Offset(), sizeof_ItemEventProperties.Get(), ItemEventProperties_actions.Offset(),
             List_ActionPtr_m_list.Offset(), List_ActionPtr_m_elements.Offset(), GUI_sAction_eventId.Offset(),
             GUI_sAction_eventParams.Offset(), GUI_sEventParams_iParam1.Offset(), GUI_sEventParams_iParam2.Offset(),
             g_wheelButtonSlotsUsable ? "" : " (off)");
    api->Log("offsets (action wheel rings): RenderObject2D::texture=%d", RenderObject2D_texture.Offset());
    api->Log("offsets (editbox): state=%d text=%d", GUI_Editbox_m_state.Offset(), GUI_Editbox_m_text.Offset());
    api->Log("offsets (concealment): Human_Template::concealment=%d%s", Human_Template_concealment.Offset(),
             ConcealmentUsable() ? "" : " (not usable: the concealment bar stays for everyone)");
    api->Log("offsets (starting loadout): Human_Template::inventory=%d%s", Human_Template_inventory.Offset(),
             StartingLoadoutUsable() ? "" : " (off: no starting loadout row)");
    api->Log("offsets (campaign cards): history list=%d,%d,%d entry size=%u fileName=%d squadName=%d medalImage=%d%s",
             List_CampaignHistory_m_size.Offset(), List_CampaignHistory_m_list.Offset(),
             List_CampaignHistory_m_elements.Offset(), sizeof_CampaignHistory.Get(), CampaignHistory_fileName.Offset(),
             CampaignHistory_squadName.Offset(), CampaignDef_medalImage.Offset(), g_campaignCardsUsable ? "" : " (off)");
    api->Log("offsets: checkbox buttons=%d button renderObjects=%d RenderObject2D size=%u suppressedSwitch=%d caliberInfo=%d",
             GUI_Checkbox_m_pButtons.Offset(), GUI_Button_m_renderObjects.Offset(), sizeof_RenderObject2D.Get(),
             SuppressedSwitchOffset(), CaliberInfoOffset());
    g_paramsUsable = CheckParams();
    api->Log("offsets (modifiable params): params=%d list=%d,%d sValue size=%u name=%d val=%d%s",
             ModifiableParamsList_params.Offset(), List_sValue_m_list.Offset(), List_sValue_m_elements.Offset(),
             sizeof_sValue.Get(), sValue_name.Offset(), sValue_val.Offset(), g_paramsUsable ? "" : " (off)");
    return true;
}

} // namespace game
