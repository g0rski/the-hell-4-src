// Production item definitions, save/recreation, payment and enchant handlers are
// included by the runner. Rendering, unrelated generation and engine IO are stubbed.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

using namespace std;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using i64 = int64_t;
using u64 = uint64_t;
using PLAYER_SPELL = int;
#define __fastcall
#define BIT(n) (1u << ((n) - 1))
#define IS(A, B) if constexpr (is_same_v<A, B>)
#define each(a) for(auto& element : (a)) element
constexpr int CurSaveVersion = 30, CurItemGenVersion = 7;
constexpr int SocketMax_6 = 6, EnchantMax_5 = 5;
#pragma pack(push, 1)
#include "potion_layout.inc"
#pragma pack(pop)
static_assert(sizeof(ItemInfo64) == 64);
constexpr int MaxItems_255 = 255, PlayersMax_4 = 4, count_BaseItems = 3000;
constexpr int IS_8_Belt_Count = 8, MaxGoldInCell = 5000, MaxAddSockets_4 = 4;
constexpr int ICF_SHARED_PROPERTIES = 0x80;
constexpr uint IPCM_ALL_CLASSES = 0xFFFFFFFFu;
constexpr int CM_1_NORMAL_HAND = 1, CM_12_ITEMS_PICS_START = 12;
constexpr int PD_0_None = 0, S_75_I_TITLEMOV = 75, S_56_I_INVGRAB = 56, S_1516_adria_enchant_sfx = 1516;
constexpr int GC_28_SUPERMUTANT = 28, GC_29_ENCLAVE_SOLDIER = 29;
constexpr int GUI_PanelWidth = 320, GUI_PanelHeight = 480, CELL_SIZE = 28;
constexpr int InvCols = 10, InvRows = 7, SO_2_RELIC = 2, SO_4_NONE = 4, PS_M1_NONE = -1;
constexpr int BLC_NONE = 0, DL_COUNT = 9, MB_TASKMODAL = 0, MB_ICONERROR = 0;
int SaveVersion = CurSaveVersion, CurrentPlayerIndex, CurrentDialogIndex;
int Screen_LeftBorder, Screen_TopBorder, LeftPanel, MaybeNeedDrawInfoPanel;
int RandomSeed = 123, LastSeed = 456, RandUsingCounter = 0, forceCallforSync;
int ChanceUniqueBoss[CurItemGenVersion + 1]{}, ChanceUniqueMob;
uint SharedItemRecreation;
bool IsChatHistoryVisible, IsINVPanelVisible;
struct { int main = 1, cooked = 2, overcells = 3, notavailable = 4, overslot = 5; } blColor;
struct DisplayObject { int Left, Top, Down, Width, Heigth; };
struct Player {
	Item ItemOnCursor;
	Item BeltInventory[8];
	Item InventorySlots[70];
	int InvItemCount, TotalGold, CharLevel;
	int invCols, SpellType, CurrentSpellIndex;
	uint AvailableRelictMask;
	char InvUsedAlt[InvRows][InvCols];
	uint gameChanger;
} Players[PlayersMax_4];
struct { int GraphicsID = CM_1_NORMAL_HAND; } Cur;
Item Items[MaxItems_255 + 1];
BaseItem BaseItems[count_BaseItems];
FlaskInfo FlasksInfo[1];
struct { int baseId; } UniqueItems[1];
char* VisualTradeBtnsCEL;
int Flip_InventorySound[6000]{}, ItemFlipTable[6000]{};
const DisplayObject* Hover;
int RollCalls, EnchantSoundCalls, DiscountPrice = 100, DropCalls, EquipmentUpdates;
bool InventoryFull;
vector<Item> ReturnedItems;
vector<string> RenderedLabels;

template<class T, class... V> bool not(T value, V... alternatives) { return ((value != alternatives) && ...); }
template<class T> void LimitToMax(T& value, int limit) { value = min<int>(value, limit); }
template<size_t N, class... V> void sprintf_s(char (&buffer)[N], const char* format, V... values)
{
	int length = snprintf(buffer, N, format, values...);
	assert(length >= 0 && length < int(N));
}
const char* L(const char* text) { return text; }
const char* LocalizedBaseItemName(const char* text) { return text; }
void DarkCampaignRenameSmith(char*, size_t) {}
void RingNameFixup(Item&) {}
void SetItemGrade(Item&) {}
void AdjustItemLook(Item&) {}
void Item_ApplyAffix(Item&, BaseEffect, int, int, int, int, int, int, bool) { assert(false); }
void SetRndSeed(int seed) { RandomSeed = seed; }
int RNG(int) { assert(false); return 0; }
void SetGraphicToGold(Item&) { assert(false); }
void GetItemAttrs(int, int, int) { assert(false); }
void GetUniqueItem(int, int) { assert(false); }
void RestoreTownersItems(int, int, int, int, int, int, int, int) { assert(false); }
template<class... V> void SetupAllItems(V...) { assert(false); }
template<class... V> void ApplyEnchants(V...) { assert(false); }
template<class... V> void ApplyQuench(V...) { assert(false); }
void ApplyGemEffects(Item&) { assert(false); }
void MessageBoxA(void*, const char*, const char*, int) { assert(false); }
void ChangeGraphValueInBelt(int, int) {}
void ChangeGoldGraphicOnQuantity(int, int) {}
void DispatchCursorAboveIsoland() {}
void UpdateRelictsState(Player&) {}
bool CursorIntoDisplayObject(const DisplayObject& rect) { return Hover == &rect; }
void PlayGlobalSound(int sound) { if(sound == S_1516_adria_enchant_sfx) ++EnchantSoundCalls; }
void SetCursorGraphics(int graphics) { Cur.GraphicsID = graphics; }
void FixItemCursor(int) {}
void RecalcPlayer(int, int) {}
int SwapItemsAndGetFirstItemGraphicsValue(Item* a, Item* b)
{
	swap(*a, *b);
	return b->GraphicValue + CM_12_ITEMS_PICS_START;
}
bool AutoPutCursorToInventoryOrDrop(bool)
{
	if(InventoryFull) return false;
	ReturnedItems.push_back(Players[CurrentPlayerIndex].ItemOnCursor);
	return true;
}
void InvPutItem(int player) { ++DropCalls; ReturnedItems.push_back(Players[player].ItemOnCursor); }
void InfoPanel_ClearBody() {}
void Tooltip_ShowItemInfo(int) {}
void Tooltip_SetOffsetTop() {}
void Tooltip_Clear() {}
void Tooltip_AddLine(const char*) {}
void Surface_DrawCEL(int, int, char*, int, int) {}
void CustomShadowItemSlot(int, int, int, int, int) {}
#include "potion_font.inc"
int GetTextWidth(const char* text)
{
	int width = 0;
	for(; *text; ++text) width += FontWidthSmall[FontIndexSmall_Latin[Codepage_Latin[uchar(*text)]]] + 1;
	return width;
}
void DrawText(int x, int, int end, const char* text, int)
{
	if(end > x) assert(GetTextWidth(text) <= end - x + 1);
	RenderedLabels.emplace_back(text);
}
void SetupPanelRect(DisplayObject&, int, int, int) {}
void SetupDisplayObject(DisplayObject& rect, int x, int y, int w, int h) { rect = {x, y, y + h, w, h}; }
int BuyPrice(int price) { return price * DiscountPrice / 100; }
int RollEnchantAffix(int) { ++RollCalls; return 42; }
void UpdateItem(Item&, bool = true);
void Enchant_DrawItem(const DisplayObject&, const Item&) {}
void Enchant_RetrieveAllItems();
void Enchant_Close();
int SlotToEnchant(Item&);
void ClearInventorySlot(int, int, bool = false);
#include "potion_items.inc"
void UpdateItem(Item& item, bool keepCharges)
{
	if(item.ItemCode == IC_1_SWORD){ ++EquipmentUpdates; return; }
	LastItemInfo saved;
	SaveItem(&saved, &item);
	LoadItem(&saved, &item, keepCharges);
}
#include "potion_enchanting.inc"

void Reset(int gold = 100)
{
	memset(Players, 0, sizeof(Players));
	for(auto& p : Players){
		p.ItemOnCursor.ItemCode = IC_M1_NONE;
		for(auto& item : p.BeltInventory) item.ItemCode = IC_M1_NONE;
		for(auto& item : p.InventorySlots) item.ItemCode = IC_M1_NONE;
	}
	Players[0].CharLevel = 1;
	Players[0].invCols = InvCols;
	Players[0].InventorySlots[0].ItemCode = IC_11_GOLD;
	Players[0].InventorySlots[0].amount = gold;
	Players[0].InvItemCount = 1;
	Players[0].TotalGold = gold;
	Players[0].InvUsedAlt[0][0] = 1;
	Cur.GraphicsID = CM_1_NORMAL_HAND;
	Enchant_Open();
	SetupEnchantPanel();
	Hover = nullptr;
	RollCalls = EnchantSoundCalls = DropCalls = EquipmentUpdates = 0;
	DiscountPrice = 100;
	InventoryFull = false;
	ReturnedItems.clear();
	RenderedLabels.clear();
}

void Place(int base, int origin = D_ADRIA)
{
	auto& cursor = Players[0].ItemOnCursor;
	SetPlayerHandItem(cursor, base);
	cursor.SetSeed(-123456789, 5);
	cursor.dropType = origin;
	Cur.GraphicsID = cursor.GraphicValue + CM_12_ITEMS_PICS_START;
	Hover = &EnchantSlot;
	Enchant_LButtonDown();
	Enchant_MouseClick();
	assert(cursor.ItemCode == IC_M1_NONE && EnchantItem.baseItemIndex == base);
}

void Click(int row)
{
	Hover = row < 3 && PotionConversionType(EnchantItem) != -1 ? &PotionConversionButtons[row] : &EnchantAffixButtons[row];
	Enchant_LButtonDown();
	Enchant_MouseClick();
}

Item& LastResult()
{
    auto& player = Players[0];
    for(int i = 0; i < player.InvItemCount; ++i){
        if(PotionConversionType(player.InventorySlots[i]) != -1) return player.InventorySlots[i];
    }
    assert(false);
    return player.ItemOnCursor;
}

void AddPotion(int base, bool belt = false)
{
    Item item;
    SetPlayerHandItem(item, base);
    item.SetSeed(base * 100 + Players[0].InvItemCount, 7);
    if(belt){
        for(auto& slot : Players[0].BeltInventory) if(slot.ItemCode == IC_M1_NONE){ slot = item; return; }
        assert(false);
    }else{
        auto& player = Players[0];
        player.ItemOnCursor = item;
        int cell = PotionConversionOutputCell();
        assert(cell != -1 && AutoPutCursorToCell(player, cell, 1, 1, true));
        player.ItemOnCursor.ItemCode = IC_M1_NONE;
    }
}

int CountPotions(int type)
{
    int count = PotionConversionType(EnchantItem) == type;
    for(int i = 0; i < Players[0].InvItemCount; ++i) count += PotionConversionType(Players[0].InventorySlots[i]) == type;
    for(const auto& item : Players[0].BeltInventory) count += PotionConversionType(item) == type;
    return count;
}

int main()
{
#include "potion_bases.inc"
    const int bases[] = {24, 25, 29, 30};
    const int expectedTargets[4][3] = {{1, 2, 3}, {0, 2, 3}, {3, -1, -1}, {2, -1, -1}};
    int conversions = 0;
    for(int source = 0; source < 4; ++source) for(int row = 0; row < 9; ++row){
        Reset();
        Place(bases[source]);
        int target = row < 3 ? expectedTargets[source][row] : -1;
        if(target == -1){
            assert(EnchantButtonPicNumber(true, row) == ENCH_INACTIVE);
            Click(row);
            assert(EnchantItem.baseItemIndex == bases[source] && CalcTotalGold(0) == 100);
            continue;
        }
        ++conversions;
        int price = source < 2 && target >= 2 ? 30 : 9;
        assert(EnchantButtonPicNumber(true, row) != ENCH_INACTIVE);
        Click(row);
        assert(EnchantItem.ItemCode == IC_M1_NONE);
        Item& result = LastResult();
        assert(result.baseItemIndex == bases[target]);
        assert(result.MagicCode == BaseItems[bases[target]].MagicCode);
        assert(result.GraphicValue == BaseItems[bases[target]].GraphicValue);
        assert(strcmp(result.Name, BaseItems[bases[target]].NamePtr) == 0);
        assert(result.dropType == D_0_SIMPLE && result.genVersion == 5);
        assert(result.IsReqMet && result.Identified);
        assert(CalcTotalGold(0) == uint(100 - price) && Players[0].TotalGold == 100 - price);
        assert(RollCalls == 0 && EnchantSoundCalls == 1);
        for(auto affix : result.enchantIndexes) assert(affix == -1);
        Enchant_MouseClick();
        assert(EnchantSoundCalls == 1 && CalcTotalGold(0) == uint(100 - price));
        UpdateItem(result); // Real compact SaveItem + LoadItem + RecreateItem.
        assert(result.baseItemIndex == bases[target]);
        assert(result.MagicCode == BaseItems[bases[target]].MagicCode);
        assert(result.seed == -123456789 && result.genVersion == 5);
    }
    assert(conversions == 8);
    cout << "PASS all 8 prices; forbidden conversions absent; real compact save/reconstruction\n";

    for(int gold : {0, 8, 9, 29, 30}) for(int row : {0, 1}){
        Reset(gold);
        Place(24);
        int price = row == 0 ? 9 : 30;
        Click(row);
        assert(CalcTotalGold(0) == uint(gold < price ? gold : gold - price));
        if(gold < price) assert(EnchantItem.ItemCode != IC_M1_NONE && EnchantItem.baseItemIndex == 24);
        else assert(EnchantItem.ItemCode == IC_M1_NONE && LastResult().baseItemIndex == (row == 0 ? 25 : 29));
    }
    Reset();
    Place(24);
    Hover = &PotionConversionButtons[1];
    Enchant_LButtonDown();
    Players[0].InventorySlots[0].amount = 29;
    Enchant_MouseClick();
    assert(CalcTotalGold(0) == 29 && EnchantItem.baseItemIndex == 24);
    Reset(0);
    Place(24);
    Players[0].BeltInventory[0].ItemCode = IC_11_GOLD;
    Players[0].BeltInventory[0].amount = 12;
    Players[0].InventorySlots[0].amount = 18;
    Click(1);
    assert(CalcTotalGold(0) == 0 && LastResult().baseItemIndex == 29);
    Reset();
    Place(24);
    DiscountPrice = 300;
    Click(0);
    assert(CalcTotalGold(0) == 91);
    cout << "PASS insufficient/exact gold, real payment across belt/inventory, rechecked balance, fixed fees\n";

    Reset();
    Place(24);
    Hover = &PotionConversionButtons[0];
    Enchant_MouseClick();
    assert(EnchantItem.baseItemIndex == 24 && CalcTotalGold(0) == 100);
    Enchant_LButtonDown();
    Hover = &PotionConversionButtons[1];
    Enchant_MouseClick();
    assert(EnchantItem.baseItemIndex == 24 && CalcTotalGold(0) == 100);
    Players[0].ItemOnCursor.ItemCode = IC_1_SWORD;
    Cur.GraphicsID = CM_12_ITEMS_PICS_START;
    Click(0);
    assert(CalcTotalGold(0) == 100);
    cout << "PASS drag release, changed button, duplicate mouse-up and occupied cursor protections\n";

    for(int variant : {24, 25, 29, 30, 77, 78, 79, 80, 235, 237, 397, 2652, 2653, 2654, 2655}){
        Reset();
        Place(variant, D_PEPIN);
        Click(0);
        assert(LastResult().baseItemIndex != variant && EnchantSoundCalls == 1);
        UpdateItem(LastResult());
        assert(PotionConversionType(LastResult()) != -1);
    }
    Reset();
    Item rejected{};
    rejected.ItemCode = IC_0_OTHER;
    for(auto code : {MC_18_POTION_OF_REJUVENATION, MC_19_POTION_OF_FULL_REJUVENATION, MC_24_BOOKS}){
        rejected.MagicCode = code;
        assert(!AllowEnchant(rejected));
    }
    cout << "PASS alternate/shop/classic inputs; rejuvenation and unrelated consumables rejected\n";

    Reset();
    Place(24);
    AddPotion(235);
    AddPotion(25); // Another source type must stay untouched.
    AddPotion(24, true);
    assert(CountPotions(0) == 3 && CountPotions(1) == 1);
    Click(0);
    assert(EnchantItem.baseItemIndex == 235); // Backpack before belt.
    assert(CountPotions(0) == 2 && CountPotions(1) == 2);
    Click(0);
    assert(EnchantItem.baseItemIndex == 24 && Players[0].BeltInventory[0].ItemCode == IC_M1_NONE);
    assert(CountPotions(0) == 1 && CountPotions(1) == 3);
    Click(0);
    assert(EnchantItem.ItemCode == IC_M1_NONE && CountPotions(0) == 0 && CountPotions(1) == 4);
    assert(CalcTotalGold(0) == 73 && EnchantSoundCalls == 3);
    for(int i = 0; i < Players[0].InvItemCount; ++i){
        Item& item = Players[0].InventorySlots[i];
        if(PotionConversionType(item) != -1){ UpdateItem(item); assert(PotionConversionType(item) == 1); }
    }
    cout << "PASS consecutive conversion, backpack then belt refill, one item/fee per click, final empty slot\n";

    Reset(9);
    Place(24);
    AddPotion(235);
    Click(0); // Paying removes the gold slot and renumbers the remaining inventory.
    assert(CalcTotalGold(0) == 0 && EnchantItem.baseItemIndex == 235);
    assert(CountPotions(0) == 1 && CountPotions(1) == 1);
    Click(0);
    assert(EnchantItem.baseItemIndex == 235 && EnchantSoundCalls == 1);
    Reset();
    Place(24);
    memset(Players[0].InvUsedAlt, 1, sizeof(Players[0].InvUsedAlt));
    Click(0);
    assert(EnchantItem.baseItemIndex == 24 && CalcTotalGold(0) == 100 && EnchantSoundCalls == 0);
    Reset();
    Place(24);
    Hover = &PotionConversionButtons[0];
    Enchant_LButtonDown();
    memset(Players[0].InvUsedAlt, 1, sizeof(Players[0].InvUsedAlt));
    Enchant_MouseClick();
    assert(EnchantItem.baseItemIndex == 24 && CalcTotalGold(0) == 100);
    cout << "PASS inventory renumbering after payment; full backpack and space changes reject without mutation\n";

    for(bool full : {false, true}){
        Reset();
        Place(24);
        AddPotion(235, true);
        Click(0);
        InventoryFull = full;
        Players[0].ItemOnCursor.ItemCode = IC_1_SWORD;
        Cur.GraphicsID = CM_12_ITEMS_PICS_START;
        Enchant_Close();
        assert(ReturnedItems.size() == 1 && ReturnedItems[0].baseItemIndex == 235);
        assert(DropCalls == int(full) && EnchantItem.ItemCode == IC_M1_NONE);
        assert(Players[0].ItemOnCursor.ItemCode == IC_1_SWORD && !IsEnchantPanelVisible);
        assert(LastResult().baseItemIndex == 25);
    }
    Reset();
    Place(24);
    AddPotion(235, true);
    Click(0);
    Hover = &EnchantSlot;
    Enchant_LButtonDown();
    assert(Players[0].ItemOnCursor.baseItemIndex == 235 && EnchantItem.ItemCode == IC_M1_NONE);
    cout << "PASS manual retrieval and close/overflow return pending input once while preserving results/cursor\n";

    Reset(10000);
    EnchantItem = Item{};
    EnchantItem.ItemCode = IC_1_SWORD;
    EnchantItem.MagicLevel = ML_0_USUAL;
    EnchantItem.dropType = D_0_SIMPLE;
    EnchantItem.enchantIndexes.fill(-1);
    assert(AllowEnchant(EnchantItem));
    assert(EnchantButtonPicNumber(true, 0) != ENCH_INACTIVE);
    assert(EnchantButtonPicNumber(true, 1) == ENCH_INACTIVE);
    Click(0);
    assert(RollCalls == 1 && EquipmentUpdates == 1 && EnchantSoundCalls == 1);
    assert(CalcTotalGold(0) == 9850 && (EnchantItem.dropType & D_ENCHANT));
    assert(SlotToEnchant(EnchantItem) == 1 && AllowEnchant(EnchantItem));
    EnchantItem.enchantIndexes.fill(42);
    assert(!AllowEnchant(EnchantItem) && EnchantButtonPicNumber(true, 0) == ENCH_INACTIVE);
    EnchantItem = Item{};
    EnchantItem.ItemCode = IC_12_RING;
    assert(!AllowEnchant(EnchantItem));
    cout << "PASS existing equipment click, affix invocation, enchant payment/capacity and character level gates\n";

    Reset();
    Place(24);
    Enchant_DrawEnchantButtons();
    assert(RenderedLabels.size() == 3);
    assert(RenderedLabels[0] == "Potion of Mana, 9 gold");
    assert(RenderedLabels[1] == "Potion of Full Healing, 30 gold");
    assert(RenderedLabels[2] == "Potion of Full Mana, 30 gold");
    for(int i = 0; i < 3; ++i){
        assert(PotionConversionButtons[i].Heigth == 20);
        if(i) assert(PotionConversionButtons[i].Top == PotionConversionButtons[i - 1].Down + 1);
    }
    Reset();
    Place(29);
    Enchant_DrawEnchantButtons();
    assert(RenderedLabels.size() == 1 && RenderedLabels[0] == "Potion of Full Mana, 9 gold");
    assert(PotionConversionButtons[2].Down < EnchantLeaveButton.Top);
    cout << "PASS single-button options fit actual Latin font widths, use one row each and remain above Exit\nAll potion conversion checks passed.\n";
}
