// The runner includes the actual protocol, packet layouts, manual-drop entry
// point, and item deletion code. Only the surrounding engine/transport is stubbed.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using namespace std;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using u64 = uint64_t;
using i64 = int64_t;
using DWORD = uint32_t;
using WORD = uint16_t;
using DUNGEON = char;
#include "shared_items_limits.inc"
enum QUENCH_AFFIX : uchar { QA_NO };
constexpr int MaxItems_255 = 255, PlayersMax_4 = 4, FineMap_112 = 112;
constexpr int SocketMax_6 = 6, EnchantMax_5 = 5, count_BaseItems = 100;
constexpr int CM_1_NORMAL_HAND = 1, CM_12_ITEMS_PICS_START = 12;
constexpr int BUFFER_OFF = 0, BUFFER_ON = 1, BUFFER_PROCESS = 2;
static_assert(CMD_110_SHARED_DROP == 110 && CMD_114_SHARED_DATA < CMD_DLEVEL_START);
static_assert(CMD_DLEVEL_START + DungeonCount <= 255);
constexpr int ICF_SHARED_PROPERTIES = 0x80;
constexpr int OriginalSpell = 37, OriginalSockets = 3;
constexpr i64 operator""_ms(unsigned long long value) { return value; }
#define __fastcall
#define memzero(value) memset(&(value), 0, sizeof(value))

struct Item;
struct ItemGet;
#pragma pack(push, 1)
#include "item_info.inc"
#pragma pack(pop)
static_assert(sizeof(LastItemInfo) == 64);

#pragma pack(push, 1)
struct Item
{
	short MapRow{}, MapCol{};
	bool gottenFromLand{};
	char* ItemCELFilePtr{};
	LastItemInfo info{};
	int seed{}, seed64_reserved{};
	uchar charFlag{}, SpellIndex{}, socketsBase{};
	int baseItemIndex = 7, genVersion = 6;
	array<uchar, 512 - 2 * sizeof(short) - sizeof(bool) - sizeof(char*) - sizeof(LastItemInfo) - 4 * sizeof(int) - 3> opaqueStats{};
	i64 SavedSeed() const;
};
#include "shared_items_packets.inc"
#pragma pack(pop)
static_assert(sizeof(Item) == 512);
static_assert(sizeof(TCmdSharedItem) == 80);
static_assert(sizeof(TCmdSharedRemove) == 9);
static_assert(sizeof(TCmdSharedData) == 90);
void ItemInfo64::operator=(const Item& item)
{
	*this = item.info;
	seed = item.SavedSeed();
	charFlag = item.charFlag;
}
struct Player { bool IsExists{}, lvlChanging{}; DUNGEON dungeon{}; int fullClassId{}; };
struct Cursor { int GraphicsID = CM_1_NORMAL_HAND; };
Player Players[PlayersMax_4];
Cursor Cur;
Item Items[MaxItems_255 + 1];
uchar ItemActive[MaxItems_255], ItemAvailable[MaxItems_255], ItemMap[FineMap_112][FineMap_112];
int ItemsCount, CurrentPlayerIndex, MaxCountOfPlayersInGame = 4;
DUNGEON Dungeon = 1;
int gbBufferMsgs, RandomSeed = 123, LastSeed = 456;
u64 RandomSeed64 = 789, LastSeed64 = 1011;
uint RandUsingCounter = 12, Rand64UsingCounter = 13;
bool NetItem = false;
int PlayerRowPos = 40, PlayerColPos = 40;
i64 Now = 1000;
i64 GetTickCountTh() { return Now; }

extern u64 SharedGroundItems[MaxItems_255];
void ShareDroppedItem(int);
void PreserveSharedItemProperties(Item&);
void DeleteItem(int, int);
void NetSendHiPri(uchar*, uchar);
void NetSendDirect(uchar*, uchar);
void NetSendMask(uint, uchar*, uchar);
void mega_add_data(int, const void*, uint);
void check_update_plr(int) {}
void RespawnItem(int index, bool)
{
	Items[index].gottenFromLand = false;
	Items[index].ItemCELFilePtr = reinterpret_cast<char*>(0x56780000 + CurrentPlayerIndex);
}
void SetCursorGraphics(int graphics) { Cur.GraphicsID = graphics; }
void NetSendCmdPItem(int, int, int, int) { assert(false); }

bool GetItemSpace(int row, int col, int& index)
{
	if( ItemsCount == MaxItems_255 ) return false;
	for( int offset = 0; offset < FineMap_112; ++offset ){
		int candidate = (col + offset) % FineMap_112;
		if( ItemMap[row][candidate] ) continue;
		index = ItemAvailable[0];
		Items[index] = Item{};
		Items[index].MapRow = row;
		Items[index].MapCol = candidate;
		SharedGroundItems[index] = 0;
		ItemMap[row][candidate] = index + 1;
		ItemAvailable[0] = ItemAvailable[MaxItems_255 - 1 - ItemsCount];
		ItemActive[ItemsCount++] = index;
		return true;
	}
	return false;
}

vector<LastItemInfo> Inventory[PlayersMax_4];
vector<Item> ReceivedItems[PlayersMax_4];
int InventorySpace[PlayersMax_4], GoldRoom[PlayersMax_4], Gold[PlayersMax_4];
LastItemInfo HeldItem;
bool DropFails;
int InvPutItem(int, int row, int col)
{
	int index = -1;
	if( DropFails || !GetItemSpace(row, col, index) ) return -1;
	Items[index].info = HeldItem;
	Items[index].seed = int(HeldItem.seed);
	Items[index].charFlag = HeldItem.charFlag;
	Items[index].SpellIndex = OriginalSpell;
	Items[index].socketsBase = OriginalSockets;
	Items[index].ItemCELFilePtr = reinterpret_cast<char*>(0x12340000 + CurrentPlayerIndex);
	for( uint i = 0; i < Items[index].opaqueStats.size(); ++i ) Items[index].opaqueStats[i] = (i * 17 + 11) & 255;
	return index;
}

void RemoveGround(int index)
{
	ItemMap[Items[index].MapRow][Items[index].MapCol] = 0;
	for( int i = 0; i < ItemsCount; ++i ){
		if( ItemActive[i] == index ) { DeleteItem(index, i); return; }
	}
	assert(false);
}

void InvGetItem(int player, int index)
{
	LastItemInfo info{};
	info = Items[index];
	Inventory[player].push_back(info);
	ReceivedItems[player].push_back(Items[index]);
	RemoveGround(index);
	Cur.GraphicsID = CM_12_ITEMS_PICS_START;
}

void AutoGetItem(int player, int index)
{
	if( Items[index].info.baseItemIndex == 0 ){
		int available = Items[index].info.amount;
		int amount = min(GoldRoom[player], available);
		Gold[player] += amount;
		GoldRoom[player] -= amount;
		Items[index].info.amount -= amount;
		if( !Items[index].info.amount ) { RemoveGround(index); return; }
	}else if( InventorySpace[player] ){
		--InventorySpace[player];
		LastItemInfo info{};
		info = Items[index];
		Inventory[player].push_back(info);
		ReceivedItems[player].push_back(Items[index]);
		RemoveGround(index);
		return;
	}
	RespawnItem(index, true);
}

// Minimal spell catalog for the actual class-dependent book/staff selection code.
static uint SharedItemRecreation = 0;
constexpr int PS_COUNT = 80, PS_1_FIREBOLT = 1, PS_2_HEALING = 2, PS_5_IDENTIFY = 5;
constexpr int PS_7_TOWN_PORTAL = 7, PS_9_INFRAVISION = 9, PS_13_HYDRA = 13, PS_14_BALL_LIGHTNING = 14;
constexpr int PS_8_STONE_CURSE = 8;
constexpr int PS_15_FORCE_WAVE = 15, PS_16_REFLECT = 16, PS_17_LIGHTING_RING = 17, PS_18_LIGHTNING_NOVA = 18;
constexpr int PS_19_FLAME_RING = 19, PS_21_GOLEM = 21, PS_22_FURY = 22, PS_24_APOCALYPSE = 24;
constexpr int PS_28_TRAP_DISARM = 28, PS_31_HOLY_BOLT = 31, PS_32_BONE_SPIRIT_REAL_RESSURECT = 32;
constexpr int PS_33_TELEKINES = 33, PS_34_HEAL_OTHER = 34, PS_37_MANA_RECHARGE = 37, PS_38_MAGI = 38;
constexpr int PS_39_HOLY_NOVA = 39, PS_41_FIERY_NOVA = 41, PS_42_WARP = 42, PS_43_ARCANE_NOVA = 43;
enum { GM_CLASSIC, GM_COLISEUM, GM_IRONMAN, GM_SURVIVAL, GM_NIGHTMARE, GM_SPEEDRUN, GM_NORMAL, GM_OUTBREAK };
constexpr int PFC_WARLOCK = 2;
int GameMode = GM_NORMAL, SpellRandom = 17;
struct BaseItem {} BaseItems[count_BaseItems];
struct Spell { int ItemQuality = 1; uint RequiredClassMask = 3; } Spells[PS_COUNT];
template<class T, class... U> bool is(T value, U... values) { return ((value == values) || ...); }
template<class T, class... U> bool not(T value, U... values) { return ((value != values) && ...); }
template<class T> void LimitToMin(T& value, T minimum) { value = max(value, minimum); }
int RNG(int range) { ++RandUsingCounter; return SpellRandom % range; }
bool BookSpellSkip(int, int) { return false; }
bool IsSpellBannedInDarkCampaign(int) { return false; }
bool BookSpellAvailable(int spell, int player, int) { return spell % 2 == Players[player].fullClassId; }

#include "shared_items_under_test.inc"

struct Message
{
	int sender;
	uint mask;
	vector<uchar> bytes;
	int Command() const { return bytes[0]; }
};
deque<Message> Wire;
vector<Message> History;
int ClientCount;

void NetSendMask(uint mask, uchar* bytes, uchar size)
{
	Message message{CurrentPlayerIndex, mask, vector<uchar>(bytes, bytes + size)};
	Wire.push_back(message);
	History.push_back(message);
}
void NetSendHiPri(uchar* bytes, uchar size) { NetSendMask((1u << ClientCount) - 1, bytes, size); }
void NetSendDirect(uchar* bytes, uchar size) { NetSendMask((1u << ClientCount) - 1, bytes, size); }

struct Client
{
	map<u64, TCmdSharedItem> shared;
	set<u64> taken;
	set<u64> applied, requested;
	map<u64, SharedItemPayload> payloads;
	uint snapshotRecipients = 0;
	map<u64, PendingSharedPickup> pending;
	array<Item, MaxItems_255 + 1> items{};
	array<uchar, MaxItems_255> active{}, available{};
	array<u64, MaxItems_255> ids{};
	uchar tiles[FineMap_112][FineMap_112]{};
	int count{}, bufferMode = BUFFER_OFF;
	DUNGEON level = 1;
	Cursor cursor;
	vector<Message> buffered;
};
Client Clients[PlayersMax_4];

void Save()
{
	auto& client = Clients[CurrentPlayerIndex];
	client.shared = SharedItems;
	client.taken = TakenSharedItems;
	client.applied = AppliedSharedPickups;
	client.requested = RequestedSharedItems;
	client.payloads = SharedItemPayloads;
	client.snapshotRecipients = SharedSnapshotRecipients;
	client.pending = PendingSharedPickups;
	copy(begin(Items), end(Items), client.items.begin());
	copy(begin(ItemActive), end(ItemActive), client.active.begin());
	copy(begin(ItemAvailable), end(ItemAvailable), client.available.begin());
	copy(begin(SharedGroundItems), end(SharedGroundItems), client.ids.begin());
	memcpy(client.tiles, ItemMap, sizeof(ItemMap));
	client.count = ItemsCount;
	client.level = Dungeon;
	client.cursor = Cur;
	client.bufferMode = gbBufferMsgs;
}

template<class Action> void At(int player, Action action)
{
	auto& client = Clients[player];
	CurrentPlayerIndex = player;
	SharedItems = client.shared;
	TakenSharedItems = client.taken;
	AppliedSharedPickups = client.applied;
	RequestedSharedItems = client.requested;
	SharedItemPayloads = client.payloads;
	SharedSnapshotRecipients = client.snapshotRecipients;
	PendingSharedPickups = client.pending;
	copy(client.items.begin(), client.items.end(), begin(Items));
	copy(client.active.begin(), client.active.end(), begin(ItemActive));
	copy(client.available.begin(), client.available.end(), begin(ItemAvailable));
	copy(client.ids.begin(), client.ids.end(), begin(SharedGroundItems));
	memcpy(ItemMap, client.tiles, sizeof(ItemMap));
	ItemsCount = client.count;
	Dungeon = client.level;
	Cur = client.cursor;
	gbBufferMsgs = client.bufferMode;
	action();
	Save();
}

void mega_add_data(int sender, const void* bytes, uint size)
{
	auto data = static_cast<const uchar*>(bytes);
	Clients[CurrentPlayerIndex].buffered.push_back({sender, 1u << CurrentPlayerIndex, vector<uchar>(data, data + size)});
}

void Deliver(const Message& message, int player)
{
	At(player, [&]{
		if( message.Command() == CMD_114_SHARED_DATA ){
			TCmdSharedData packet;
			memcpy(&packet, message.bytes.data(), sizeof(packet));
			On_SHARED_DATA(&packet, message.sender);
		}else if( message.Command() == CMD_113_SHARED_REMOVE ){
			TCmdSharedRemove packet;
			memcpy(&packet, message.bytes.data(), sizeof(packet));
			On_SHARED_REMOVE(&packet, message.sender);
		}else{
			TCmdSharedItem packet;
			memcpy(&packet, message.bytes.data(), sizeof(packet));
			switch( message.Command() ){
			case CMD_110_SHARED_DROP: On_SHARED_DROP(&packet, message.sender); break;
			case CMD_111_SHARED_REQUEST: On_SHARED_REQUEST(&packet, message.sender); break;
			case CMD_112_SHARED_TAKE: On_SHARED_TAKE(&packet, message.sender); break;
			default: assert(false);
			}
		}
	});
}

void Pump()
{
	int iterations = 0;
	while( !Wire.empty() ){
		assert(++iterations < 2000);
		auto message = Wire.front();
		Wire.pop_front();
		for( int i = 0; i < ClientCount; ++i ) if( message.mask & (1u << i) ) Deliver(message, i);
	}
}

LastItemInfo RichItem(int base = 7, int amount = 1)
{
	LastItemInfo info{};
	info.genVersion = 6;
	info.saveVersion = 26;
	info.seed = 0x12345678;
	info.dropType = 0x654321;
	info.baseItemIndex = base;
	info.amount = amount;
	info.magicLevelAndIdentified = 9;
	info.quality = 49;
	info.magicFind = 77;
	info.uniqIndex = 100;
	info.curDurability = 21;
	info.baseDurability = 80;
	info.curCharges = 11;
	info.baseCharges = 90;
	info.socketsAdded = 2;
	info.sockets = {1, 2, 3, 4, 5, 6};
	info.enchantIndexes = {7, 8, 9, 10, 11};
	info.quenchLevel = 10;
	info.difficulty = 3;
	info.charFlag = 65;
	return info;
}

bool Same(const LastItemInfo& a, const LastItemInfo& b) { return memcmp(&a, &b, sizeof(a)) == 0; }

LastItemInfo SharedInfo(LastItemInfo info)
{
	info.charFlag |= ICF_SHARED_PROPERTIES;
	info.seed = i64(u64(uint(info.seed)) | (u64(OriginalSpell | (OriginalSockets << 8)) << 32));
	return info;
}

void Reset(int count = 3)
{
	ClientCount = count;
	MaxCountOfPlayersInGame = count;
	Wire.clear();
	History.clear();
	for( int i = 0; i < PlayersMax_4; ++i ){
		Clients[i] = Client{};
		for( int j = 0; j < MaxItems_255; ++j ) Clients[i].available[j] = j;
		Players[i] = Player{i < count, false, 1};
		Inventory[i].clear();
		ReceivedItems[i].clear();
		InventorySpace[i] = 10;
		GoldRoom[i] = 1000;
		Gold[i] = 0;
	}
	HeldItem = RichItem();
	DropFails = false;
}

u64 Drop(int player, LastItemInfo info = RichItem())
{
	u64 id = 0;
	At(player, [&]{
		HeldItem = info;
		assert(DropCursorItemBeforeTrig(true));
		assert(SharedItems.size() == 1);
		id = SharedItems.begin()->first;
	});
	return id;
}

void Request(int player, u64 id, bool autoGet = true)
{
	At(player, [&]{
		int index = FindSharedGroundItem(id);
		assert(index != -1);
		RequestSharedItem(index, autoGet);
	});
}

void ClearLocalGround()
{
	ItemsCount = 0;
	memzero(ItemMap);
	memzero(SharedGroundItems);
	for( int i = 0; i < MaxItems_255; ++i ) ItemAvailable[i] = i;
}

void TestPrivateLootAndRace()
{
	Reset();
	At(1, []{
		int index;
		assert(GetItemSpace(40, 40, index));
		Items[index].info = RichItem(); // identical private seed and occupied drop tile
	});
	u64 id = Drop(0);
	auto initialDrop = Wire.front();
	Pump();
	assert(Clients[1].count == 2 && !Clients[1].ids[0]);
	assert(Same(Clients[1].items[0].info, RichItem()));
	assert(RandomSeed == 123 && LastSeed == 456 && RandUsingCounter == 12);
	assert(RandomSeed64 == 789 && LastSeed64 == 1011 && Rand64UsingCounter == 13);
	At(0, []{ Dungeon = 2; Players[0].dungeon = 2; ClearLocalGround(); });
	Request(1, id);
	Request(2, id, false);
	Pump();
	assert(Inventory[1].size() == 1 && Same(Inventory[1][0], SharedInfo(RichItem())));
	for( uint i = 0; i < ReceivedItems[1][0].opaqueStats.size(); ++i ){
		assert(ReceivedItems[1][0].opaqueStats[i] == ((i * 17 + 11) & 255));
	}
	assert(ReceivedItems[1][0].ItemCELFilePtr == reinterpret_cast<char*>(0x56780001));
	assert(Inventory[0].empty() && Inventory[2].empty());
	assert(Clients[1].count == 1 && !Clients[1].ids[0]);
	for( int i = 0; i < 3; ++i ) assert(Clients[i].shared.empty());
	auto grant = *find_if(History.begin(), History.end(), [](const Message& m){ return m.Command() == CMD_112_SHARED_TAKE; });
	for( int i = 0; i < 3; ++i ) { Deliver(grant, i); Deliver(initialDrop, i); }
	assert(Inventory[1].size() == 1);
	for( int i = 0; i < 3; ++i ) assert(Clients[i].shared.empty());
	cout << "PASS private loot, divergent indices, remote authority, simultaneous pickups, replay\n";
}

void TestFullInventoryAndPartialGold()
{
	Reset(2);
	u64 id = Drop(0);
	Pump();
	InventorySpace[1] = 0;
	Request(1, id);
	Pump();
	assert(Inventory[1].empty());
	assert(Clients[0].shared.size() == 1 && Clients[1].shared.size() == 1);
	u64 replacement = Clients[1].shared.begin()->first;
	assert(replacement != id && Same(Clients[1].shared.begin()->second.info, SharedInfo(RichItem())));
	Request(0, replacement, false);
	Pump();
	assert(Inventory[0].size() == 1 && Clients[1].shared.empty());

	Reset(2);
	id = Drop(0, RichItem(0, 200));
	Pump();
	InventorySpace[1] = 0;
	GoldRoom[1] = 75;
	Request(1, id);
	Pump();
	assert(Gold[1] == 75);
	for( int i = 0; i < 2; ++i ){
		assert(Clients[i].shared.size() == 1);
		assert(Clients[i].shared.begin()->second.info.amount == 125);
	}
	GoldRoom[1] = 1000;
	Request(1, Clients[1].shared.begin()->first);
	Pump();
	assert(Gold[1] == 200 && Clients[0].shared.empty() && Clients[1].shared.empty());
	cout << "PASS full inventory, cursor pickup, partial gold conservation\n";
}

void TestLevelChangeAndBusyCursor()
{
	Reset(2);
	u64 id = Drop(0);
	Pump();
	Request(1, id);
	Client cached = Clients[1];
	At(1, []{ Dungeon = 2; Players[1].dungeon = 2; ClearLocalGround(); });
	// Request was sent on level 1; let authority grant before it learns of the warp.
	Players[1].dungeon = 1;
	Pump();
	assert(Inventory[1].empty() && Clients[0].shared.size() == 1);
	assert(Clients[0].shared.begin()->second.dungeon == 1);
	At(1, [&]{
		Dungeon = 1;
		copy(cached.items.begin(), cached.items.end(), begin(Items));
		copy(cached.active.begin(), cached.active.end(), begin(ItemActive));
		copy(cached.available.begin(), cached.available.end(), begin(ItemAvailable));
		copy(cached.ids.begin(), cached.ids.end(), begin(SharedGroundItems));
		memcpy(ItemMap, cached.tiles, sizeof(ItemMap));
		ItemsCount = cached.count;
		SharedItemsProcess();
		assert(ItemsCount == 1 && FindSharedGroundItem(id) == -1);
	});
	Request(1, Clients[1].shared.begin()->first);
	At(1, []{ Cur.GraphicsID = CM_12_ITEMS_PICS_START; });
	Pump();
	assert(Inventory[1].empty() && Clients[0].shared.size() == 1);
	assert(Clients[1].cursor.GraphicsID == CM_12_ITEMS_PICS_START);
	cout << "PASS warp during pickup, stale cached level, occupied cursor\n";
}

void TestLateJoinAndTombstones()
{
	Reset();
	u64 first = Drop(0);
	Pump();
	auto staleDrop = History.front();
	Request(1, first);
	Pump();
	u64 floor = Drop(0);
	Pump();
	At(2, []{ SharedItemsInit(); ClearLocalGround(); gbBufferMsgs = BUFFER_ON; });
	At(0, []{ SharedItemsSend(2); });
	Pump();
	assert(Clients[2].shared.empty() && !Clients[2].buffered.empty());
	auto buffered = Clients[2].buffered;
	At(2, []{ gbBufferMsgs = BUFFER_PROCESS; });
	for( const auto& message : buffered ) Deliver(message, 2);
	At(2, []{ gbBufferMsgs = BUFFER_OFF; SharedItemsProcess(); });
	assert(Clients[2].shared.size() == 1 && Clients[2].shared.count(floor));
	assert(Clients[2].taken.count(first) && Clients[2].count == 1);
	Deliver(staleDrop, 2);
	assert(Clients[2].shared.size() == 1 && Inventory[2].empty());
	Request(2, floor);
	Pump();
	assert(Inventory[2].size() == 1 && Clients[0].shared.empty());
	cout << "PASS buffered late join, consumed snapshots, stale drop rejection\n";
}

void TestRetriesAndFullGround()
{
	Reset();
	u64 id = Drop(0);
	vector<Message> dropMessages(Wire.begin(), Wire.end());
	Wire.clear();
	for( const auto& message : dropMessages ) Deliver(message, 2);
	// Authority temporarily lacks the drop; requester keeps retrying.
	At(0, []{ SharedItems.clear(); SharedItemPayloads.clear(); ClearLocalGround(); });
	Request(2, id);
	Pump();
	assert(Inventory[2].empty());
	for( const auto& message : dropMessages ) { Deliver(message, 0); Deliver(message, 1); }
	Now += 501;
	At(2, []{ SharedItemsProcess(); });
	Pump();
	assert(Inventory[2].size() == 1);

	Reset(2);
	At(1, []{
		for( int i = 0; i < MaxItems_255; ++i ){
			int index;
			assert(GetItemSpace(i / 100, i % 100, index));
			Items[index].info = RichItem(8);
		}
	});
	id = Drop(0);
	Pump();
	assert(Clients[1].count == MaxItems_255 && Clients[1].shared.count(id));
	At(1, [&]{
		RemoveGround(ItemActive[0]);
		SharedItemsProcess();
		assert(FindSharedGroundItem(id) != -1);
	});
	Request(1, id);
	Pump();
	assert(Inventory[1].size() == 1 && Clients[1].count == MaxItems_255 - 1);
	cout << "PASS out-of-order request retries, full ground array, deferred projection\n";
}

void TestDropScopeAndFailure()
{
	Reset(2);
	At(0, []{
		assert(DropCursorItemBeforeTrig(false));
		assert(SharedItems.empty() && Wire.empty());
	});
	Reset(1);
	At(0, []{
		assert(DropCursorItemBeforeTrig(true));
		assert(SharedItems.empty() && Wire.empty());
	});
	Reset(2);
	DropFails = true;
	At(0, []{
		assert(!DropCursorItemBeforeTrig(true));
		assert(SharedItems.empty() && Wire.empty());
	});
	cout << "PASS automatic drops stay private, single-player behavior, failed drop\n";
}

void TestFragmentsAndSnapshotOverlap()
{
	Reset();
	u64 id = Drop(0);
	vector<Message> messages(Wire.begin(), Wire.end());
	Wire.clear();
	assert(messages.size() == 1 + SharedItemChunkCount);
	TCmdSharedData firstChunk;
	memcpy(&firstChunk, messages[1].bytes.data(), sizeof(firstChunk));
	char* serializedPointer;
	memcpy(&serializedPointer, firstChunk.data + offsetof(Item, ItemCELFilePtr), sizeof(serializedPointer));
	assert(serializedPointer == nullptr);
	// Metadata and chunks can arrive in any order; incomplete items stay hidden.
	Deliver(messages.back(), 1);
	Deliver(messages.back(), 1);
	Deliver(messages.front(), 1);
	assert(Clients[1].count == 0);
	for( uint i = 1; i + 1 < messages.size(); ++i ){
		Deliver(messages[i], 1);
		assert(Clients[1].count == (i + 2 == messages.size() ? 1 : 0));
	}
	assert(Clients[1].count == 1);

	// Snapshot sender has only metadata and one chunk when the join begins.
	At(1, []{ SharedItemsInit(); ClearLocalGround(); });
	Deliver(messages.front(), 1);
	Deliver(messages[1], 1);
	At(1, []{ SharedItemsSend(2); });
	Pump();
	assert(Clients[2].count == 0);
	// Original broadcasts for the remaining chunks preceded the connection.
	// Only the existing peer sees them; it must forward the missing state.
	for( uint i = 2; i < messages.size(); ++i ) Deliver(messages[i], 1);
	Pump();
	assert(Clients[2].count == 1 && Clients[2].shared.count(id));
	Request(2, id);
	Pump();
	assert(Inventory[2].size() == 1);
	for( int i = 0; i < 3; ++i ) assert(Clients[i].payloads.empty());
	cout << "PASS complete item bytes, pointer clearing, reordered fragments, overlapping snapshot\n";
}

void TestGrantAfterSnapshotAndAuthorityChange()
{
	Reset(2);
	u64 id = Drop(0);
	Pump();
	Request(1, id);
	auto request = Wire.front();
	Wire.clear();
	Deliver(request, 0);
	auto grant = Wire.front();
	Wire.clear();
	At(0, []{ SharedItemsSend(1); });
	Pump(); // snapshot removal overtakes the actual grant
	assert(Inventory[1].empty() && Clients[1].count == 0);
	Deliver(grant, 1);
	Deliver(grant, 1);
	assert(Inventory[1].size() == 1);

	Reset(2);
	id = Drop(0);
	Pump();
	Request(1, id);
	request = Wire.front();
	Wire.clear();
	Deliver(request, 0);
	grant = Wire.front();
	Wire.clear();
	At(1, []{ SharedItemsInit(); ClearLocalGround(); }); // a new character in the same slot
	Deliver(grant, 1);
	assert(Inventory[1].empty() && Wire.empty() && Clients[1].shared.empty());

	Reset();
	id = Drop(0);
	Pump();
	Players[0].IsExists = false;
	Request(2, id);
	Pump();
	assert(Inventory[2].size() == 1 && Clients[1].shared.empty());
	cout << "PASS snapshot before grant, reused player slot, authority disconnect\n";
}

void TestSavedSpellAndSocketChoices()
{
	Reset(2);
	At(0, []{
		Players[0].fullClassId = 0;
		Players[1].fullClassId = 1;
		for( int i = 1; i < PS_COUNT; ++i ) Spells[i].RequiredClassMask = 1u << (i % 2);
		SharedItemRecreation = 0;
		int sourceBook = ChooseBookForTest(0, 10);
		int sourceStaff = ChooseStaffForTest(Items[0], 10);
		Items[0].seed = -123456789;
		Items[0].charFlag = 0;
		Items[0].seed64_reserved = 999;
		assert(Items[0].SavedSeed() == -123456789); // ordinary saves remain unchanged
		Items[0].SpellIndex = sourceBook;
		Items[0].socketsBase = 5;
		PreserveSharedItemProperties(Items[0]);
		i64 saved = Items[0].SavedSeed();
		uint savedChoices = u64(saved) >> 32;
		assert(int(saved) == -123456789 && ((savedChoices >> 8) & 7) == 5);
		CurrentPlayerIndex = 1;
		assert(ChooseBookForTest(0, 10) != sourceBook);
		assert(ChooseStaffForTest(Items[0], 10) != sourceStaff);
		SharedItemRecreation = savedChoices | 0x80000000u;
		assert(ChooseBookForTest(0, 10) == sourceBook);
		SharedItemRecreation = uint(sourceStaff) | 0x80000000u;
		assert(ChooseStaffForTest(Items[0], 10) == sourceStaff);
		SharedItemRecreation = 0;
		CurrentPlayerIndex = 0;
	});
	cout << "PASS saved seed compatibility, signed seeds, sockets, cross-class book/staff reconstruction\n";
}

void TestExpandedDungeonRange()
{
	Reset(2);
	for( int i = 0; i < ClientCount; ++i ){
		Clients[i].level = DungeonCount - 1;
		Players[i].dungeon = DungeonCount - 1;
	}
	u64 id = Drop(0);
	Pump();
	assert(Clients[1].count == 1);
	Request(1, id);
	Pump();
	assert(Inventory[1].size() == 1 && Clients[0].count == 0 && Clients[1].count == 0);
	At(0, []{
		TCmdSharedItem packet{};
		packet.dropId = 123;
		packet.info = RichItem();
		packet.dungeon = DungeonCount;
		assert(!ValidSharedItem(packet));
		packet.dungeon = -1;
		assert(!ValidSharedItem(packet));
	});
	cout << "PASS expanded dungeon range and separation from dungeon-delta commands\n";
}

int main()
{
	TestPrivateLootAndRace();
	TestFullInventoryAndPartialGold();
	TestLevelChangeAndBusyCursor();
	TestLateJoinAndTombstones();
	TestRetriesAndFullGround();
	TestDropScopeAndFailure();
	TestFragmentsAndSnapshotOverlap();
	TestGrantAfterSnapshotAndAuthorityChange();
	TestSavedSpellAndSocketChoices();
	TestExpandedDungeonRange();
	cout << "All shared-item protocol checks passed.\n";
}
