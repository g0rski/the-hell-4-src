#!/usr/bin/env python3
"""Exercise the production inventory sorter and button with ASan/UBSan."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from test_shared_items import function

ROOT = Path(__file__).resolve().parents[1]


def main():
    def read(name):
        return (ROOT / name).read_text(encoding="utf-8-sig", errors="replace")

    enums = read("src/enums.h")
    structs = read("src/structs.h")
    layout = ""
    for name in ("ITEM_CODE", "MAGIC_CODE", "MAGIC_LEVEL", "QUENCH_AFFIX",
                 "AFFIX_EFFECT", "AFFIX_ORIGIN"):
        signature = re.search(r"^enum " + name + r"\b[^\n]*", enums, re.MULTILINE)[0]
        layout += function(enums, signature) + ";\n"
    for name in ("BaseEffect", "Effect", "Item", "DisplayObject"):
        match = re.search(r"^struct " + name + r"\b[^\n]*", structs, re.MULTILINE)
        definition = function(structs[match.start():], match[0])
        if name == "Item":
            definition = definition[:definition.index("\tstatic constexpr StructVersion")] + "}\n"
        layout += definition + ";\n"

    panel = read("src/InventoryPanel.cpp")
    production = panel[panel.index("struct InventorySortEntry\n"):
                       panel.index("//----- (0042118B)")]
    # Exercise all real drop-level variants of healing/mana potions, which have
    # distinct base IDs but should still form one group per potion effect.
    bases = []
    for line in read("src/BaseItems.cpp").splitlines():
        match = re.match(r'\{\s*(\d+),.*?"(Potion of (?:Full )?(?:Healing|Mana))"', line)
        if match:
            magic = re.search(r"\bMC_\w+", line)[0]
            bases.append(f'{{{match[1]}, {magic}, "{match[2]}"}},')

    with tempfile.TemporaryDirectory(prefix="dyniamod-inventory-sort-") as temporary:
        directory = Path(temporary)
        (directory / "inventory_layout.inc").write_text(layout)
        (directory / "inventory_sort.inc").write_text(production)
        (directory / "inventory_potion_bases.inc").write_text("\n".join(bases))
        binary = directory / "inventory_sort_test"
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++17", "-fno-operator-names",
            "-Wall", "-Wextra", "-Werror", "-Wno-missing-field-initializers",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-g",
            "-I", str(directory), str(ROOT / "tests/inventory_sort_harness.cpp"),
            "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
