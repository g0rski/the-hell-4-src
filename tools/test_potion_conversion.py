#!/usr/bin/env python3
"""Exercise Adria's production handlers, gold payment and item save reconstruction."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from test_shared_items import function

ROOT = Path(__file__).resolve().parents[1]


def main():
    def read(name):
        return (ROOT / name).read_text(encoding='utf-8-sig', errors='replace')

    enums = read('src/enums.h')
    structs = read('src/structs.h')
    items = read('src/Item.cpp')
    trade = read('src/PlayerTrade.cpp')
    save = read('src/SaveInfo.cpp')
    enum_names = ('ITEM_CODE', 'MAGIC_CODE', 'MAGIC_LEVEL', 'DROP_TYPE', 'BUTTONS',
                  'QUENCH_AFFIX', 'ITEM_TYPE', 'EQUIPPED_LOCATION', 'ITEM_SPECIALISATION',
                  'AFFIX_EFFECT', 'AFFIX_ORIGIN')
    layout = ''
    for name in enum_names:
        signature = re.search(r'^enum ' + name + r'\b[^\n]*', enums, re.MULTILINE)[0]
        layout += function(enums, signature) + ';\n'
    for name in ('BI_0_GOLD', 'BI_24_POTION_OF_HEALING', 'BI_25_POTION_OF_MANA',
                 'BI_29_POTION_OF_FULL_HEALING', 'BI_30_POTION_OF_FULL_MANA', 'PS_0_NONE'):
        value = re.search(r'\b' + name + r'\s*=\s*(\d+)', enums)[1]
        layout += f'constexpr int {name} = {value};\n'
    for name in ('BaseEffect', 'Effect', 'Item', 'BaseItem', 'FlaskInfo',
                 'ItemInfo19', 'ItemInfo32', 'ItemInfo42', 'ItemInfo64'):
        match = re.search(r'^struct ' + name + r'\b[^\n]*', structs, re.MULTILINE)
        definition = function(structs[match.start():], match[0])
        if name == 'Item':
            definition = definition[:definition.index('\tstatic constexpr StructVersion')] + '};\n'
        else:
            definition += ';\n'
        layout += definition
    layout += 'using LastItemInfo = ItemInfo64;\n'

    # Use the real base definitions, including alternative drop and classic variants.
    bases = []
    for line in read('src/BaseItems.cpp').splitlines():
        if re.match(r'\{\s*\d+,', line) and re.search(r'"Potion of (Full )?(Healing|Mana)"', line):
            index = re.match(r'\{\s*(\d+),', line)[1]
            bases.append(f'BaseItems[{index}] = ' + line.rstrip().rstrip(',') + ';')

    production = '\n'.join(function(items, signature) for signature in (
        'void __fastcall SetPlayerHandItem(', 'void Item::SetSeed(',
        'i64 Item::SavedSeed() const', 'uint __fastcall CalcTotalGold(',
        'bool __fastcall AutoPutCursorToCell(', 'void __fastcall ClearInventorySlot(',
        'void __fastcall ClearBeltSlot(',
        'void __fastcall RecreateItem('))
    production += function(read('src/msg.cpp'), 'void ItemInfo64::operator=(const Item& item)')
    production += function(save, 'void __fastcall SaveItem(')
    production += function(save, 'template<class ItemInfo> void LoadItem(')
    production += function(trade, 'void __fastcall ClearGoldByInventoryAsPrice(')

    enchanting = read('src/Enchanting.cpp')
    for signature in ('void Enchant_DrawItem(', 'int RollEnchantAffix(', 'void ApplyEnchants('):
        enchanting = enchanting.replace(function(enchanting, signature).rstrip(), '')
    enchanting = enchanting.replace('#include "stdafx.h"', '').replace('#include "ItemOverlay.h"', '')

    font = read('src/dataseg.cpp')
    font_tables = '\n'.join(re.search(r'uchar ' + name + r'\s*\[[^\]]+\]\s*=\s*\{.*?\};', font, re.S)[0]
                            for name in ('Codepage_Latin', 'FontIndexSmall_Latin', 'FontWidthSmall'))

    with tempfile.TemporaryDirectory(prefix='dyniamod-potions-') as temporary:
        directory = Path(temporary)
        for name, content in (('potion_layout.inc', layout),
                              ('potion_bases.inc', '\n'.join(bases)),
                              ('potion_font.inc', font_tables),
                              ('potion_items.inc', production),
                              ('potion_enchanting.inc', enchanting)):
            (directory / name).write_text(content)
        binary = directory / 'potion_conversion_test'
        subprocess.run([
            os.environ.get('CXX', 'g++'), '-std=c++17', '-fno-operator-names',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
            '-Wno-parentheses', '-Wno-sign-compare', '-Wno-write-strings', '-Wno-address',
            '-Wno-missing-field-initializers', '-fsanitize=address,undefined',
            '-fno-sanitize-recover=all', '-g', '-I', str(directory),
            str(ROOT / 'tests/potion_conversion_harness.cpp'), '-o', str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
