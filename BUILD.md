# Building DyniaMod on Windows 11

This workspace uses the official The Hell 4 v0.774 sources dated October 13,
2026, with manual multiplayer item sharing. The result is DyniaMod v0.774.1.
It is a Win32/x86 executable that runs on Windows 11 x64; the legacy libraries
and assembly do not provide a native x64 target.

## Prerequisites

Install Visual Studio Build Tools with Desktop development with C++, the
**v145** x86/x64 toolset, and a Windows SDK containing **fxc.exe**. The active
build definitions are `TheHell_4.sln` and `thehell3.vcxproj`. CMake is for IDE
indexing only.

## Configure and build

From the repository root, run:

```bat
InitProject.bat
```

This creates `personal.props` and `thehell3.vcxproj.user` from portable example
files under `tools/`, without overwriting existing local settings. Both generated
files remain untracked. Edit `personal.props` if needed:

- `GameFolder`: output directory, ending in a backslash; defaults to `build/`.
- `FxCopDir`: directory containing `fxc.exe`; defaults to the selected Windows
  SDK's x86 tools. An explicit example is
  `C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86`.

In a Developer Command Prompt for Visual Studio Build Tools:

```bat
msbuild TheHell_4.sln /p:Configuration=Release /p:Platform=Win32
msbuild TheHell_4.sln /p:Configuration=Debug /p:Platform=Win32
```

The output is `DyniaMod.exe`. The existing PowerShell wrapper `build.ps1`
also works after adjusting its MSBuild path for the local installation.

The official source archive omits `res/credits.txt` and `res/support.txt`, which
its resource script still requires. These two text resources are retained from
the previous repository snapshot so the source can compile.

## Run and verify

Use a separate copy of the game installation and saves. The output directory
needs the matching game's `config.ini`, `diabdat.mpq`, `TH4data.mor` and runtime
DLL/network support files. Game data archives are not supplied by this source
repository. Launch `DyniaMod.exe`; all multiplayer peers need the same build.

The save format stays at version **30**. Production decoder/reader tests accept
the supplied v0.778 hero record; full Windows gameplay and live multiplayer
verification are still required. The base game code is v0.774, so this build
does not include later official v0.775–v0.778 fixes.

Portable protocol and save-reader checks need Python 3 and GCC with ASan/UBSan:

```sh
python3 tools/test_shared_items.py
python3 tools/test_save_compatibility.py
python3 tools/test_potion_conversion.py
python3 tools/test_inventory_sort.py
```

See [the detailed implementation and gameplay checks](doc/multiplayer-item-sharing.md).
Adria's conversion behavior and focused checks are documented in
[potion conversion](doc/potion-conversion.md).
The inventory sort button and manual checks are documented in
[inventory sorting](doc/inventory-sorting.md).
