#!/usr/bin/env python3
"""Run the production shared-item protocol with simulated clients on any C++17 host."""

from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start:end + 1] + "\n"
    raise ValueError(f"Unterminated function: {signature}")


def main():
    structs = (ROOT / "src/structs.h").read_text(encoding="utf-8-sig")
    messages = (ROOT / "src/msg.h").read_text(encoding="utf-8-sig")
    transport = (ROOT / "src/multi.h").read_text(encoding="utf-8-sig")
    items = (ROOT / "src/Item.cpp").read_text(encoding="utf-8-sig", errors="replace")
    protocol = (ROOT / "src/SharedItems.cpp").read_text(encoding="utf-8-sig")
    enums = (ROOT / "src/enums.h").read_text(encoding="utf-8-sig", errors="replace")
    limits = function(enums, "enum NET_CMD: uchar") + ";\n"
    limits += function(enums, "enum DUNGEON: char").replace("enum DUNGEON: char", "enum DUNGEON_LIMITS: char", 1) + ";\n"
    item_info = structs[structs.index("struct ItemInfo64\n"):structs.index("using LastItemInfo = ItemInfo64;")]
    item_info += "using LastItemInfo = ItemInfo64;\n"
    packets = messages[messages.index("struct TCmdSharedItem\n"):messages.index("struct DLevel\n")]
    header_start = transport.index("struct TPktHdr\n")
    packets = transport[header_start:transport.index("};", header_start) + 2] + "\n" + packets
    production = protocol.replace('#include "stdafx.h"', "", 1)
    production += function(items, "int DropCursorItemBeforeTrig( bool share )")
    production += function(items, "void __fastcall DeleteItem( int itemIndex, int freeIndex )")
    production += function(items, "i64 Item::SavedSeed() const")
    production += function(items, "void PreserveSharedItemProperties( Item& item )")
    book = function(items, "void __fastcall Item_InitBooks( int itemIndex, int qlvl )")
    book = book[book.index("{") + 1:book.index("\n\tSpell& spell")]
    production += "int ChooseBookForTest(int itemIndex, int qlvl) {" + book + "\n(void)player; return spellIndex; }\n"
    staff = function(items, "void __fastcall Item_AddSpellCastAbility(Item& item,")
    staff = staff[staff.index("{") + 1:staff.index("\n\tSpell& spell")]
    production += "int ChooseStaffForTest(Item& item, int maxAffixQLVL) {" + staff + "\n(void)baseItem; return spellIndex; }\n"

    with tempfile.TemporaryDirectory(prefix="th4-shared-items-") as temporary:
        directory = Path(temporary)
        (directory / "item_info.inc").write_text(item_info)
        (directory / "shared_items_limits.inc").write_text(limits)
        (directory / "shared_items_packets.inc").write_text(packets)
        (directory / "shared_items_under_test.inc").write_text(production)
        binary = directory / "shared_items_test"
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++17", "-fno-operator-names", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-fno-sanitize-recover=all",
            "-g", "-I", str(directory), str(ROOT / "tests/shared_items_harness.cpp"),
            "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
