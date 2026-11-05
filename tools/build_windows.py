#!/usr/bin/env python3
"""Build the active Win32 project using Microsoft's compiler under portable Wine."""
from pathlib import Path
import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--debug', action='store_true')
parser.add_argument('--clean', action='store_true', help='Recompile all sources, shaders and headers')
parser.add_argument('--prepare', action='store_true', help='Generate shaders and compile the precompiled header only')
parser.add_argument('--check-tools', action='store_true', help='Check compiler, linker and SDK availability')
parser.add_argument('--cache', type=Path, default=Path.home() / '.cache/th4-windows-build')
parser.add_argument('--output', type=Path)
args = parser.parse_args()
ROOT = Path(__file__).resolve().parents[1]
OUT = args.output.resolve() if args.output else ROOT / 'build/windows-release'
CONFIGURATION = 'Debug' if args.debug else 'Release'
if CONFIGURATION == 'Debug' and not args.output:
    OUT = OUT / 'debug'
GEN = OUT / 'generated'
CACHE = args.cache.resolve()
versions = list((CACHE / 'msvc/VC/Tools/MSVC').glob('14.5*'))
if not versions:
    raise SystemExit(f'Missing MSVC v145 (14.5x) installation in {CACHE}; see BUILD.md and doc/updating.md.')
MSVC = max(versions, key=lambda p: tuple(map(int, p.name.split('.'))))
SDK = CACHE / 'msvc/Windows Kits/10'
SDKVER = '10.0.22621.0'
HOST = MSVC / 'bin/Hostx64/x86'
SDKHOST = SDK / 'bin' / SDKVER / 'x64'
WINE = CACHE / 'root/usr/lib/wine/wine64'
OBJ = OUT / 'obj'
LOG = OUT / 'logs'
if args.clean:
    for directory in (OBJ, GEN, LOG):
        if directory.exists():
            shutil.rmtree(directory)
for directory in (OBJ, GEN, LOG):
    directory.mkdir(parents=True, exist_ok=True)

def win(path):
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')

ENV = os.environ.copy()
ENV.update({
    'WINEPREFIX': str(CACHE / 'wineprefix2'),
    'WINEARCH': 'win64',
    'WINEDEBUG': '-all',
    'WINEDLLPATH': str(CACHE / 'root/usr/lib/x86_64-linux-gnu/wine'),
    'LD_LIBRARY_PATH': str(CACHE / 'root/usr/lib/x86_64-linux-gnu'),
    'WINEDLLOVERRIDES': 'mscoree,mshtml=',
    'WINEPATH': ';'.join(map(win, (HOST, SDKHOST, MSVC / 'bin/Hostx64/x64'))),
    'INCLUDE': ';'.join(map(win, (MSVC / 'include', SDK / 'Include' / SDKVER / 'ucrt', SDK / 'Include' / SDKVER / 'shared', SDK / 'Include' / SDKVER / 'um', SDK / 'Include' / SDKVER / 'winrt'))),
    'LIB': ';'.join(map(win, (MSVC / 'lib/x86', SDK / 'Lib' / SDKVER / 'ucrt/x86', SDK / 'Lib' / SDKVER / 'um/x86'))),
})

def run(tool, arguments, label):
    command = [str(WINE), str(tool), *arguments]
    started = time.monotonic()
    result = subprocess.run(command, cwd=ROOT, env=ENV, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode('utf-8', errors='replace').replace('\r', '')
    (LOG / (label + '.log')).write_text(output)
    print(f'{label}: exit={result.returncode} ({time.monotonic() - started:.1f}s)', flush=True)
    if result.returncode:
        print(output[-18000:], flush=True)
        raise RuntimeError(label)
    return output

NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
PROJECT = ET.parse(ROOT / 'thehell3.vcxproj')
PCH = OBJ / 'stdafx.pch'
FLAGS = ['/nologo', '/c', '/O2', '/Ob1', '/MT', '/Zp1', '/Gy', '/GR-', '/Oi', '/EHsc', '/fp:fast', '/GS-', '/W3', '/std:c++17', '/DWIN32', '/D_WINDOWS', '/DNDEBUG', '/D_PROTECTION', '/DOPTIMIZE=1', '/DMSVC_TOOLSET=145', '/DPRJ_NAME="DyniaMod"', '/DTAR_DIR=R"(' + win(OUT) + '\\)"', '/I' + win(ROOT), '/I' + win(ROOT / 'src'), '/I' + win(GEN), '/Fp' + win(PCH)]
if CONFIGURATION == 'Debug':
    FLAGS = [flag for flag in FLAGS if flag not in ['/O2', '/Ob1', '/MT', '/DNDEBUG', '/D_PROTECTION']]
    FLAGS += ['/Od', '/Ob0', '/MTd', '/D_DEBUG', '/Z7', '/RTC1']

def object_path(source):
    return OBJ / (source.replace('\\', '_').replace('/', '_').removesuffix('.cpp') + '.obj')

def compile_source(source, create_pch=False):
    actual = ROOT / source.replace('\\', '/')
    target = object_path(source)
    if not create_pch and target.exists() and target.stat().st_mtime > max(actual.stat().st_mtime, PCH.stat().st_mtime):
        return target
    pch_flag = '/Ycstdafx.h' if create_pch else '/Yustdafx.h'
    run(HOST / 'cl.exe', [*FLAGS, pch_flag, '/Fo' + win(target), win(actual)], Path(source.replace('\\', '/')).stem)
    return target

def main():
    if args.check_tools:
        for path in (WINE, HOST / 'cl.exe', HOST / 'link.exe', SDKHOST / 'fxc.exe', SDKHOST / 'rc.exe'):
            if not path.is_file():
                raise SystemExit(f'Missing build tool: {path}')
        output = run(HOST / 'cl.exe', ['/?'], 'compiler-version')
        print(next((line for line in output.splitlines() if 'Compiler' in line), output.splitlines()[0]))
        print('MSVC v145 and Windows SDK:', MSVC.name, SDKVER)
        return
    if not args.prepare:
        (OUT / 'DyniaMod.exe').unlink(missing_ok=True)
    for shader in PROJECT.findall('.//m:FxCompile[@Include]', NS):
        source = ROOT / shader.get('Include').replace('\\', '/')
        target = GEN / (source.stem + '.h')
        if target.exists() and target.stat().st_mtime > source.stat().st_mtime:
            continue
        entry = shader.find('m:EntryPointName', NS).text
        model = shader.find('m:ShaderModel', NS).text
        shader_type = 'vs_' if shader.find('m:ShaderType', NS).text == 'Vertex' else 'ps_'
        arguments = ['/nologo', '/T', shader_type + model.replace('.', '_'), '/E', entry, '/Fh', win(target)]
        variable = shader.find('m:VariableName', NS)
        if variable is not None:
            arguments += ['/Vn', variable.text]
        run(SDKHOST / 'fxc.exe', [*arguments, win(source)], source.stem)
    sources = [node.get('Include') for node in PROJECT.findall('.//m:ClCompile[@Include]', NS)]
    headers = list((ROOT / 'src').glob('*.h')) + [ROOT / 'tools/version.h']
    if not PCH.exists() or PCH.stat().st_mtime < max(path.stat().st_mtime for path in headers):
        compile_source('src\\stdafx.cpp', True)
    if args.prepare:
        return
    sources = [source for source in sources if source != 'src\\stdafx.cpp']
    failed = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        pending = {executor.submit(compile_source, source): source for source in sources}
        for task in concurrent.futures.as_completed(pending):
            try:
                task.result()
            except Exception:
                failed.append(pending[task])
    if failed:
        (OUT / 'failed-sources.json').write_text(json.dumps(failed, indent=2))
        raise SystemExit(f'Failed C++ sources: {len(failed)}')
    resource = OBJ / 'thehell4.res'
    run(SDKHOST / 'rc.exe', ['/nologo', '/D' + ('NDEBUG' if CONFIGURATION == 'Release' else '_DEBUG'), '/l', '0x419', '/i', win(ROOT / 'src'), '/fo', win(resource), win(ROOT / 'src/thehell4.rc')], 'resources')
    objects = [object_path('src\\stdafx.cpp')] + [object_path(source) for source in sources]
    suffix = '_debug' if CONFIGURATION == 'Debug' else ''
    libraries = ['dbghelp.lib', win(ROOT / 'lib/storm.lib'), win(ROOT / 'lib/dxguid.lib'), win(ROOT / 'lib/ddraw.lib'), 'd3d9.lib', 'Version.lib', 'odbc32.lib', 'odbccp32.lib', win(ROOT / 'lib/SmackW32.LIB'), win(ROOT / ('lib/oggvorbis/libvorbisfile_static' + suffix + '.lib')), win(ROOT / ('lib/oggvorbis/libvorbis_static' + suffix + '.lib')), win(ROOT / ('lib/oggvorbis/libogg_static' + suffix + '.lib')), 'kernel32.lib', 'user32.lib', 'gdi32.lib', 'winspool.lib', 'comdlg32.lib', 'advapi32.lib', 'shell32.lib', 'ole32.lib', 'oleaut32.lib', 'uuid.lib', 'winmm.lib', 'ws2_32.lib', 'delayimp.lib']
    arguments = ['/NOLOGO', '/IGNORE:4099', '/MACHINE:X86', '/SUBSYSTEM:WINDOWS', '/DYNAMICBASE:NO', '/FIXED:NO', '/BASE:0x01400000', '/NXCOMPAT:NO', '/SAFESEH:NO', '/STACK:10485760', '/DELAYLOAD:ddraw.dll', '/DELAYLOAD:xinput9_1_0.dll', '/DEBUG', '/PDB:' + win(OUT / 'DyniaMod.pdb'), '/OUT:' + win(OUT / 'DyniaMod.exe'), *map(win, objects), win(resource), *libraries]
    response = OUT / 'link.rsp'
    response.write_text('\n'.join('"' + argument + '"' for argument in arguments), encoding='utf-8')
    run(HOST / 'link.exe', ['@' + win(response)], 'link')
    print('BUILT:', OUT / 'DyniaMod.exe', flush=True)

if __name__ == '__main__':
    main()
