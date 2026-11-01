#!/usr/bin/env python3
"""Exercise the real save reader/codec and hero creation against reserved slots."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import argparse

from test_shared_items import function

ROOT = Path(__file__).resolve().parents[1]


def structure(source, name):
    start = re.search(r'^struct ' + re.escape(name) + r'(?:\s|:)', source, re.MULTILINE).start()
    return function(source[start:], source[start:source.index('{', start)].rstrip()) + ';\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hero-record', type=Path, help='Optional encoded mphero member to verify read-only')
    args = parser.parse_args()
    structs = (ROOT / 'src/structs.h').read_text(encoding='utf-8-sig')
    slots = (ROOT / 'src/CharSlots.cpp').read_text(encoding='utf-8-sig')
    engine = (ROOT / 'src/engine.h').read_text(encoding='utf-8-sig')
    enums = (ROOT / 'src/enums.h').read_text(encoding='utf-8-sig', errors='replace')
    layout = 'constexpr unsigned POWERUP_COUNT = ' + re.search(r'#define POWERUP_COUNT\s+(\d+)', structs)[1] + ';\n'
    layout += '\n'.join(structure(structs, name) for name in (
        'ItemInfo19', 'ItemInfo32', 'ItemInfo42', 'ItemInfo64',
        'PlayerInfo', 'PlayerInfo_v18', 'PlayerInfo_v21', 'PlayerInfo_v22',
        'PlayerInfo_v26', 'AppendRec', 'KeyRec', 'ShaInfo'))
    layout += 'using LastPlayerInfo = PlayerInfo_v26;\n'
    layout += 'constexpr unsigned CurSaveVersion = ' + re.search(r'CurSaveVersion = (\d+)', engine)[1] + ';\n'
    layout += 'constexpr unsigned KEY_SIZE = ' + re.search(r'KEY_SIZE\s*=\s*(\d+)', enums)[1] + ';\n'
    crypto = (ROOT / 'src/SHA.cpp').read_text(encoding='utf-8-sig').replace('#include "stdafx.h"', '', 1)
    crypto += (ROOT / 'src/CodeSave.cpp').read_text(encoding='utf-8-sig').replace('#include "stdafx.h"', '', 1)
    production = '\n'.join(function(slots, signature) for signature in (
        'static bool IsSupportedPlayerInfo(', 'uchar* __fastcall ReadPlayerInfo(',
        'static bool IsUnusedSaveSlot(', 'int __stdcall UiCreateHero('))
    with tempfile.TemporaryDirectory(prefix='th4-save-compatibility-') as temporary:
        directory = Path(temporary)
        for name, content in (('save_layout.inc', layout), ('save_crypto.inc', crypto), ('save_under_test.inc', production)):
            (directory / name).write_text(content)
        binary = directory / 'save_compatibility_test'
        subprocess.run([
            os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-parentheses',
            '-Wno-sign-compare', '-fwrapv', '-fsanitize=address,undefined',
            '-fno-sanitize=shift', '-fno-sanitize-recover=all', '-g',
            '-I', str(directory), str(ROOT / 'tests/save_compatibility_harness.cpp'),
            '-o', str(binary),
        ], check=True)
        arguments = [str(binary), str(directory / 'saves')]
        if args.hero_record:
            arguments.append(str(args.hero_record.resolve()))
        subprocess.run(arguments, check=True)


if __name__ == '__main__':
    main()
