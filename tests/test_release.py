#!/usr/bin/env python3
"""Exercise release artifacts in an isolated workspace, without uploads or game builds."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location("release", Path(__file__).resolve().parents[1] / "tools/release.py")
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_package_and_manifest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "tools").mkdir()
            (root / "lib").mkdir()
            (root / "README.md").write_text("redistribution readme")
            (root / "tools/update.ps1").write_bytes((release.ROOT / "tools/update.ps1").read_bytes())
            config = {"manifestFileId": "fixture_manifest_123", "extraFiles": [{"source": "resource.dat", "path": "data/resource.dat"}]}
            (root / "tools/release-config.json").write_text(json.dumps(config))
            (root / "resource.dat").write_bytes(b"cumulative resource")
            for name in release.RUNTIME:
                (root / "lib" / name).write_bytes(name.encode())
            # A minimal x86 PE header is sufficient for artifact-format validation.
            pe = bytearray(128)
            pe[:2] = b"MZ"
            struct.pack_into("<I", pe, 60, 64)
            pe[64:68] = b"PE\0\0"
            struct.pack_into("<H", pe, 68, 0x14C)
            executable = root / "DyniaMod.exe"
            executable.write_bytes(pe)
            old_root, old_config = release.ROOT, release.CONFIG
            release.ROOT, release.CONFIG = root, root / "tools/release-config.json"
            try:
                args = SimpleNamespace(executable=str(executable), release_id="20261103T120000000000Z", include=[])
                release.package(args)
                zip_path = root / "updates/DyniaMod-update-20261103T120000000000Z.zip"
                descriptor = zip_path.with_suffix(".release.json")
                info = release.read_json(descriptor)
                with zipfile.ZipFile(zip_path) as archive:
                    self.assertIsNone(archive.testzip())
                    self.assertEqual(archive.read("DyniaMod.exe"), pe)
                    self.assertEqual(archive.read("data/resource.dat"), b"cumulative resource")
                    self.assertIn("README.md", archive.namelist())
                    self.assertTrue(all(not name.endswith("/") for name in archive.namelist()))
                    bat = archive.read("update.bat")
                    self.assertIn(b"fixture_manifest_123", bat)
                    self.assertNotIn(b"__MANIFEST_FILE_ID__", bat)
                    for file in info["files"]:
                        self.assertEqual(release.digest(archive.read(file["path"])), file["sha256"])
                current = root / "updates/previous-latest.json"
                release.write_json(current, {"schema": 1, "app": "DyniaMod", "latest": None})
                manifest_args = SimpleNamespace(descriptor=str(descriptor), file_id="fixture_zip_123", current=str(current), output=root / "updates/latest.json")
                release.manifest(manifest_args)
                published = release.read_json(manifest_args.output)
                self.assertEqual(published["latest"]["sha256"], release.digest(zip_path.read_bytes()))
                release.write_json(current, published)
                with self.assertRaises(ValueError):
                    release.manifest(manifest_args)
                with self.assertRaises(ValueError):
                    release.package(args)
                zip_path.write_bytes(b"corrupted after packaging")
                with self.assertRaises(ValueError):
                    release.manifest(manifest_args)
                args.release_id = "20261103T120001000000Z"
                args.include = [str(root / "resource.dat") + "=config.ini"]
                with self.assertRaises(ValueError):
                    release.package(args)
                self.assertFalse((root / "updates/DyniaMod-update-20261103T120001000000Z.zip").exists())
            finally:
                release.ROOT, release.CONFIG = old_root, old_config

    def test_reject_player_paths_and_non_x86(self):
        for path in ("../a", "/a", "a\\b", "CON.txt", "config.ini", "th4sp_0.ssv", "th4mp_0.msv", "th4_save_mp/hero", "diabdat.mpq", "TH4data.mor", ".dyniamod-update.json", "data/a:stream"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                release.safe_path(path)
        with self.assertRaises(ValueError):
            release.verify_pe(b"not an executable")


if __name__ == "__main__":
    unittest.main()
