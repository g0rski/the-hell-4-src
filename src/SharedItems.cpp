#include "stdafx.h"

static_assert( sizeof(TCmdSharedItem) + sizeof(TPktHdr) <= 128, "Shared item command exceeds the minimum transport packet" );
static_assert( sizeof(TCmdSharedData) + sizeof(TPktHdr) <= 128, "Shared item data exceeds the minimum transport packet" );

// Only manual inventory drops enter this registry. Private loot never uses it.
u64 SharedGroundItems[MaxItems_255];

namespace
{
map<u64, TCmdSharedItem> SharedItems;
set<u64> TakenSharedItems;
set<u64> AppliedSharedPickups;
set<u64> RequestedSharedItems;

constexpr uint CompleteSharedItem = (1u << SharedItemChunkCount) - 1;
struct SharedItemPayload
{
	Item item{};
	uint received = 0;
};
map<u64, SharedItemPayload> SharedItemPayloads;
uint SharedSnapshotRecipients = 0;

void ForwardSharedUpdate( const void* packet, uchar size, int sender )
{
	// A snapshot can overlap a fragmented drop or pickup from another sender.
	// Forward newly observed state to snapshot recipients; ids/chunk masks make
	// these updates idempotent even when the original broadcast also arrives.
	uint mask = SharedSnapshotRecipients & ~(1u << CurrentPlayerIndex) & ~(1u << sender);
	if( mask ) NetSendMask(mask, (uchar*)packet, size);
}

struct PendingSharedPickup
{
	TCmdSharedItem packet;
	i64 sentAt;
};
map<u64, PendingSharedPickup> PendingSharedPickups;

int SharedItemAuthority()
{
	// Use one session authority, including when players are on different levels.
	// Changing levels must not create two simultaneous pickup arbiters.
	for( int i = 0; i < PlayersMax_4; ++i ){
		if( Players[i].IsExists ) return i;
	}
	return -1;
}

int FindSharedGroundItem( u64 dropId )
{
	for( int i = 0; i < ItemsCount; ++i ){
		int itemIndex = ItemActive[i];
		if( SharedGroundItems[itemIndex] == dropId ) return itemIndex;
	}
	return -1;
}

void RemoveSharedGroundItem( u64 dropId )
{
	for( int i = ItemsCount - 1; i >= 0; --i ){
		int itemIndex = ItemActive[i];
		if( SharedGroundItems[itemIndex] != dropId ) continue;
		Item& item = Items[itemIndex];
		if( ItemMap[item.MapRow][item.MapCol] == itemIndex + 1 ){
			ItemMap[item.MapRow][item.MapCol] = 0;
		}
		DeleteItem(itemIndex, i);
	}
}

bool ValidSharedItem( const TCmdSharedItem& packet )
{
	return packet.dropId && (uint)packet.dungeon < DungeonCount
		&& (uint)packet.row < FineMap_112 && (uint)packet.col < FineMap_112
		&& (uint)packet.info.baseItemIndex < count_BaseItems;
}

bool SharedItemReady( u64 dropId )
{
	auto it = SharedItemPayloads.find(dropId);
	return it != SharedItemPayloads.end() && it->second.received == CompleteSharedItem;
}

void MaterializeSharedItem( const TCmdSharedItem& packet )
{
	if( Dungeon != packet.dungeon || Players[CurrentPlayerIndex].lvlChanging
	 || !SharedItemReady(packet.dropId) || FindSharedGroundItem(packet.dropId) != -1 ) return;

	int itemIndex = -1;
	u64 dropId = packet.dropId;
	LastItemInfo info{};
	info = SharedItemPayloads.at(dropId).item;
	if( memcmp(&info, &packet.info, sizeof(info)) != 0 ) return;
	// Private loot may occupy this tile on this client. Find nearby space without
	// replacing private loot or depending on another client's ground-item index.
	if( !GetItemSpace(packet.row, packet.col, itemIndex) ) return;
	int row = Items[itemIndex].MapRow;
	int col = Items[itemIndex].MapCol;
	// Seed-based LoadItem depends on the recipient's class, traits and level.
	// Copy the full saved item so sharing cannot reroll spells or modifiers.
	Items[itemIndex] = SharedItemPayloads.at(dropId).item;
	Items[itemIndex].MapRow = row;
	Items[itemIndex].MapCol = col;
	SharedGroundItems[itemIndex] = packet.dropId;
	RespawnItem(itemIndex, true);
}

void SendSharedItemData( u64 dropId, const Item& item, uint playerMask = 0, uint chunks = CompleteSharedItem )
{
	Item serialized = item;
	serialized.ItemCELFilePtr = nullptr;
	for( uint i = 0; i < SharedItemChunkCount; ++i ){
		if( !(chunks & (1u << i)) ) continue;
		TCmdSharedData packet{};
		packet.bCmd = CMD_114_SHARED_DATA;
		packet.dropId = dropId;
		packet.chunkIndex = i;
		uint offset = i * SharedItemChunkSize;
		uint size = min(SharedItemChunkSize, uint(sizeof(Item)) - offset);
		memcpy(packet.data, (const uchar*)&serialized + offset, size);
		if( playerMask ) NetSendMask(playerMask, (uchar*)&packet, sizeof(packet));
		else NetSendDirect((uchar*)&packet, sizeof(packet));
	}
}

u64 NextSharedDropId()
{
	// Do not consume gameplay RNG. Include wall-clock time so a reconnect using
	// a vacated player slot cannot reuse an earlier process's drop sequence.
	static u64 sequence = 0;
	u64 now = chrono::system_clock::now().time_since_epoch().count();
	u64 dropId;
	do{
		sequence = max(sequence + 1, now);
		dropId = (sequence << 2) | CurrentPlayerIndex;
	}while( !dropId || SharedItems.count(dropId) || TakenSharedItems.count(dropId) );
	return dropId;
}

void PublishSharedItem( TCmdSharedItem packet, const Item& item, int itemIndex = -1 )
{
	packet.bCmd = CMD_110_SHARED_DROP;
	u64 dropId = NextSharedDropId();
	packet.dropId = dropId;
	packet.player = CurrentPlayerIndex;
	packet.autoGet = false;
	SharedItems.emplace(dropId, packet);
	SharedItemPayloads.emplace(dropId, SharedItemPayload{item, CompleteSharedItem});
	if( itemIndex != -1 ) SharedGroundItems[itemIndex] = dropId;
	NetSendDirect((uchar*)&packet, sizeof(packet));
	SendSharedItemData(dropId, item);
}

void ApplySharedPickup( const TCmdSharedItem& packet )
{
	u64 dropId = packet.dropId;
	PendingSharedPickups.erase(dropId);
	if( packet.player != CurrentPlayerIndex ){
		RemoveSharedGroundItem(packet.dropId);
		return;
	}

	int itemIndex = FindSharedGroundItem(packet.dropId);
	if( itemIndex == -1 && Dungeon == packet.dungeon ){
		MaterializeSharedItem(packet);
		itemIndex = FindSharedGroundItem(packet.dropId);
	}
	bool canPickUp = itemIndex != -1 && Dungeon == packet.dungeon
		&& !Players[CurrentPlayerIndex].lvlChanging && Cur.GraphicsID == CM_1_NORMAL_HAND;
	if( canPickUp ){
		if( packet.autoGet ) AutoGetItem(CurrentPlayerIndex, itemIndex);
		else InvGetItem(CurrentPlayerIndex, itemIndex);
	}
	if( itemIndex != -1 && SharedGroundItems[itemIndex] == packet.dropId ){
		// A full inventory (including partially placed gold) leaves the remainder
		// on the ground. Publish its actual state under a fresh drop identity.
		ShareDroppedItem(itemIndex);
	}else if( !canPickUp ){
		// A level change or a full local ground-item array can overtake a grant.
		// Keep the item available on its original level instead of losing it.
		u64 dropId = packet.dropId;
		auto it = SharedItemPayloads.find(dropId);
		if( it != SharedItemPayloads.end() ) PublishSharedItem(packet, it->second.item);
	}else{
		check_update_plr(CurrentPlayerIndex);
	}
}
}

void SharedItemsInit()
{
	SharedItems.clear();
	TakenSharedItems.clear();
	AppliedSharedPickups.clear();
	RequestedSharedItems.clear();
	SharedItemPayloads.clear();
	SharedSnapshotRecipients = 0;
	PendingSharedPickups.clear();
	memzero(SharedGroundItems);
}

void SharedItemsSend( int playerIndex )
{
	if( MaxCountOfPlayersInGame == 1 ) return;
	SharedSnapshotRecipients |= 1u << playerIndex;
	// These ordinary commands enter the joiner's existing message buffer before
	// the delta terminator. No private ground items are exported.
	for( const auto& entry : SharedItems ){
		auto packet = entry.second;
		NetSendMask(1u << playerIndex, (uchar*)&packet, sizeof(packet));
		auto payload = SharedItemPayloads.find(entry.first);
		if( payload != SharedItemPayloads.end() ){
			SendSharedItemData(entry.first, payload->second.item, 1u << playerIndex, payload->second.received);
		}
	}
	for( u64 dropId : TakenSharedItems ){
		TCmdSharedRemove packet{};
		packet.bCmd = CMD_113_SHARED_REMOVE;
		packet.dropId = dropId;
		NetSendMask(1u << playerIndex, (uchar*)&packet, sizeof(packet));
	}
}

void ShareDroppedItem( int itemIndex )
{
	PreserveSharedItemProperties(Items[itemIndex]);
	TCmdSharedItem packet{};
	packet.dungeon = Dungeon;
	packet.row = Items[itemIndex].MapRow;
	packet.col = Items[itemIndex].MapCol;
	packet.info = Items[itemIndex];
	PublishSharedItem(packet, Items[itemIndex], itemIndex);
	check_update_plr(CurrentPlayerIndex);
}

void RequestSharedItem( int itemIndex, bool autoGet )
{
	u64 dropId = SharedGroundItems[itemIndex];
	auto it = SharedItems.find(dropId);
	if( it == SharedItems.end() || !SharedItemReady(dropId) || PendingSharedPickups.count(dropId) ) return;
	auto packet = it->second;
	packet.bCmd = CMD_111_SHARED_REQUEST;
	packet.player = CurrentPlayerIndex;
	packet.autoGet = autoGet;
	RequestedSharedItems.insert(dropId);
	PendingSharedPickups.emplace(dropId, PendingSharedPickup{packet, GetTickCountTh()});
	NetSendDirect((uchar*)&packet, sizeof(packet));
}

void SharedItemsProcess()
{
	if( MaxCountOfPlayersInGame == 1 || gbBufferMsgs == BUFFER_ON
	 || Players[CurrentPlayerIndex].lvlChanging ) return;

	// A cached level can contain projections taken while this client was away.
	for( int i = ItemsCount - 1; i >= 0; --i ){
		u64 dropId = SharedGroundItems[ItemActive[i]];
		if( dropId && !SharedItems.count(dropId) ) RemoveSharedGroundItem(dropId);
	}
	for( const auto& entry : SharedItems ) MaterializeSharedItem(entry.second);

	for( auto it = PendingSharedPickups.begin(); it != PendingSharedPickups.end(); ){
		auto& pending = it->second;
		if( !SharedItems.count(it->first) || Dungeon != pending.packet.dungeon
		 || Cur.GraphicsID != CM_1_NORMAL_HAND ){
			int itemIndex = FindSharedGroundItem(it->first);
			if( itemIndex != -1 ) Items[itemIndex].gottenFromLand = false;
			it = PendingSharedPickups.erase(it);
			continue;
		}
		// A request can precede its drop on the authority or outlive an authority
		// disconnect. Retry while this client is still waiting for the item.
		if( GetTickCountTh() - pending.sentAt >= 500_ms ){
			pending.sentAt = GetTickCountTh();
			NetSendDirect((uchar*)&pending.packet, sizeof(pending.packet));
		}
		++it;
	}
}

uint On_SHARED_DROP( const TCmdSharedItem* packet, int playerIndex )
{
	// Packed wire fields must be copied before passing them to STL by reference.
	u64 dropId = packet->dropId;
	if( gbBufferMsgs == BUFFER_ON ){
		mega_add_data(playerIndex, packet, sizeof(*packet));
	}else if( ValidSharedItem(*packet) && !TakenSharedItems.count(dropId) ){
		if( SharedItems.emplace(dropId, *packet).second ){
			ForwardSharedUpdate(packet, sizeof(*packet), playerIndex);
			MaterializeSharedItem(*packet);
		}
	}
	return sizeof(*packet);
}

uint On_SHARED_REQUEST( const TCmdSharedItem* packet, int playerIndex )
{
	if( gbBufferMsgs == BUFFER_ON || SharedItemAuthority() != CurrentPlayerIndex
	 || packet->player != playerIndex || (uint)playerIndex >= PlayersMax_4 ) return sizeof(*packet);
	u64 dropId = packet->dropId;
	auto it = SharedItems.find(dropId);
	if( it == SharedItems.end() || !SharedItemReady(dropId) || it->second.dungeon != packet->dungeon
	 || Players[playerIndex].dungeon != packet->dungeon ) return sizeof(*packet);

	// Consume the authoritative record before sending the grant. A second
	// simultaneous request can no longer find or receive this drop.
	auto grant = it->second;
	grant.bCmd = CMD_112_SHARED_TAKE;
	grant.player = packet->player;
	grant.autoGet = packet->autoGet;
	SharedItems.erase(it);
	TakenSharedItems.insert(dropId);
	AppliedSharedPickups.insert(dropId);
	NetSendDirect((uchar*)&grant, sizeof(grant));
	ApplySharedPickup(grant);
	SharedItemPayloads.erase(dropId);
	return sizeof(*packet);
}

uint On_SHARED_TAKE( const TCmdSharedItem* packet, int playerIndex )
{
	u64 dropId = packet->dropId;
	if( gbBufferMsgs == BUFFER_ON ){
		mega_add_data(playerIndex, packet, sizeof(*packet));
	}else if( ValidSharedItem(*packet) && packet->player < PlayersMax_4
	 && AppliedSharedPickups.insert(dropId).second ){
		TakenSharedItems.insert(dropId);
		SharedItems.erase(dropId);
		ForwardSharedUpdate(packet, sizeof(*packet), playerIndex);
		// A snapshot removal can precede the grant. Track grant application
		// separately, and never award an old player's grant to a reused slot.
		if( packet->player != CurrentPlayerIndex || RequestedSharedItems.count(dropId) ){
			ApplySharedPickup(*packet);
		}else{
			RemoveSharedGroundItem(dropId);
		}
		SharedItemPayloads.erase(dropId);
	}
	return sizeof(*packet);
}

uint On_SHARED_REMOVE( const TCmdSharedRemove* packet, int playerIndex )
{
	u64 dropId = packet->dropId;
	if( gbBufferMsgs == BUFFER_ON ){
		mega_add_data(playerIndex, packet, sizeof(*packet));
	}else if( dropId ){
		if( TakenSharedItems.insert(dropId).second ) ForwardSharedUpdate(packet, sizeof(*packet), playerIndex);
		SharedItems.erase(dropId);
		if( !RequestedSharedItems.count(dropId) || AppliedSharedPickups.count(dropId) ){
			SharedItemPayloads.erase(dropId);
		}
		PendingSharedPickups.erase(dropId);
		RemoveSharedGroundItem(dropId);
	}
	return sizeof(*packet);
}

uint On_SHARED_DATA( const TCmdSharedData* packet, int playerIndex )
{
	u64 dropId = packet->dropId;
	if( gbBufferMsgs == BUFFER_ON ){
		mega_add_data(playerIndex, packet, sizeof(*packet));
	}else if( dropId && packet->chunkIndex < SharedItemChunkCount && !TakenSharedItems.count(dropId) ){
		auto& payload = SharedItemPayloads[dropId];
		if( payload.received & (1u << packet->chunkIndex) ) return sizeof(*packet);
		uint offset = packet->chunkIndex * SharedItemChunkSize;
		uint size = min(SharedItemChunkSize, uint(sizeof(Item)) - offset);
		memcpy((uchar*)&payload.item + offset, packet->data, size);
		payload.item.ItemCELFilePtr = nullptr;
		payload.received |= 1u << packet->chunkIndex;
		ForwardSharedUpdate(packet, sizeof(*packet), playerIndex);
		auto it = SharedItems.find(dropId);
		if( it != SharedItems.end() && SharedItemReady(dropId) ){
			LastItemInfo info{};
			info = payload.item;
			if( memcmp(&info, &it->second.info, sizeof(info)) == 0 ) MaterializeSharedItem(it->second);
		}
	}
	return sizeof(*packet);
}
