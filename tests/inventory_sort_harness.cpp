// Compile the actual Item definition, sorting and button code. Only the engine
// state and rendering primitives are substituted by this portable harness.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <tuple>
#include <vector>

using namespace std;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using i64 = int64_t;
constexpr int CurItemGenVersion = 7, SocketMax_6 = 6, EnchantMax_5 = 5;
#include "inventory_layout.inc"
constexpr int InvRows = 7, InvCols = 10, IS_70_40_Inv_Count = 70;
constexpr int CELL_SIZE = 28, MAX_9000_ITEM_PICS = 9000;
constexpr int CM_1_NORMAL_HAND = 1, CM_12_ITEMS_PICS_START = 12;
constexpr int PCA_3_WALK_HORISONTAL = 3, MBP_1_LEFT = 1, C_0_White = 0;
struct Player
{
	Item InventorySlots[70], TalismanSlots[70], ItemOnCursor;
	Item OnBodySlots[20], BeltInventory[8];
	char InvUsedAlt[InvRows][InvCols], TalUsedAlt[InvRows][InvCols];
	int InvItemCount, TalItemCount, invCols, TotalGold, CurAction, CurLife;
} Players[1];
int CurrentPlayerIndex, CursorX, CursorY, MouseButtonPressed;
bool IsSigilInventoryOpen, IsGoldSplitPanelVisible;
struct { int GraphicsID, InventoryCellID; } Cur;
uchar ItemWidthTable[MAX_9000_ITEM_PICS], ItemHeightTable[MAX_9000_ITEM_PICS];
DisplayObject InventoryPanelRect;
vector<DisplayObject> DrawnBoxes;
vector<string> Tooltips;
void DrawBox(const DisplayObject& box, uchar) { DrawnBoxes.push_back(box); }
void DrawRect(const DisplayObject& box, uchar) { DrawnBoxes.push_back(box); }
void Tooltip_SetOffsetLeft() {}
void Tooltip_AddLine(const char* text, int) { Tooltips.push_back(text); }
const char* L(const char* text) { return text; }
#include "inventory_sort.inc"
struct PotionSortBase { int base; MAGIC_CODE magic; const char* name; };
const PotionSortBase PotionBases[] = {
#include "inventory_potion_bases.inc"
};

static Player& Reset(int columns = InvCols)
{
	memset(Players, 0, sizeof(Players));
	memset(ItemWidthTable, 0, sizeof(ItemWidthTable));
	memset(ItemHeightTable, 0, sizeof(ItemHeightTable));
	auto& player = Players[0];
	player.invCols = columns;
	player.CurLife = 640;
	player.ItemOnCursor.ItemCode = IC_M1_NONE;
	Cur = {CM_1_NORMAL_HAND, 20};
	IsSigilInventoryOpen = IsGoldSplitPanelVisible = false;
	MouseButtonPressed = 0;
	return player;
}

static Item& AddItem(int base, int width = 1, int height = 1)
{
	auto& player = Players[0];
	int& count = IsSigilInventoryOpen ? player.TalItemCount : player.InvItemCount;
	Item* slots = IsSigilInventoryOpen ? player.TalismanSlots : player.InventorySlots;
	assert(count < 70);
	int index = count++;
	auto& item = slots[index];
	item.ItemCode = IC_0_OTHER;
	item.baseItemIndex = base;
	item.GraphicValue = width * 8 + height;
	item.Identified = true;
	item.seed = 1000 + index;
	item.amount = index * 17;
	strcpy(item.Name, "test item");
	strcpy(item.FullMagicalItemName, "test item");
	ItemWidthTable[item.GraphicValue + 12] = width * CELL_SIZE;
	ItemHeightTable[item.GraphicValue + 12] = height * CELL_SIZE;
	return item;
}

static void AssertOnlyGridChanged(Player before)
{
	const auto& after = Players[0];
	if( IsSigilInventoryOpen ) memcpy(before.TalUsedAlt, after.TalUsedAlt, sizeof(before.TalUsedAlt));
	else memcpy(before.InvUsedAlt, after.InvUsedAlt, sizeof(before.InvUsedAlt));
	assert(memcmp(&before, &after, sizeof(before)) == 0);
}

static void AssertValidGrid()
{
	const auto& player = Players[0];
	const auto* grid = IsSigilInventoryOpen ? player.TalUsedAlt : player.InvUsedAlt;
	const Item* slots = IsSigilInventoryOpen ? player.TalismanSlots : player.InventorySlots;
	int count = IsSigilInventoryOpen ? player.TalItemCount : player.InvItemCount;
	int anchors[70] = {}, cells[70] = {};
	for( int r = 0; r < InvRows; ++r ) for( int c = 0; c < InvCols; ++c ){
		int number = grid[r][c];
		if( !number ) continue;
		assert(c < player.invCols && abs(number) <= count);
		int index = abs(number) - 1;
		++cells[index];
		if( number < 0 ) continue;
		++anchors[index];
		const auto& item = slots[index];
		int w = ItemWidthTable[item.GraphicValue + 12] / CELL_SIZE;
		int h = ItemHeightTable[item.GraphicValue + 12] / CELL_SIZE;
		assert(r - h + 1 >= 0 && c + w <= player.invCols);
		for( int y = r - h + 1; y <= r; ++y ) for( int x = c; x < c + w; ++x ){
			assert(grid[y][x] == (y == r && x == c ? number : -number));
		}
	}
	for( int i = 0; i < count; ++i ){
		assert(anchors[i] == 1);
		assert(cells[i] == ItemWidthTable[slots[i].GraphicValue + 12] / CELL_SIZE
			* (ItemHeightTable[slots[i].GraphicValue + 12] / CELL_SIZE));
	}
}

static void TestRowsAndGrouping()
{
	auto& player = Reset();
	for( int i = 0; i < 70; ++i ) AddItem((i * 11) % 7);
	player.TotalGold = 6652;
	player.InventorySlots[3].ItemCode = IC_11_GOLD;
	Player before = player;
	assert(SortInventory());
	AssertValidGrid();
	AssertOnlyGridChanged(before);
	int previousCode = -1, previousBase = -1;
	for( int row = 6; row >= 0; --row ) for( int col = 0; col < 10; ++col ){
		int number = player.InvUsedAlt[row][col];
		assert(number > 0);
		const auto& item = player.InventorySlots[number - 1];
		assert(item.ItemCode >= previousCode);
		if( item.ItemCode == previousCode ) assert(item.baseItemIndex >= previousBase);
		previousCode = item.ItemCode;
		previousBase = item.baseItemIndex;
	}
	Player sorted = player;
	assert(SortInventory());
	assert(memcmp(&sorted, &player, sizeof(player)) == 0);
	assert(Cur.InventoryCellID == -1);

	Reset(5);
	for( int i = 0; i < 12; ++i ) AddItem(i % 3);
	assert(SortInventory());
	AssertValidGrid();
	assert(player.InvUsedAlt[6][0] == 1 && player.InvUsedAlt[6][4] == 2);
	assert(player.InvUsedAlt[5][0] > 0 && player.InvUsedAlt[4][1] > 0);
	assert(player.InvUsedAlt[4][2] == 0);
}

static void TestVariantsAndSigils()
{
	auto& player = Reset();
	for( const char* name : {"zeta", "alpha", "zeta", "alpha"} ){
		auto& item = AddItem(1);
		strcpy(item.FullMagicalItemName, name);
	}
	assert(SortInventory());
	assert(player.InvUsedAlt[6][0] == 2 && player.InvUsedAlt[6][1] == 4);
	assert(player.InvUsedAlt[6][2] == 1 && player.InvUsedAlt[6][3] == 3);
	for( auto& item : player.InventorySlots ) item.Identified = false;
	assert(SortInventory());
	// Hidden magical names do not affect the ordering of unidentified items.
	assert(player.InvUsedAlt[6][0] == 1 && player.InvUsedAlt[6][1] == 2);

	IsSigilInventoryOpen = true;
	for( int i = 0; i < 20; ++i ) AddItem(2504 + (i % 5));
	Player before = player;
	assert(SortInventory());
	AssertValidGrid();
	AssertOnlyGridChanged(before);
	assert(player.TalUsedAlt[6][0] == 1 && player.TalUsedAlt[6][1] == 6);
}

static void TestRealPotionVariants()
{
	auto& player = Reset();
	assert(size(PotionBases) > 20 && size(PotionBases) <= 70);
	for( int index = int(size(PotionBases)) - 1; index >= 0; --index ){
		const auto& base = PotionBases[index];
		auto& item = AddItem(base.base);
		item.MagicCode = base.magic;
		strcpy(item.Name, base.name);
		strcpy(item.FullMagicalItemName, base.name);
	}
	Player before = player;
	assert(SortInventory());
	AssertValidGrid();
	AssertOnlyGridChanged(before);
	int previousMagic = -1;
	for( int cell = 0; cell < player.InvItemCount; ++cell ){
		int number = player.InvUsedAlt[6 - cell / 10][cell % 10];
		const auto& item = player.InventorySlots[number - 1];
		assert(item.MagicCode >= previousMagic);
		previousMagic = item.MagicCode;
	}
	cout << "Grouped real potion base variants: " << size(PotionBases) << '\n';
}

static void TestRectanglesAndCrowdedBags()
{
	auto& player = Reset();
	AddItem(1, 2, 3);
	AddItem(2, 2, 3);
	AddItem(3, 1, 4);
	for( int i = 0; i < 54; ++i ) AddItem(i % 4);
	Player before = player;
	assert(SortInventory());
	AssertOnlyGridChanged(before);
	AssertValidGrid();
	assert(player.InvUsedAlt[6][0] == 3);
	assert(player.InvUsedAlt[5][0] == -3 && player.InvUsedAlt[3][0] == -3);

	// Generated valid inventories cover mixed widths/heights and restricted bags.
	mt19937 random(20261103);
	int successes = 0;
	for( int trial = 0; trial < 400; ++trial ){
		Reset(trial % 2 ? 5 : 10);
		for( int step = 0; step < 180; ++step ){
			int width = 1 + random() % 2, height = 1 + random() % 4;
			int left = random() % player.invCols, top = random() % InvRows;
			if( left + width > player.invCols || top + height > InvRows ) continue;
			bool free = true;
			for( int r = top; r < top + height; ++r ) for( int c = left; c < left + width; ++c ){
				if( player.InvUsedAlt[r][c] ) free = false;
			}
			if( !free ) continue;
			AddItem(width * 10 + height, width, height);
			int number = player.InvItemCount;
			for( int r = top; r < top + height; ++r ) for( int c = left; c < left + width; ++c ){
				player.InvUsedAlt[r][c] = r == top + height - 1 && c == left ? number : -number;
			}
		}
		before = player;
		if( SortInventory() ){
			++successes;
			AssertValidGrid();
			AssertOnlyGridChanged(before);
			Player sorted = player;
			assert(SortInventory());
			assert(memcmp(&sorted, &player, sizeof(player)) == 0);
		}else{
			assert(memcmp(&before, &player, sizeof(player)) == 0);
		}
	}
	assert(successes >= 390);
	cout << "Sorted mixed-size bags: " << successes << "/400\n";
}

static void TestGuardsAndRollback()
{
	auto& player = Reset();
	AddItem(1);
	player.InvUsedAlt[0][4] = 1;
	Player before = player;
	Cur.GraphicsID = CM_12_ITEMS_PICS_START;
	assert(!SortInventory());
	assert(memcmp(&before, &player, sizeof(player)) == 0);
	Cur.GraphicsID = CM_1_NORMAL_HAND;
	player.ItemOnCursor.ItemCode = IC_11_GOLD;
	assert(!SortInventory());
	player.ItemOnCursor.ItemCode = IC_M1_NONE;
	player.CurAction = 8;
	assert(!SortInventory());
	player.CurAction = 0;
	player.CurLife = 0;
	assert(!SortInventory());
	player.CurLife = 640;
	IsGoldSplitPanelVisible = true;
	assert(!SortInventory());
	IsGoldSplitPanelVisible = false;
	for( int count : {-1, 71} ){
		player.InvItemCount = count;
		before = player;
		assert(!SortInventory());
		assert(memcmp(&before, &player, sizeof(player)) == 0);
	}
	player.InvItemCount = 1;
	player.InventorySlots[0].GraphicValue = 9000;
	assert(!SortInventory());
	player.InventorySlots[0].GraphicValue = 0;
	assert(!SortInventory());
	for( int columns : {0, 11} ){
		player.invCols = columns;
		assert(!SortInventory());
	}

	// A valid old layout survives exhausting the packing search budget.
	char grid[InvRows][InvCols] = {};
	vector<InventorySortEntry> entries = {{0, 2, 2}, {1, 2, 3}};
	int attempts = 2;
	assert(!PlaceSortedInventory(entries, 0, 5, grid, -1, attempts));
	for( const auto& row : grid ) for( char cell : row ) assert(cell == 0);

	Reset(5);
	for( int i = 0; i < 6; ++i ) AddItem(i, 2, 3);
	before = player;
	assert(!SortInventory()); // 36 cells do not fit in a 35-cell bag.
	assert(memcmp(&before, &player, sizeof(player)) == 0);

	Reset();
	assert(SortInventory());
	AssertValidGrid();
}

static void TestButton()
{
	Reset();
	for( const auto& panel : {DisplayObject{0, 0, 320, 480, 480, 320},
		DisplayObject{750, 200, 320, 480, 680, 1070}} ){
		InventoryPanelRect = panel;
		auto button = InventorySortButtonRect();
		assert(button.Right + 4 == panel.Right - 49);
		assert(button.Top == panel.Down - 36 && button.Down == panel.Down - 4);
		for( int state = 0; state < 4; ++state ){
			CursorX = button.Left;
			CursorY = button.Top;
			MouseButtonPressed = state == 1 ? MBP_1_LEFT : 0;
			Cur.GraphicsID = state == 2 ? CM_12_ITEMS_PICS_START : CM_1_NORMAL_HAND;
			if( state == 3 ) CursorX = button.Right;
			assert(CursorOnInventorySortButton() == (state != 3));
			DrawnBoxes.clear();
			Tooltips.clear();
			DrawInventorySortButton();
			assert(Tooltips.size() == (state == 3 ? 0 : 2));
			for( const auto& box : DrawnBoxes ){
				assert(box.Left >= button.Left && box.Top >= button.Top);
				assert(box.Left + box.Width <= button.Right && box.Top + box.Heigth <= button.Down);
			}
		}
		CursorX = button.Left - 1;
		assert(!CursorOnInventorySortButton());
		CursorX = button.Left;
		CursorY = button.Down;
		assert(!CursorOnInventorySortButton());
	}
}

int main()
{
	TestRowsAndGrouping();
	TestVariantsAndSigils();
	TestRealPotionVariants();
	TestRectanglesAndCrowdedBags();
	TestGuardsAndRollback();
	TestButton();
	cout << "Inventory sorting, item preservation, rollback and button checks passed\n";
}
