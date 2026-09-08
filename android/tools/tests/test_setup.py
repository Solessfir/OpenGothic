import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import game_package as assets
import setup_android as setup


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.game = self.base / "Gothic II"
        for folder in ("Data", "_work/Data", "system", "Saves"):
            (self.game / folder).mkdir(parents=True)
        for name in ("Worlds.vdf", "Worlds_Addon.vdf"):
            (self.game / "Data" / name).write_bytes(b"test data")
        (self.game / "system/Gothic.ini").write_text("[VIDEO]\nzVidResFullscreenX=9999\n[GAME]\nusePotionKeys=1\nmouseSensitivity=0.7\nuseQuickSaveKeys=0\n[ENGINE]\nzMaxFPS=144\n[SOUND]\nmusicVolume=0.4\n")
        (self.game / "system/SystemPack.ini").write_text("[INTERFACE]\nScale=0.01\n")
        (self.game / "system/Gothic2.exe").write_bytes(b"not an asset")
        (self.game / "Saves/user.sav").write_bytes(b"private save")

    def test_enter_defaults_and_invalid_answer(self):
        with patch("builtins.input", side_effect=["", "", "bad", "n"]):
            self.assertTrue(setup.ask("Yes"))
            self.assertFalse(setup.ask("No", False))
            self.assertFalse(setup.ask("Retry"))

    def test_build_variants_and_release_default(self):
        for build_type in ("release", "debug"):
            for bundled in (False, True):
                with self.subTest(build_type=build_type, bundled=bundled):
                    outputs = self.base / "build/android/OpenGothic/app/build/outputs"
                    apk = outputs / f"apk/{build_type}/app-{build_type}.apk"
                    apk.parent.mkdir(parents=True, exist_ok=True)
                    with zipfile.ZipFile(apk, "w") as archive:
                        archive.writestr("AndroidManifest.xml", b"manifest")
                        archive.writestr("lib/arm64-v8a/libopengothic.so", b"library")
                        if bundled:
                            archive.writestr("assets/private-game-00000.ogpart", b"private")
                    for name in ("native-debug-symbols/release/native-debug-symbols.zip", "mapping/release/mapping.txt"):
                        target = outputs / name
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(b"symbols")
                    output = self.base / "result"
                    output.mkdir(exist_ok=True)
                    def run(command, **kwargs):
                        result = "application-debuggable" if "badging" in command and build_type == "debug" else ""
                        return subprocess.CompletedProcess(command, 0, result, "")
                    with patch.object(setup, "ROOT", self.base), patch.object(setup, "OUTPUT", output), \
                         patch.object(setup, "run", side_effect=run) as calls, patch.object(setup, "stage_chunks") as stage:
                        args = {} if build_type == "release" else {"build_type": "debug"}
                        destination = setup.build(self.base / "sdk", {}, bundled, **args)
                    variant = build_type.capitalize()
                    self.assertIn(f"-DTEMPEST_ANDROID_BUILD_TYPE={variant}", calls.call_args_list[0].args[0])
                    self.assertIn(f"assemble{variant}", calls.call_args_list[1].args[0])
                    self.assertIn(f"lint{variant}", calls.call_args_list[1].args[0])
                    self.assertEqual(stage.call_count, int(bundled))
                    self.assertEqual("-debug-" in destination.name, build_type == "debug")
                    self.assertEqual("-with-data-" in destination.name, bundled)
                    report = json.loads((output / "build-report.json").read_text())
                    self.assertEqual(report["build_type"], build_type)
                    self.assertEqual(report["debuggable"], build_type == "debug")
                    self.assertEqual(len(report["debug_artifacts"]), 2 if build_type == "release" else 0)

    def test_build_rejects_unknown_variant(self):
        with self.assertRaisesRegex(ValueError, "Build type"):
            setup.build(self.base, {}, False, "relase")

    def test_safe_ini(self):
        result = assets.safe_preferences(self.game / "system/Gothic.ini").decode()
        self.assertIn("usePotionKeys=1", result)
        self.assertIn("useQuickSaveKeys=0", result)
        self.assertIn("mouseSensitivity=0.7", result)
        self.assertNotIn("VIDEO", result)
        self.assertNotIn("ENGINE", result)

    def test_assets_exclude_executables_settings_saves(self):
        (self.game / "Data/evil.dll").write_bytes(b"dll")
        output = self.base / "game-data.zip"
        metadata = assets.package(self.game, output, progress=lambda _: None)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.namelist(), [assets.INDEX, "Gothic2/Data/Worlds.vdf", "Gothic2/Data/Worlds_Addon.vdf"])
            for line in archive.read(assets.INDEX).decode().splitlines():
                size, digest, name = line.split("\t")
                content = archive.read(name)
                self.assertEqual(int(size), len(content))
                self.assertEqual(digest, assets.hashlib.sha256(content).hexdigest())
        self.assertEqual(metadata["entries"], 2)

    def test_explicit_save_and_preferences(self):
        save = self.base / "save_slot_2.sav"
        with zipfile.ZipFile(save, "w") as archive:
            archive.writestr("header", b"OpenGothic/Save\x00" + b"\x37\x00")
        output = self.base / "game-data.zip"
        assets.package(self.game, output, b"[GAME]\nusePotionKeys=1\n", [save], lambda _: None)
        with zipfile.ZipFile(output) as archive:
            self.assertIn("save_slot_2.sav", archive.namelist())
            self.assertIn("Gothic.ini", archive.namelist())
        save.write_bytes(b"original Gothic save")
        with self.assertRaises(ValueError):
            assets.validate_save(save)

    def test_missing_addon(self):
        (self.game / "Data/Worlds_Addon.vdf").unlink()
        with self.assertRaisesRegex(ValueError, "Night of the Raven"):
            assets.validate_game(self.game)

    def test_bundled_chunks_use_game_data_archive(self):
        content = b"game archive"
        (self.base / "game-data.zip").write_bytes(content)
        with patch.object(setup, "OUTPUT", self.base):
            setup.stage_chunks()
        self.assertEqual((self.base / "assets/private-game-00000.ogpart").read_bytes(), content)

    def test_custom_steam_library(self):
        (self.base / "steamapps").mkdir()
        (self.base / "steamapps/libraryfolders.vdf").write_text('"libraryfolders" { "1" { "path" "D:\\\\SteamLibrary" } }')
        result = setup.steam_libraries(self.base)
        self.assertEqual(str(result[1]), "D:\\SteamLibrary" if setup.WINDOWS else "/mnt/d/SteamLibrary")

    def test_download_cache_checksum(self):
        cached = self.base / "download.zip"
        cached.write_bytes(b"verified data")
        spec = {"url": "https://example.invalid/download.zip", "sha256": assets.sha256(cached)}
        with patch("urllib.request.urlopen", side_effect=AssertionError("Should use verified cache")):
            self.assertEqual(setup.download(spec, self.base), cached)

    def test_unpack_rejects_traversal(self):
        archive = self.base / "tool.zip"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("../escaped.txt", "bad")
        with self.assertRaisesRegex(RuntimeError, "Unsafe path"):
            setup.unpack(archive, self.base / "tool")
        self.assertFalse((self.base / "escaped.txt").exists())

    def test_apk_mode_guard(self):
        apk = self.base / "test.apk"
        with zipfile.ZipFile(apk, "w") as archive:
            archive.writestr("AndroidManifest.xml", "test")
            archive.writestr("lib/arm64-v8a/libopengothic.so", "test")
        setup.inspect_apk(apk, False)
        with self.assertRaisesRegex(RuntimeError, "game-data"):
            setup.inspect_apk(apk, True)

    def test_apk_rejects_orphaned_private_bytes(self):
        apk = self.base / "orphaned.apk"
        with apk.open("wb") as out:
            out.write(b"private bytes" * 100000)
        with zipfile.ZipFile(apk, "a") as archive:
            archive.writestr("AndroidManifest.xml", "test")
            archive.writestr("lib/arm64-v8a/libopengothic.so", "test")
        with self.assertRaisesRegex(RuntimeError, "unreferenced"):
            setup.inspect_apk(apk, False)

    def test_install_retries_same_device_without_uninstall(self):
        apk = self.base / "test.apk"
        apk.write_bytes(b"test")
        failure = subprocess.CompletedProcess([], 1, "", "device offline")
        success = subprocess.CompletedProcess([], 0, "Success", "")
        with patch.object(setup, "run", side_effect=[failure, success, success]) as run, patch.object(setup, "ask", return_value=True):
            setup.install_apk(["adb", "-s", "phone-one"], apk)
        self.assertEqual(run.call_args_list[0].args[0], ["adb", "-s", "phone-one", "install", "--no-streaming", "-r", apk])
        self.assertEqual(run.call_args_list[0], run.call_args_list[2])
        self.assertNotIn("uninstall", str(run.call_args_list))

    @unittest.skipUnless(os.environ.get("JAVA_HOME"), "Set JAVA_HOME to run host extraction checks")
    def test_java_extractor(self):
        java = Path(os.environ["JAVA_HOME"]) / "bin"
        suffix = ".exe" if os.name == "nt" else ""
        source = setup.ROOT / "android/app/src/main/java/org/opengothic/app/PrivateAssets.java"
        test = Path(__file__).with_name("PrivateAssetsTest.java")
        subprocess.run([str(java / ("javac" + suffix)), "-d", str(self.base), str(source), str(test)], check=True)
        subprocess.run([str(java / ("java" + suffix)), "-cp", str(self.base), "org.opengothic.app.PrivateAssetsTest", str(self.base / "extraction")], check=True)


if __name__ == "__main__":
    unittest.main()
