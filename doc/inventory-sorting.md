# Inventory sorting

The brass button immediately to the left of the sigil inventory switch sorts
the open inventory. Its icon shows rows of cells and an upward arrow. Hovering
shows a tooltip, including a Polish translation when Polish is selected.

Items fill the bottom row from left to right, followed by rows above it.
Multi-cell items keep their original dimensions and extend upward from their
bottom-left cell. Taller and wider items are placed first to leave room for
equipment. Within each size, items are grouped by type and variant (including
spell, subtype and grade), then by their visible name and picture. Identical
potions and oils stay together even when they have different drop-level base IDs.
Unidentified magical names are not used for ordering.

Sorting moves only grid references. Item records, inventory counts, gold piles,
equipment, belt contents and the other inventory remain unchanged. Slot numbers
are preserved, so references held by other panels stay valid. No data archives,
new save fields or network protocol changes are needed.

Sorting is disabled while holding an item, using a special cursor, performing
an action other than standing/walking, splitting gold or dead. A complete new
layout is prepared before replacing the grid. A bounded packing search handles
crowded bags; if it cannot find a layout, the original inventory remains intact
and a message is shown.

## Automated checks

```sh
python3 tools/test_inventory_sort.py
python3 tools/test_shared_items.py
python3 tools/test_save_compatibility.py
python3 tools/test_potion_conversion.py
```

The sort harness compiles the production item definition, packing, sorting,
button drawing and hit detection with ASan/UBSan. It checks full inventories,
grouping (including real potion drop variants), bottom-to-top ordering,
five-column bags, sigils, mixed-size items,
repeated sorting, byte-for-byte preservation of item records and other player
state, rejected actions and rollback. It also exercises 400 generated valid
mixed-size inventories and button bounds at different panel positions.

## Windows gameplay checks

1. Open the inventory at supported resolutions. Check the button's brass frame,
   icon, hover/pressed states and tooltip next to the sigil switch. Verify that
   neither button overlaps the other or the gold count.
2. Mix potions, gems, oils, gold and equipment. Click sort and verify that matching
   items are adjacent, starting at the bottom left. Click again and verify that
   positions stay the same. Pick up and replace every multi-cell item from each
   of its occupied cells.
3. Repeat with a full bag and with Lithe Build's restricted columns. Hold an item
   over the button and confirm that it stays on the cursor and the grid does not
   change. Check disabled behavior while splitting gold and while dead.
4. Open the sigil inventory and sort it. Confirm that the main inventory and
   equipped sigils' effects remain unchanged. Also check sorting in game modes
   or classes where the sigil switch is unavailable.
5. Save and reload both sorted inventories. Check item positions, properties,
   counts and gold. In multiplayer, pick up, use and share sorted items, then
   reconnect and check persistence.

Automated checks and Win32 builds do not replace these rendering, save/reload
and live multiplayer gameplay checks with the installed game data.
