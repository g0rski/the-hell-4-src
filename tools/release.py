#!/usr/bin/env python3
"""Stage DyniaMod updates and render the standalone Windows updater. No upload side effects."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "tools/release-config.json"
RUNTIME = ("Strm2.dll", "Storm.dll", "SmackW32.DLL", "Standard.sn2")
RELEASE_ID = re.compile(r"\d{8}T\d{12}Z\Z")
RESERVED = re.compile(r"(?:CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)", re.I)
PLAYER_DIR = re.compile(r"(?:save.*|.*_save.*|characters?|players?|screenshots?|config|settings|\.dyniamod.*|updates?)\Z", re.I)
PLAYER_EXT = {".ini", ".cfg", ".sv", ".hsv", ".ssv", ".msv", ".csv", ".ksv", ".osv", ".drv", ".dsv", ".bak", ".key", ".mpq", ".mor"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, data):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def safe_path(path):
    parts = path.split("/")
    if (not path or len(path) > 200 or re.search(r'[\\:<>"|?*\x00-\x1f]', path)
            or any(not p or p in (".", "..") or p.endswith((".", " ")) or RESERVED.match(p) for p in parts)
            or PLAYER_DIR.fullmatch(parts[0]) or PurePosixPath(path).suffix.lower() in PLAYER_EXT):
        raise ValueError(f"Unsafe or player-owned update path: {path}")
    return path


def verify_pe(data):
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("DyniaMod.exe is not a Windows PE executable")
    offset = struct.unpack_from("<I", data, 60)[0]
    if offset + 24 > len(data) or data[offset:offset + 4] != b"PE\0\0" or struct.unpack_from("<H", data, offset + 4)[0] != 0x14C:
        raise ValueError("DyniaMod.exe must be a Win32/x86 PE executable")


def updater_bytes(config):
    file_id = config["manifestFileId"]
    if not re.fullmatch(r"[A-Za-z0-9_-]{10,200}", file_id):
        raise ValueError("Invalid manifestFileId")
    script = (ROOT / "tools/update.ps1").read_text(encoding="utf-8").replace("__MANIFEST_FILE_ID__", file_id)
    # %~f0 travels through the environment, so spaces, &, parentheses and apostrophes in paths stay literal.
    # The complete batch tail is parsed before PowerShell starts: self-update cannot
    # move cmd.exe's next instruction into the newly written embedded script.
    batch = '''@echo off
setlocal DisableDelayedExpansion
set "DYNIA_UPDATE_SELF=%~f0"
(
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$t=[IO.File]::ReadAllText($env:DYNIA_UPDATE_SELF); $m='# DYNIA_POWERSHELL' + '_START'; & ([scriptblock]::Create($t.Substring($t.IndexOf($m) + $m.Length)))"
if errorlevel 1 (
echo.
pause
exit /b 1
)
echo.
pause
exit /b 0
)
# DYNIA_POWERSHELL_START
'''
    return (batch + script).replace("\n", "\r\n").encode("ascii")


def render(args):
    target = Path(args.output)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(updater_bytes(read_json(CONFIG)))
    print(target)


def package(args):
    release_id = args.release_id or datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    if not RELEASE_ID.fullmatch(release_id):
        raise ValueError("release-id must be a UTC timestamp such as 20261103T120000000000Z")
    out = ROOT / "updates"
    out.mkdir(exist_ok=True)
    target = out / f"DyniaMod-update-{release_id}.zip"
    descriptor = target.with_suffix(".release.json")
    if target.exists() or descriptor.exists():
        raise ValueError(f"Release already staged: {release_id}; never reuse a release ID")
    payload = {"DyniaMod.exe": Path(args.executable).read_bytes(), "README.md": (ROOT / "README.md").read_bytes()}
    verify_pe(payload["DyniaMod.exe"])
    for name in RUNTIME:
        payload[name] = (ROOT / "lib" / name).read_bytes()
    config = read_json(CONFIG)
    payload["update.bat"] = updater_bytes(config)
    includes = [str(ROOT / item["source"]) + "=" + item["path"] for item in config.get("extraFiles", [])]
    for extra in includes + args.include:
        source, separator, destination = extra.partition("=")
        if not separator:
            raise ValueError("--include expects SOURCE=GAME_RELATIVE_PATH")
        safe_path(destination)
        if destination.casefold() in {p.casefold() for p in payload}:
            raise ValueError(f"Duplicate payload destination: {destination}")
        payload[destination] = Path(source).read_bytes()
    for path in payload:
        safe_path(path)
    files = [{"path": path, "sha256": digest(data), "size": len(data)} for path, data in sorted(payload.items())]
    # Validate everything before creating a publishable ZIP. No enclosing directory.
    try:
        with zipfile.ZipFile(target, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path, data in sorted(payload.items()):
                archive.writestr(path, data)
        with zipfile.ZipFile(target) as archive:
            if archive.testzip() is not None:
                raise ValueError("ZIP verification failed")
        data = target.read_bytes()
        write_json(descriptor, {"releaseId": release_id, "sha256": digest(data), "size": len(data), "files": files})
    except BaseException:
        target.unlink(missing_ok=True)
        descriptor.unlink(missing_ok=True)
        raise
    print(json.dumps({"zip": str(target), "descriptor": str(descriptor), "releaseId": release_id}, indent=2))


def manifest(args):
    if not re.fullmatch(r"[A-Za-z0-9_-]{10,200}", args.file_id):
        raise ValueError("Invalid Drive ZIP file ID")
    release = read_json(args.descriptor)
    if not RELEASE_ID.fullmatch(release["releaseId"]):
        raise ValueError("Invalid release descriptor")
    zip_path = Path(args.descriptor).with_suffix("").with_suffix(".zip")
    # *.release.json -> *.zip; reject descriptors that no longer match their staged archive.
    data = zip_path.read_bytes()
    if digest(data) != release["sha256"] or len(data) != release["size"]:
        raise ValueError("Staged ZIP changed after packaging")
    for file in release["files"]:
        safe_path(file["path"])
    if args.current:
        old = read_json(args.current).get("latest")
        if old and old["releaseId"] >= release["releaseId"]:
            raise ValueError("New release ID must be newer than the published release")
    release["fileId"] = args.file_id
    write_json(args.output, {"schema": 1, "app": "DyniaMod", "latest": release})
    print(args.output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("render-updater")
    p.add_argument("--output", default=str(ROOT / "update.bat"))
    p.set_defaults(run=render)
    p = sub.add_parser("package")
    p.add_argument("--executable", required=True)
    p.add_argument("--release-id")
    p.add_argument("--include", action="append", default=[], metavar="SOURCE=GAME_PATH")
    p.set_defaults(run=package)
    p = sub.add_parser("manifest")
    p.add_argument("--descriptor", required=True)
    p.add_argument("--file-id", required=True)
    p.add_argument("--current", required=True, help="Downloaded, currently published latest.json")
    p.add_argument("--output", default=str(ROOT / "updates/latest.json"))
    p.set_defaults(run=manifest)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
