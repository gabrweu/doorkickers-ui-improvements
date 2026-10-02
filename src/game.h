// Game functions, globals and struct layouts used by the plugin, all resolved by name from the PDB.
#pragma once

#include <cstdint>

#include "dk2ml.h"

namespace game {

// --- hooked functions (safe hooks in plugin.cpp; the signatures say where the arguments are in DK2ML_Regs) ---
// void GUI::ItemList::Init(): lays out a list's children. The stock equipment list builder (on
// GUI_GAME_CUSTOMIZE_INVENTORY_SELECT) calls it right after cloning and filling every item.
using ItemListInitFn = void (*)(void* itemList);
// void GameGUI::Customize_OnTrooperSelected(const Roster::Trooper*): fills the Customization screen for a trooper.
// Called when the screen opens and when another trooper is picked; does nothing while the screen is hidden.
using CustomizeOnTrooperSelectedFn = void (*)(void* gameGUI, const void* trooper);
// bool GameGUI::Customize_OnEvent(unsigned eventId, const GUI::sEventParams*)
using CustomizeOnEventFn = bool (*)(void* gameGUI, unsigned eventId, const void* params);
// void GameGUI::Customize_OnChangePortrait(bool next): the stock PREV/NEXT arrows on the large portrait
using CustomizeOnChangePortraitFn = void (*)(void* gameGUI, bool next);
// void UpdateHumanTooltip(GUI::Item* root, bool, bool, bool, eHumanType, const sHumanId&, const StaticString<48>&,
//     const StaticString<48>&, const Equipment_Client* equipment, Inventory::eType, ...): fills the trooper hover
//     tooltip (HUD_Human_Tooltip) in missions and on the deploy screen. equipment is the 9th argument (stack[9] in a
//     pre hook): Equipment_Client[Inventory::NUM_VALUES], indexed by slot.
using UpdateHumanTooltipFn = void (*)(void* root);
// GUI::Item::Show() / Hide() (ItemFn below): every XML Show/Hide of an inventory slot Checkbox goes through them

// --- called functions ---
using ActionExecuteFn = void (*)(void* action);
using FindChildFn = void* (*)(void* item, const char* name);            // recursive, by name
using ItemFn = void (*)(void* item);                                    // Show / Hide
using SetNameFn = void (*)(void* item, const char* name);
using CloneFn = void* (*)(const void* item);                            // CloneInternal: deep copy, no parent
using UpdatePointerReferencesFn = void (*)(void* item, void* localRoot, void* globalRoot);
using SetImageFn = void (*)(void* staticImage, const char* texture);    // also sizes the image to the texture
using ChangeTextFn = bool (*)(void* staticText, const char* text);    // copies text (up to the item's length limit)
// const Equipment_Template* ObjectLibrary::GetEquipment(const char* name) const
using GetEquipmentFn = const void* (*)(const void* objectLibrary, const char* name);
// bool ObjectLibrary::IsEquipmentBoundTo(const HashedString& eqp, const StaticString<48>& to) const: a <Bind> ties
// the equipment (or its type, see slots.cpp) to a unit or class
using IsEquipmentBoundToFn = bool (*)(const void* objectLibrary, const void* eqp, const void* to);
using GetNameForEnumFn = const char* (*)(int value); // Inventory:: / EquipmentType::GetNameForEnum
// void GUI::Item::ExecuteOnEvent(eItemEventType, Vector2 cursor): runs the item's XML actions for that event, as if
// it happened; the Vector2 is passed by value, its 8 bytes in a register
using ExecuteOnEventFn = void (*)(void* item, int event, uint64_t cursor);
using DeployUpdateFn = void (*)(void* gameGUI);
using RosterSaveFn = void (*)();
using GetSquadForTrooperFn = const void* (*)(const void* trooper);      // Roster::GetSquadForTrooper
// const ArmyOrg::Unit* ObjectLibrary::GetUnit(const StaticString<48>&) const: compares only the key's m_hash
using GetUnitFn = const void* (*)(const void* objectLibrary, const void* key);
// const char* Loca::GetTextForId(const char* id, bool, bool): the localized text for an "@id", else null
using GetTextForIdFn = const char* (*)(const char* id, bool a, bool b);
// bool FileManagerImpl::FileExists(const char* path) const: searches the data folders, packed archives and mods
using FileExistsFn = bool (*)(const void* fileManager, const char* path);

struct Functions {
    ItemListInitFn itemListInit;
    CustomizeOnTrooperSelectedFn customizeOnTrooperSelected;
    CustomizeOnEventFn customizeOnEvent;
    CustomizeOnChangePortraitFn customizeOnChangePortrait;
    UpdateHumanTooltipFn updateHumanTooltip;
    ExecuteOnEventFn itemExecuteOnEvent;
    IsEquipmentBoundToFn isEquipmentBoundTo;
    GetNameForEnumFn inventoryName;     // eg. "PrimaryWeapon", "Custom3": the slot Checkbox's name
    GetNameForEnumFn equipmentTypeName; // eg. "Grenade"

    ActionExecuteFn actionExecute;
    FindChildFn findChild;
    ItemFn itemShow;
    ItemFn itemHide;
    SetNameFn itemSetName;
    CloneFn buttonClone;
    UpdatePointerReferencesFn buttonUpdatePointerReferences;
    CloneFn itemClone;                                           // Item::CloneInternal (header template)
    UpdatePointerReferencesFn itemUpdatePointerReferences;
    SetImageFn setImage;
    ChangeTextFn staticTextChangeText;
    GetEquipmentFn getEquipment;
    DeployUpdateFn deployUpdate;
    RosterSaveFn rosterSave;
    GetSquadForTrooperFn getSquadForTrooper;
    GetUnitFn getUnit;
    GetTextForIdFn getTextForId;
    FileExistsFn fileExists;
};

struct Globals {
    void** guiManager;    // g_pGUIManager
    void** gameGUI;       // g_pGameGUI
    void** objectLibrary; // g_pObjectLibrary
    const void* buttonVtbl;      // GUI::Button::`vftable', to type-check items before class-specific calls
    const void* staticImageVtbl; // GUI::StaticImage::`vftable'
    const void* staticTextVtbl;  // GUI::StaticText::`vftable'
    const void* checkboxVtbl;    // GUI::Checkbox::`vftable' (the inventory slots)
    void** fileManager;          // g_fileManager (a FileManagerImpl)
    void* rosterSquads;          // Roster::m_squads, List<Roster::Squad*> (the list itself)
};

struct Offsets {
    int32_t guiRoot;               // GUIManager::m_pRoot
    int32_t customizationScreen;   // GameGUI::m_pCustomizationScreen
    int32_t customizeSelection;    // GameGUI::m_pCustomizeSelection: the trooper being customized
    int32_t deploySelection;       // GameGUI::m_deploySelection (sDeploySlot)
    int32_t deployRoster;          // sDeploySlot::roster (unnamed struct)
    int32_t deployRosterTrooper;   // ...::roster::pTrooper; -1 if missing (only used to refresh the deploy slot)

    int32_t libraryTemplates;      // ObjectLibrary::m_equipmentTemplates, std::unordered_map<unsigned, Equipment_Template*>
    int32_t trooperUnit;           // Roster::Trooper::unit (StaticString<48>)
    int32_t trooperClass;          // Roster::Trooper::className (StaticString<48>)
    int32_t trooperInventory;      // Roster::Trooper::inventory, StaticString<48>[Inventory::NUM_VALUES]: equipped names
    uint32_t staticStringSize;     // sizeof(StaticString<48>)
    int32_t staticStringHash;      // StaticString<48>::m_hash (0 = empty)
    int32_t staticStringText;      // StaticString<48>::m_szString, char[]
    int32_t hashedStringHash;      // HashedString::m_hash

    // portraits (portraits.cpp, picker.cpp)
    int32_t trooperId;             // Roster::Trooper::id (sHumanId)
    int32_t humanIdTex;            // sHumanId::tex, char[]: the portrait texture path, saved to roster.xml
    int32_t humanIdTexHash;        // sHumanId::texHash (only used to bound tex)
    int32_t humanIdGender;         // sHumanId::gender
    int32_t humanIdName;           // sHumanId::name (StaticString<48>)
    uint32_t trooperSize;          // sizeof(Roster::Trooper): stride of Squad::roster
    int32_t unitName;              // ArmyOrg::Unit::name (StaticString<48>), the internal name, eg. "Rangers"
    int32_t unitNameUI;            // ArmyOrg::Unit::nameUI, char*: localization id, eg. "@unit_rangers_name"; -1 if unusable
    int32_t squadCount;            // Roster::Squad::numInRoster
    int32_t squadRoster;           // Roster::Squad::roster: inline Roster::Trooper[]
    int32_t squadRosterEnd;        // Roster::Squad::diedInCampaign, the field after the array (bounds the count)
    int32_t squadName;             // Roster::Squad::name (StaticString<32>)
    int32_t staticString32Text;    // StaticString<32>::m_szString
    int32_t squadListData;         // List<Roster::Squad *>::m_list
    int32_t squadListCount;        // List<Roster::Squad *>::m_elements
    int32_t libraryPortraits;      // ObjectLibrary::m_humanPortraits (List<sHumanPortrait>)
    int32_t listData;              // List<sHumanPortrait>::m_list
    int32_t listCount;             // List<sHumanPortrait>::m_elements
    uint32_t portraitSize;         // sizeof(sHumanPortrait)
    int32_t portraitTex;           // sHumanPortrait::tex, char[128]
    int32_t portraitUnit;          // sHumanPortrait::unit: hash of the unit name, as StaticString<48>::m_hash
    int32_t portraitClass;         // sHumanPortrait::className: hash, 0 = any class
    int32_t portraitGender;        // sHumanPortrait::gender

    int32_t itemName;              // GUI::Item::m_hashedName (HashedString)
    int32_t itemParent;            // GUI::Item::m_parent
    int32_t itemChildren;          // GUI::Item::m_children (LinkedList<GUI::Item>, the list's head node)
    int32_t itemSize;              // GUI::Item::m_size, int[2]
    int32_t itemLocalOrigin;       // GUI::Item::m_localOrigin, Vector2
    int32_t itemHidden;            // GUI::Item::m_hidden
    int32_t itemAlignFlags;        // GUI::Item::m_alignFlags
    int32_t itemAlignOffset;       // GUI::Item::m_alignOffset, Vector2: the XML origin of an aligned item
    int32_t itemStealFocus;        // GUI::Item::m_bStealFocus
    int32_t itemEvents;            // GUI::Item::m_eventProperties, sItemEventProperties[EVENT_NUM_VALUES]
    int32_t linkHead;              // LinkedList<GUI::Item>::head
    int32_t linkNext;              // LinkedList<GUI::Item>::next
    int32_t linkOwner;             // LinkedList<GUI::Item>::owner: the item a node belongs to
    int32_t hashedStringText;      // HashedString::m_pString
    int32_t staticTextText;        // GUI::StaticText::m_text, char*
    int32_t staticTextRender;      // GUI::StaticText::m_textRenderObject (GUI::sTextRenderObject)
    int32_t textRenderColor;       // GUI::sTextRenderObject::color, 0xAARRGGBB
    uint32_t equipmentClientSize;  // sizeof(Equipment_Client): stride of the tooltip's equipment array
    int32_t equipmentClientItem;   // Equipment_Client::pItem (const Equipment_Template*)
    int32_t staticImageRender;     // GUI::StaticImage::m_renderObject (an inline RenderObject2D)
    int32_t renderScale;           // RenderObject2D::scale, Vector2: drawn size = size * scale
    int32_t renderSize;            // RenderObject2D::size, Vector2: the texture's size in pixels
    int32_t renderNeedsUpdate;     // RenderObject2D::bNeedsUpdate: rebuild the quad from size/scale
    uint32_t renderObjectSize;     // sizeof(RenderObject2D)
    int32_t checkboxButtons;       // GUI::Checkbox::m_pButtons, GUI::Button*[2]: the unchecked / checked state
    int32_t buttonRenderObjects;   // GUI::Button::m_renderObjects, RenderObject2D[3]: normal / hover / click
    uint32_t eventPropsSize;       // sizeof(GUI::Item::sItemEventProperties)
    int32_t eventPropsActions;     // sItemEventProperties::actions, List<sAction*>
    int32_t actionListData;        // List<GUI::sAction*>::m_list
    int32_t actionListCount;       // List<GUI::sAction*>::m_elements

    uint32_t actionSize;           // sizeof(GUI::sAction)
    int32_t actionOwner;           // GUI::sAction::owner
    int32_t actionType;            // GUI::sAction::action
    int32_t actionTargetName;      // GUI::sAction::targetName
    int32_t actionTarget;          // GUI::sAction::target
    int32_t actionParams;          // GUI::sAction::params
    int32_t actionCallback;        // GUI::sAction::pCallback, void (*)(const sAction&)
    int32_t actionEventParams;     // GUI::sAction::eventParams (GUI::sEventParams), filled in by Item::ExecuteOnEvent
    int32_t eventParamsCursor;     // GUI::sEventParams::cursor, Vector2 in GUI space (+y up)

    int32_t eqpType;               // Equipment_Template::type (EquipmentType::eType)
    int32_t eqpName;               // Equipment_Template::name (HashedString): the template name binds refer to
    int32_t eqpTooltip;            // Equipment_Template::tooltip, char*: the game skips items without one
    int32_t eqpGuiImage;           // Equipment_Template::guiImage, char*: the list icon, eg. data/models/weapons/m4_ui.dds
    int32_t eqpCategory;           // Equipment_Template::category (HashedString), eg. "rifle"; firearms only
    int32_t eqpBinding;            // Equipment_Template::inventoryBinding, char*: the slot, eg. "PrimaryWeapon"
    int32_t firearmSuppressedSwitch; // EqpFirearm_Template::suppressedSwitch, char*: the other half of a weapon's
                                     // suppressed / unsuppressed pair (the muzzle slot equips the "_Sup" firearm); -1 if unusable

    int64_t actionAddChild;        // GUI::eAction::ACTION_ADD_CHILD
    int64_t actionSetOrigin;       // GUI::eAction::ACTION_SET_ORIGIN
    int64_t actionCallbackType;    // GUI::eAction::ACTION_CALLBACK
    int64_t eventScrollDown;       // GUI::Item::eItemEventType::EVENT_SCROLL_DOWN (mouse wheel over the item)
    int64_t eventScrollUp;         // GUI::Item::eItemEventType::EVENT_SCROLL_UP
    int64_t eventCursorDown;       // GUI::Item::eItemEventType::EVENT_CURSOR_DOWN
    int64_t eventDrag;             // GUI::Item::eItemEventType::EVENT_DRAG (cursor moved while pressed on the item)
    int64_t eventCursorHover;      // GUI::Item::eItemEventType::EVENT_CURSOR_HOVER (XML OnHover)
    int64_t eventCursorHoverEnd;   // GUI::Item::eItemEventType::EVENT_CURSOR_HOVER_END (XML OnHoverEnd)
    int64_t eventClick;            // GUI::Item::eItemEventType::EVENT_CLICK (XML OnClick)
    int64_t customizeBack;         // GUI::Events::eEventType::GUI_GAME_CUSTOMIZE_BACK (Esc, background click, BACK)
    int64_t customizeOpened;       // GUI::Events::eEventType::GUI_GAME_CUSTOMIZE_OPENED
    int64_t customizeClosed;       // GUI::Events::eEventType::GUI_GAME_CUSTOMIZE_CLOSED

    int64_t eqpFirearm;            // EquipmentType::eType::FIREARM
    int64_t eqpEmpty;              // EquipmentType::eType::EMPTY (the "nothing" entry of a slot)

    int64_t invCount;              // Inventory::eType::NUM_VALUES
    int64_t invUtility1;           // Inventory::eType::UTILITY_POUCH1 .. SUPPORT_GEAR3: bindings matched as substrings
    int64_t invSupport3;           //   ("UtilityPouch" items go in UtilityPouch1 and 2)
    int64_t invGameManaged[7];     // weapon scope/ammo/muzzle slots and HELMET_NVG: the game shows/hides those itself
};

extern const DK2ML_API* api;
extern Functions fn;
extern Globals globals;
extern Offsets off;

// Resolves everything above. Returns false (after logging what's missing) if the plugin can't work.
bool Resolve();

template <typename T>
T& Field(void* base, int32_t offset)
{
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

template <typename T>
const T& Field(const void* base, int32_t offset)
{
    return *reinterpret_cast<const T*>(static_cast<const char*>(base) + offset);
}

inline void* GameGUI() { return *globals.gameGUI; }
inline void* ObjectLibrary() { return *globals.objectLibrary; }
inline void* GuiRoot()
{
    void* manager = *globals.guiManager;
    return manager ? Field<void*>(manager, off.guiRoot) : nullptr;
}

} // namespace game
