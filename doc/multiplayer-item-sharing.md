# Manual item sharing

Only explicit mouse drops, inventory Ctrl-drops, and controller drops publish a
shared item in multiplayer. Enemy/chest/reward loot and automatic drops caused
by overflow, crafting, death, or level transitions retain their private behavior.
Single-player drops follow the existing path.

## Implementation

`DropCursorItemBeforeTrig(true)` first uses `InvPutItem`. A successful drop is
published with its actual location and a new session drop ID. Forced drops use
the default `false` argument. The global `NetItem` switch stays disabled.

`SharedItems.cpp` keeps a separate registry and associates visible ground slots
with drop IDs. Private item searches exclude those slots, including when a
private item has the same seed as a shared item. Each client places shared items
using its existing free-space search, preserving private loot occupying the
advertised tile. If the local ground array is full, the shared item waits in the
registry until a slot becomes available.

The ordinary pickup request entry point routes shared items to the lowest
connected player index, regardless of level. That authority removes the shared
record before broadcasting one grant. Other simultaneous requests cannot claim
the same drop. Clients apply a grant once, remove the corresponding local ground
item, and use the existing cursor/automatic pickup functions for the winner.
Full inventories and partially placed gold republish the actual remainder under
a fresh ID. A level change overtaking a grant leaves the item on its original
level. Requests retry while the player is waiting.

The complete fixed-layout `Item` is sent in seven 80-byte chunks, alongside the
existing compact item metadata. Its sprite pointer is cleared on the wire and
rebuilt locally by `RespawnItem`. This preserves actual stats, affixes, names,
rarity, quantity, spells, sockets, enchants, quenching, durability, charges, and
requirements without running generation code for another player's class.
Items become visible after all chunks arrive and match the compact metadata.
Commands fit the transport's minimum 128-byte packet size, including its header.
Shared broadcasts use the v0.774 engine's `NetSendDirect`, which sends to local
loopback and other peers without the bounded high-priority queue. Targeted
snapshots use `NetSendMask`, which also sends directly. A burst of ordinary
gameplay updates therefore cannot silently discard a shared-item fragment or
pickup grant in that queue.

Compact character saves and periodic item reconstruction also need to preserve
class-dependent choices. Shared items use the previously unused high bit in
`charFlag` and the reserved high seed word to remember their spell and base
socket count. `SavedSeed()` includes that word only for marked items. Their
reconstruction preserves these choices, original shop quality, quantity, and
metadata, and bypasses recipient-specific loot-to-gold filtering. Unmarked item
generation and reconstruction retain their existing behavior. `Item` and
`ItemInfo64` keep their existing sizes and save layouts.

Shared ground IDs accompany the existing local level cache. On re-entry, stale
cached items are removed and remaining shared items are restored. Late joiners
receive shared registry snapshots through ordinary buffered network commands.
Snapshots include consumed IDs, and subsequent observed changes are forwarded
to snapshot recipients so a snapshot overlapping a fragmented drop cannot miss
its remaining chunks. Consumed IDs reject delayed drops and fragments; grants
have a separate application journal so a snapshot removal cannot swallow a
pending pickup. A grant to a reused player slot never awards an item without a
request from that client.

The executable is **DyniaMod v0.774.1**, based on the official **v0.774** source
release dated October 13, 2026. Its separate network revision **10774** prevents
stock clients and the previous v0.209 build from joining incompatible sessions.
All participants must use the updated executable. Commands 110–114 stay below
the upstream dungeon-delta range starting at command 128.

## Existing game and save compatibility

The base source archive is
[`th_team-th4-src_2026-10-13.zip`](https://thehellmod.ucoz.ru/Downloads/Src/th_team-th4-src_2026-10-13.zip)
(SHA-256 `8e59b657f558c6fa930bf20990dbd85273fd2b1c851314aecbeadf6d64636ba9`).
The archive's original author readme is preserved as `__README.txt`.

The official save version remains **30** and the current player record remains
**11,817 bytes**. This port does not replace that layout with the old March
source's version 27 layout. The actual production codec and reader accept the
supplied v0.778 multiplayer hero record, including its original checksum and
character name. No source save was edited or converted. Versions 18/21/22,
pre-powerup records and v27 powerup records retain their supported read paths.
Full gameplay compatibility with v0.778 still needs Windows verification; this
is a derivative of v0.774, not a reconstruction of the v0.778 release.

The save reader rejects unsupported future versions and mismatched record sizes. Hero
creation reserves every existing save archive, even when its hero cannot be
read or is absent from the filtered list. File access failures also reserve a
slot. This prevents creating a hero from overwriting an unseen save. It does
not recover a save already overwritten by an earlier executable.

## Automated verification

Run on a host with Python 3 and GCC with AddressSanitizer/UndefinedBehaviorSanitizer:

```sh
python3 tools/test_shared_items.py
python3 tools/test_save_compatibility.py
```

The portable harness compiles the production protocol, wire layouts, drop/delete
entry points, saved-seed helpers, and actual book/staff spell-selection code. It
simulates the surrounding engine and transport. It covers private loot sharing
a seed/tile, divergent item indices, simultaneous pickups, replayed grants,
partial gold pickup, full inventories/ground arrays, level changes, stale cached
levels, busy cursors, late joins, reordered fragments, overlapping snapshots,
snapshot removals before grants, reused slots, authority departure, automatic
drop isolation, single-player drops, signed seeds, saved sockets, and spell
reconstruction for another class.

The save compatibility harness compiles the production save layouts, codec,
reader and hero creation path. It checks format 30 and supported older layouts,
rejection of future versions and invalid/partial records, and preservation of existing unreadable
archives when creating a hero, including full slots and access errors. All file
writes are confined to a temporary fixture directory.

To additionally verify an existing encoded `mphero` member without changing it:

```sh
python3 tools/test_save_compatibility.py --hero-record /path/to/mphero.encoded
```

This harness does not replace Windows builds or live gameplay verification.

## Windows gameplay checks

Build Win32 Debug and Release using the commands in `BUILD.md`, then run two or
three updated clients in the same session:

1. Kill enemies, open chests, and collect rewards. Confirm each player's loot
   stays private, including after another player enters the level.
2. Drop items using the mouse, Ctrl-drop, and controller. Pick them up through
   cursor pickup, automatic pickup, and telekinesis. Confirm one owner and removal
   from every world view, including simultaneous pickup attempts.
3. Compare stats and quantities for gold, unidentified/magic/rare/unique items,
   books, charged weapons, socketed/enchanted/quenched equipment, and shop gear.
   Use different classes and character levels; wait through periodic item
   reconstruction, then save/reload the receiving character and compare again.
4. Repeat with a full inventory, a partly filled gold stack, private loot on the
   drop tile, and a full ground-item array. Confirm remainders remain shared and
   private loot is retained.
5. Leave/re-enter levels, join after an item was dropped or taken, change levels
   during pickup, and disconnect the authority before a new pickup request.
   Confirm stale items do not reappear and outstanding requests can complete.
6. Trigger crafting/overflow/death/transition drops and repeat ordinary drops in
   single-player. Confirm their existing behavior.
