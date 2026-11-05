# Adria potion conversion

DyniaMod extends **Witch's shack → Enchant Items** using its existing item slot.
Healing and mana potions show conversion buttons in place of enchant tiers.
Weapons and armor retain the existing enchant buttons, level requirements,
price modifiers and affix behavior. The existing restrictions on access to
Adria's enchanting service also apply to conversion, in single and multiplayer.

Only these four potion types are accepted; alternate base entries with the same
healing/mana effect are accepted too. Rejuvenation, holy water, elixirs, scrolls
and other consumables are excluded.

| Input | Output | Gold |
| --- | --- | ---: |
| Potion of Healing | Potion of Mana | 9 |
| Potion of Healing | Potion of Full Healing | 30 |
| Potion of Healing | Potion of Full Mana | 30 |
| Potion of Mana | Potion of Healing | 9 |
| Potion of Mana | Potion of Full Healing | 30 |
| Potion of Mana | Potion of Full Mana | 30 |
| Potion of Full Healing | Potion of Full Mana | 9 |
| Potion of Full Mana | Potion of Full Healing | 9 |

Full potions cannot be downgraded to ordinary potions. There is no conversion
to the input's own type. These fees are fixed, independent of shop discounts,
traits or the character level.

Pressing and releasing the same enabled option converts one potion immediately
and puts its result into a free backpack cell. The slot automatically takes
another potion of the original input type, checking the backpack first and
then the belt. After the last input it becomes empty. It does not select another
potion type or use the freshly converted result as its next input. Closing the
window returns any pending input through the existing inventory/drop path.

Insufficient gold, a full backpack or an occupied item cursor disables conversion.
The gold and space checks run again on release, before any mutation. Releasing
a drag over an option, releasing over another option, or receiving another
mouse-up event does not purchase a conversion. Disabled options in a full
backpack explain the missing inventory space in a tooltip.

Each option occupies one button in the original enchant options area, showing
only the potion name and price, for example **Potion of Full Healing, 30 gold**.
The complete names and fees fit within the existing panel width.

## Persistence and compatibility

Conversion creates a normal base potion with the target's name, graphic, effect
and price using `SetPlayerHandItem`. It preserves the input's seed/generation
version and resets its origin to `D_0_SIMPLE`; shop-specific restoration must
not recreate a previous shop potion instead of the conversion result. It does
not add enchant affixes or change save structures or multiplayer packets.
The save format remains **30** and `ItemInfo64` remains **64 bytes**.

The executable, project name, window caption and version resources use
**DyniaMod**, replacing ItemShare. The base game version and existing network
revision remain v0.774.1 and 10774.

## Verification

Run `python3 tools/test_potion_conversion.py`. It builds a C++17 harness with
ASan/UBSan and executes the production conversion/enchant handlers, base item
definitions, gold counting/payment, inventory cell placement/removal and compact
item save/reconstruction. It checks all eight conversions, forbidden targets,
exact and insufficient funds, full inventory, rechecks on release, input refill
from backpack/belt, conservation of potions/gold, shop and alternate inputs,
negative seeds, persistence and separation from equipment enchanting. Existing
equipment checks exercise its click, fee and affix invocation; the surrounding
renderer, audio, affix rolling/reconstruction and engine IO are stubbed. Label
width checks use the production Latin font tables.

Also run the existing shared-item and save-compatibility tests and compile Win32
Release and Debug with MSVC v145.

In a game installation with its data archives, check:

1. Drag each of the four potions from the backpack and belt into Adria's slot;
   confirm the displayed targets and prices match the table.
2. Convert several inputs in succession, including mixed backpack/belt stock;
   verify each result enters the backpack, one fee is paid per click and the
   slot clears after the final input.
3. Fill the backpack or run short of gold; verify the input and gold remain.
   Retrieve the input, close/reopen the window and repeat with another type.
4. Enchant eligible equipment at several tiers; verify its affixes and costs,
   and rejection of unsupported items and exhausted enchant slots.
5. Save/load converted potions, use their healing/mana effects, and manually
   share and pick them up in multiplayer. Verify their types remain correct.

Automated checks and compilation do not cover live rendering/gameplay or a
real multiplayer session; those require the game data and running clients.
