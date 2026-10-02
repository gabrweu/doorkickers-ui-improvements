#include "game.h"

#include <iterator>
#include <utility>

namespace game {

const DK2ML_API* api = nullptr;
Functions fn = {};
Globals globals = {};
Offsets off = {};

namespace {

bool g_ok = true;

template <typename T>
void Sym(const char* name, T* out)
{
    *out = reinterpret_cast<T>(api->ResolveSymbol(name));
    if (!*out) {
        api->Log("missing symbol %s", name);
        g_ok = false;
    }
}

void Field(const char* type, const char* field, int32_t* out)
{
    *out = api->GetFieldOffset(type, field);
    if (*out < 0) {
        api->Log("missing field %s::%s", type, field);
        g_ok = false;
    }
}

void Size(const char* type, uint32_t* out)
{
    *out = api->GetTypeSize(type);
    if (*out == 0) {
        api->Log("missing type %s", type);
        g_ok = false;
    }
}

void Enum(const char* type, const char* name, int64_t* out)
{
    if (api->GetEnumValue(type, name, out) != DK2ML_OK) {
        api->Log("missing enum %s::%s", type, name);
        g_ok = false;
    }
}

} // namespace

bool Resolve()
{
    Sym("?Init@ItemList@GUI@@QEAAXXZ", &fn.itemListInit);
    Sym("GameGUI::Customize_OnTrooperSelected", &fn.customizeOnTrooperSelected);
    Sym("GameGUI::Customize_OnEvent", &fn.customizeOnEvent);
    Sym("GameGUI::Customize_OnChangePortrait", &fn.customizeOnChangePortrait);
    Sym("?UpdateHumanTooltip@@YAXPEAVItem@GUI@@_N11W4eHumanType@@AEBUsHumanId@@AEBV?$StaticString@$0DA@@@4QEBUEquipment_Client@@W4eType@Inventory@@PEBV?$List@I@@HHHHIPEBD@Z", &fn.updateHumanTooltip);
    Sym("GameGUI::Deploy_Update", &fn.deployUpdate);
    Sym("Roster::Save", &fn.rosterSave);
    Sym("?GetSquadForTrooper@Roster@@YAPEBUSquad@1@PEBUTrooper@1@@Z", &fn.getSquadForTrooper);
    Sym("?GetUnit@ObjectLibrary@@QEBAPEBUUnit@ArmyOrg@@AEBV?$StaticString@$0DA@@@@Z", &fn.getUnit);
    Sym("?GetTextForId@Loca@@YAPEBDPEBD_N1@Z", &fn.getTextForId);
    Sym("?FileExists@FileManagerImpl@@UEBA_NPEBD@Z", &fn.fileExists);
    Sym("?ExecuteOnEvent@Item@GUI@@IEAAXW4eItemEventType@12@VVector2@@@Z", &fn.itemExecuteOnEvent);
    Sym("?IsEquipmentBoundTo@ObjectLibrary@@QEBA_NAEBVHashedString@@AEBV?$StaticString@$0DA@@@@Z", &fn.isEquipmentBoundTo);
    Sym("?GetNameForEnum@Inventory@@YAPEBDW4eType@1@@Z", &fn.inventoryName);
    Sym("?GetNameForEnum@EquipmentType@@YAPEBDW4eType@1@@Z", &fn.equipmentTypeName);
    Sym("GUI::sAction::Execute", &fn.actionExecute);
    Sym("?FindChild@Item@GUI@@QEAAPEAV12@PEBD@Z", &fn.findChild); // overloaded; this one finds by name
    Sym("GUI::Item::Show", &fn.itemShow);
    Sym("GUI::Item::Hide", &fn.itemHide);
    Sym("GUI::Item::SetName", &fn.itemSetName);
    Sym("GUI::Button::CloneInternal", &fn.buttonClone);
    Sym("GUI::Button::UpdatePointerReferences", &fn.buttonUpdatePointerReferences);
    Sym("?CloneInternal@Item@GUI@@UEBAPEAV12@XZ", &fn.itemClone);
    Sym("?UpdatePointerReferences@Item@GUI@@UEAAXPEAV12@0@Z", &fn.itemUpdatePointerReferences);
    Sym("GUI::StaticImage::SetImageAndImplicitSize", &fn.setImage);
    Sym("?ChangeText@StaticText@GUI@@QEAA_NPEBD@Z", &fn.staticTextChangeText);
    // overloaded (also by HashedString); this one takes the template name
    Sym("?GetEquipment@ObjectLibrary@@QEBAPEBVEquipment_Template@@PEBD@Z", &fn.getEquipment);

    Sym("g_pGUIManager", &globals.guiManager);
    Sym("g_pGameGUI", &globals.gameGUI);
    Sym("?g_pObjectLibrary@@3PEAVObjectLibrary@@EA", &globals.objectLibrary);
    Sym("??_7Button@GUI@@6B@", &globals.buttonVtbl);
    Sym("??_7StaticImage@GUI@@6B@", &globals.staticImageVtbl);
    Sym("??_7StaticText@GUI@@6B@", &globals.staticTextVtbl);
    Sym("??_7Checkbox@GUI@@6B@", &globals.checkboxVtbl);
    Sym("?g_fileManager@@3PEAVFileManager@@EA", &globals.fileManager);
    Sym("?m_squads@Roster@@3V?$List@PEAUSquad@Roster@@@@A", &globals.rosterSquads);

    Field("GUIManager", "m_pRoot", &off.guiRoot);
    Field("GameGUI", "m_pCustomizationScreen", &off.customizationScreen);
    Field("GameGUI", "m_pCustomizeSelection", &off.customizeSelection);
    Field("GameGUI", "m_deploySelection", &off.deploySelection);
    Field("GameGUI::sDeploySlot", "roster", &off.deployRoster);
    // optional: without it only the deploy slot's small portrait waits for the screen's next refresh
    off.deployRosterTrooper = api->GetFieldOffset("GameGUI::sDeploySlot::<unnamed-tag>::<unnamed-type-roster>", "pTrooper");
    Field("ObjectLibrary", "m_equipmentTemplates", &off.libraryTemplates);
    Field("Roster::Trooper", "unit", &off.trooperUnit);
    Field("Roster::Trooper", "className", &off.trooperClass);
    Field("Roster::Trooper", "inventory", &off.trooperInventory);
    Size("StaticString<48>", &off.staticStringSize);
    Field("StaticString<48>", "m_hash", &off.staticStringHash);
    Field("StaticString<48>", "m_szString", &off.staticStringText);
    Field("HashedString", "m_hash", &off.hashedStringHash);

    Field("Roster::Trooper", "id", &off.trooperId);
    Field("sHumanId", "tex", &off.humanIdTex);
    Field("sHumanId", "texHash", &off.humanIdTexHash);
    Field("sHumanId", "gender", &off.humanIdGender);
    Field("sHumanId", "name", &off.humanIdName);
    Size("Roster::Trooper", &off.trooperSize);
    Field("ArmyOrg::Unit", "name", &off.unitName);
    Field("ArmyOrg::Unit", "nameUI", &off.unitNameUI);
    Field("Roster::Squad", "numInRoster", &off.squadCount);
    Field("Roster::Squad", "roster", &off.squadRoster);
    Field("Roster::Squad", "diedInCampaign", &off.squadRosterEnd);
    Field("Roster::Squad", "name", &off.squadName);
    Field("StaticString<32>", "m_szString", &off.staticString32Text);
    Field("List<Roster::Squad *>", "m_list", &off.squadListData);
    Field("List<Roster::Squad *>", "m_elements", &off.squadListCount);
    Field("ObjectLibrary", "m_humanPortraits", &off.libraryPortraits);
    Field("List<sHumanPortrait>", "m_list", &off.listData);
    Field("List<sHumanPortrait>", "m_elements", &off.listCount);
    Size("sHumanPortrait", &off.portraitSize);
    Field("sHumanPortrait", "tex", &off.portraitTex);
    Field("sHumanPortrait", "unit", &off.portraitUnit);
    Field("sHumanPortrait", "className", &off.portraitClass);
    Field("sHumanPortrait", "gender", &off.portraitGender);

    Field("GUI::Item", "m_hashedName", &off.itemName);
    Field("GUI::Item", "m_parent", &off.itemParent);
    Field("GUI::Item", "m_children", &off.itemChildren);
    Field("GUI::Item", "m_size", &off.itemSize);
    Field("GUI::Item", "m_localOrigin", &off.itemLocalOrigin);
    Field("GUI::Item", "m_hidden", &off.itemHidden);
    Field("GUI::Item", "m_alignFlags", &off.itemAlignFlags);
    Field("GUI::Item", "m_alignOffset", &off.itemAlignOffset);
    Field("GUI::Item", "m_bStealFocus", &off.itemStealFocus);
    Field("GUI::Item", "m_eventProperties", &off.itemEvents);
    Field("LinkedList<GUI::Item>", "head", &off.linkHead);
    Field("LinkedList<GUI::Item>", "next", &off.linkNext);
    Field("LinkedList<GUI::Item>", "owner", &off.linkOwner);
    Field("HashedString", "m_pString", &off.hashedStringText);
    Field("GUI::StaticText", "m_text", &off.staticTextText);
    Field("GUI::StaticText", "m_textRenderObject", &off.staticTextRender);
    Field("GUI::sTextRenderObject", "color", &off.textRenderColor);
    Size("Equipment_Client", &off.equipmentClientSize);
    Field("Equipment_Client", "pItem", &off.equipmentClientItem);
    Field("GUI::StaticImage", "m_renderObject", &off.staticImageRender);
    Field("RenderObject2D", "scale", &off.renderScale);
    Field("RenderObject2D", "size", &off.renderSize);
    Field("RenderObject2D", "bNeedsUpdate", &off.renderNeedsUpdate);
    Size("RenderObject2D", &off.renderObjectSize);
    Field("GUI::Checkbox", "m_pButtons", &off.checkboxButtons);
    Field("GUI::Button", "m_renderObjects", &off.buttonRenderObjects);
    Size("GUI::Item::sItemEventProperties", &off.eventPropsSize);
    Field("GUI::Item::sItemEventProperties", "actions", &off.eventPropsActions);
    Field("List<GUI::sAction *>", "m_list", &off.actionListData);
    Field("List<GUI::sAction *>", "m_elements", &off.actionListCount);

    Size("GUI::sAction", &off.actionSize);
    Field("GUI::sAction", "owner", &off.actionOwner);
    Field("GUI::sAction", "action", &off.actionType);
    Field("GUI::sAction", "targetName", &off.actionTargetName);
    Field("GUI::sAction", "target", &off.actionTarget);
    Field("GUI::sAction", "params", &off.actionParams);
    Field("GUI::sAction", "pCallback", &off.actionCallback);
    Field("GUI::sAction", "eventParams", &off.actionEventParams);
    Field("GUI::sEventParams", "cursor", &off.eventParamsCursor);

    Field("Equipment_Template", "type", &off.eqpType);
    Field("Equipment_Template", "name", &off.eqpName);
    Field("Equipment_Template", "tooltip", &off.eqpTooltip);
    Field("Equipment_Template", "guiImage", &off.eqpGuiImage);
    Field("Equipment_Template", "category", &off.eqpCategory);
    Field("Equipment_Template", "inventoryBinding", &off.eqpBinding);
    // optional: without it, squadmates with a suppressed weapon just don't count for the unsuppressed one
    off.firearmSuppressedSwitch = api->GetFieldOffset("EqpFirearm_Template", "suppressedSwitch");
    if (off.firearmSuppressedSwitch % 8 != 0 || off.firearmSuppressedSwitch < 264) {
        api->Log("EqpFirearm_Template::suppressedSwitch offset %d looks wrong, not used", off.firearmSuppressedSwitch);
        off.firearmSuppressedSwitch = -1;
    }

    for (auto [name, out] : {std::pair{"ACTION_ADD_CHILD", &off.actionAddChild}, std::pair{"ACTION_SET_ORIGIN", &off.actionSetOrigin},
                             std::pair{"ACTION_CALLBACK", &off.actionCallbackType}})
        Enum("GUI::eAction", name, out);
    Enum("GUI::Item::eItemEventType", "EVENT_SCROLL_DOWN", &off.eventScrollDown);
    Enum("GUI::Item::eItemEventType", "EVENT_SCROLL_UP", &off.eventScrollUp);
    Enum("GUI::Item::eItemEventType", "EVENT_CURSOR_DOWN", &off.eventCursorDown);
    Enum("GUI::Item::eItemEventType", "EVENT_DRAG", &off.eventDrag);
    Enum("GUI::Item::eItemEventType", "EVENT_CURSOR_HOVER", &off.eventCursorHover);
    Enum("GUI::Item::eItemEventType", "EVENT_CURSOR_HOVER_END", &off.eventCursorHoverEnd);
    Enum("GUI::Item::eItemEventType", "EVENT_CLICK", &off.eventClick);
    Enum("GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_BACK", &off.customizeBack);
    Enum("GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_OPENED", &off.customizeOpened);
    Enum("GUI::Events::eEventType", "GUI_GAME_CUSTOMIZE_CLOSED", &off.customizeClosed);
    Enum("EquipmentType::eType", "FIREARM", &off.eqpFirearm);
    Enum("EquipmentType::eType", "EMPTY", &off.eqpEmpty);
    Enum("Inventory::eType", "NUM_VALUES", &off.invCount);
    Enum("Inventory::eType", "UTILITY_POUCH1", &off.invUtility1);
    Enum("Inventory::eType", "SUPPORT_GEAR3", &off.invSupport3);
    const char* managed[] = {"PRIMARY_WEAPON_SCOPE",   "PRIMARY_WEAPON_AMMO",   "PRIMARY_WEAPON_MUZZLE", "SECONDARY_WEAPON_SCOPE",
                             "SECONDARY_WEAPON_AMMO", "SECONDARY_WEAPON_MUZZLE", "HELMET_NVG"};
    static_assert(std::size(managed) == std::size(off.invGameManaged));
    for (size_t i = 0; i < std::size(managed); i++)
        Enum("Inventory::eType", managed[i], &off.invGameManaged[i]);

    // The PDB also holds a 32-bit layout of ArmyOrg::Unit (nameUI at 52 instead of 56); a pointer field that isn't
    // 8-aligned, or overlaps name, came from that one. Portrait headers then use the internal unit name instead.
    if (off.unitNameUI >= 0 && (off.unitNameUI % 8 != 0 || off.unitNameUI < off.unitName + static_cast<int32_t>(off.staticStringSize))) {
        api->Log("ArmyOrg::Unit::nameUI offset %d looks wrong, using internal unit names", off.unitNameUI);
        off.unitNameUI = -1;
    }

    // The PDB also holds a 32-bit layout of Equipment_Template (guiImage at 44 instead of 80); pointer fields that
    // aren't 8-aligned came from that one, and reading through them would crash.
    if (g_ok && (off.eqpGuiImage % 8 != 0 || off.eqpCategory % 8 != 0 || off.eqpBinding % 8 != 0)) {
        api->Log("Equipment_Template offsets look wrong (guiImage=%d category=%d inventoryBinding=%d)", off.eqpGuiImage,
                 off.eqpCategory, off.eqpBinding);
        g_ok = false;
    }

    // Same for ObjectLibrary (m_equipmentTemplates at 64 in a stale 32-bit layout, 128 in the real one) and
    // Roster::Trooper (inventory at 928/936 in unrelated types of the same name; 328 here).
    if (g_ok && (off.libraryTemplates % 8 != 0 || off.libraryTemplates < 100 || off.trooperInventory % 4 != 0 ||
                 off.staticStringSize < 48 || off.invCount <= 0 || off.invCount > 64)) {
        api->Log("ObjectLibrary / Roster::Trooper offsets look wrong (equipmentTemplates=%d inventory=%d string size=%u)",
                 off.libraryTemplates, off.trooperInventory, off.staticStringSize);
        g_ok = false;
    }

    if (g_ok) {
        api->Log("offsets (portraits): deploy pTrooper=%d+%d+%d humanPortraits=%d tex=%d+%d gender=%d portrait size=%u "
                 "squad count=%d roster=%d..%d trooper size=%u name=%d+%d unit name=%d nameUI=%d squad name=%d+%d squads list=%d,%d",
                 off.deploySelection, off.deployRoster, off.deployRosterTrooper, off.libraryPortraits, off.trooperId,
                 off.humanIdTex, off.trooperId + off.humanIdGender, off.portraitSize, off.squadCount, off.squadRoster,
                 off.squadRosterEnd, off.trooperSize, off.humanIdName, off.staticStringText, off.unitName, off.unitNameUI,
                 off.squadName, off.staticString32Text, off.squadListData, off.squadListCount);
        api->Log("offsets: customizeSelection=%d equipmentTemplates=%d trooper unit/class/inventory=%d/%d/%d string size=%u "
                 "hash=%d eqp name/tooltip=%d/%d inventory slots=%lld text color=%d+%d Equipment_Client size=%u alignOffset=%d",
                 off.customizeSelection, off.libraryTemplates, off.trooperUnit, off.trooperClass, off.trooperInventory,
                 off.staticStringSize, off.staticStringHash, off.eqpName, off.eqpTooltip, off.invCount, off.staticTextRender, off.textRenderColor,
                 off.equipmentClientSize, off.itemAlignOffset);
        // The PDB holds several layouts of some types (eg. GUI::Item, GameGUI); log what was picked so a wrong one
        // shows up in dk2ml.log / the symtest dry-run.
        api->Log("offsets: customizationScreen=%d item name=%d parent=%d children=%d size=%d origin=%d hidden=%d align=%d "
                 "stealFocus=%d events=%d link head/next/owner=%d/%d/%d text=%d sAction size=%u pCallback=%d "
                 "eqp type=%d guiImage=%d category=%d binding=%d image render=%d scale/size/needsUpdate=%d/%d/%d",
                 off.customizationScreen, off.itemName, off.itemParent, off.itemChildren, off.itemSize, off.itemLocalOrigin,
                 off.itemHidden, off.itemAlignFlags, off.itemStealFocus, off.itemEvents, off.linkHead, off.linkNext,
                 off.linkOwner, off.staticTextText, off.actionSize, off.actionCallback, off.eqpType, off.eqpGuiImage,
                 off.eqpCategory, off.eqpBinding, off.staticImageRender, off.renderScale, off.renderSize,
                 off.renderNeedsUpdate);
        api->Log("offsets: checkbox buttons=%d button renderObjects=%d RenderObject2D size=%u suppressedSwitch=%d",
                 off.checkboxButtons, off.buttonRenderObjects, off.renderObjectSize, off.firearmSuppressedSwitch);
    }
    return g_ok;
}

} // namespace game
