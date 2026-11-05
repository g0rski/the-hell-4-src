# Repository Guidelines

## Project Structure & Module Organization

This repository contains The Hell 4, a Windows Diablo/Hellfire mod written primarily in C++17.

- `src/`: gameplay, rendering, networking, shared headers, HLSL shaders, and assembly.
- `lib/`: bundled compression, audio, archive libraries, and legacy binaries.
- `res/`: icons, cursors, MPQ archives, and other game resources.
- `tools/`: development utilities; `doc/`: design notes; `bin/`: runtime support files and changelogs.
- `TheHell_4.sln` and `thehell3.vcxproj`: active build definitions. The root `CMakeLists.txt` supports IDE indexing only.

## Build, Test, and Development Commands

Use Windows Visual Studio Build Tools with C++ support, the project's `v145` toolset, and a Windows SDK containing `fxc.exe`. See `BUILD.md` for setup.

- `InitProject.bat`: attempts to initialize local build/debugger settings. Its expected `tools/personal.props` and `tools/thehell4.vcxproj.user` templates are currently absent; create `personal.props` using the settings documented in `BUILD.md`.
- `msbuild TheHell_4.sln /p:Configuration=Release /p:Platform=Win32`: builds `TH4.exe` from a Developer Command Prompt. Substitute `Debug` for debugging.
- `.\build.ps1` or `.\build.ps1 Debug`: PowerShell wrappers; adjust the hardcoded MSBuild path for your installation.

Set `GameFolder` and `FxCopDir` in `personal.props`. Supply `config.ini`, `diabdat.mpq`, and `TH2data.mor` in the output directory, then launch `TH4.exe`.

## Publikowanie zmian w aplikacji

Po wykonaniu zmian w aplikacji zawsze korzystaj ze skilla [`$dyniamod-release`](.agents/skills/dyniamod-release/SKILL.md), aby sprawdzić zmiany, skompilować grę i opublikować aktualizację w konfiguracji **Release** w publicznym katalogu `updates` na Google Drive. Publikację wykonuj w ramach tego samego zadania, bez ponownego pytania o zgodę, zgodnie z procedurą skilla. Po zakończeniu podaj użytkownikowi link do gotowego pliku ZIP.

## Coding Style & Naming Conventions

Match adjacent code: tab indentation, opening function braces on separate lines, and existing spacing. Preserve established PascalCase function/type names, local variable conventions, and uppercase constants. Keep `stdafx.h` first in C++ translation units. Register new files in the Visual Studio project and filters. Preserve structure packing and calling conventions used by legacy binary interfaces. No repository-wide formatter or linter configuration is present; avoid unrelated reformatting.

## Testing Guidelines

There is no configured automated test suite, test naming convention, or coverage threshold. Validate changes with Win32 Debug/Release builds and focused gameplay checks. Exercise save/load for persistence changes and multiplayer sessions for networking changes. Record reproduction steps, configuration, and results in the pull request.

## Commit & Pull Request Guidelines

History mixes plain summaries with `docs:` and `chore:` prefixes. Prefer concise, imperative messages such as `fix: preserve difficulty after loading`. Keep changes focused. PRs should explain the behavior change, link relevant issues, report validation, and include screenshots for visible changes.

## Configuration & Redistribution

Keep machine-specific `personal.props`, debugger settings, and build outputs untracked. Include `README.md` when redistributing source; derivative works must use a name other than “The Hell.”
