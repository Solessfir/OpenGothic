#!/usr/bin/env python3
"""Interactive Android build and installation. Uses Python's standard library only."""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
import zipfile

from game_package import child_ci, game_edition, package, safe_preferences, sha256, unsupported_plugins, validate_game, validate_save

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "build/android-setup"
WINDOWS = os.name == "nt"
HOST = "windows" if WINDOWS else "linux"
APP = "org.opengothic.gothic2notr"
EDITIONS = {"gothic1": "org.opengothic.gothic1", "gothic2": "org.opengothic.gothic2", "gothic2notr": APP}
EDITION_NAMES = {"gothic1": "Gothic 1", "gothic2": "Gothic II Classic", "gothic2notr": "Gothic II: Night of the Raven"}
BUILD_DIRS = {"gothic1": "build/android-g1", "gothic2": "build/android-g2-classic", "gothic2notr": "build/android"}
APK_NAMES = {"gothic1": "OpenGothic-Gothic1", "gothic2": "OpenGothic-Gothic2-Classic", "gothic2notr": "OpenGothic-Gothic2-NotR"}
EDITIONS["archolos"] = "org.opengothic.archolos"
EDITION_NAMES["archolos"] = "The Chronicles of Myrtana: Archolos"
BUILD_DIRS["archolos"] = "build/android-archolos"
APK_NAMES["archolos"] = "OpenGothic-Archolos"
LOCK = json.loads(Path(__file__).with_name("toolchain.json").read_text())


def ask(question, default=True):
    suffix = "[Y/n]" if default else "[y/N]"
    while True:
        answer = input(f"{question} {suffix} ").strip().lower()
        if not answer:
            return default
        if answer in ("y", "yes", "n", "no"):
            return answer in ("y", "yes")
        print("Enter Y or N; Enter uses the capitalized choice.")


def choose(values, label=str):
    if len(values) == 1:
        print(label(values[0]))
        return values[0]
    for index, value in enumerate(values, 1):
        print(f"  {index}. {label(value)}")
    while True:
        answer = input("Choose a number [1]: ").strip() or "1"
        if answer.isdigit() and 1 <= int(answer) <= len(values):
            return values[int(answer) - 1]


def run(command, *, env=None, capture=False, check=True):
    command = [str(arg) for arg in command]
    print("\n> " + subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                            encoding="utf-8", errors="replace", capture_output=capture)
    if check and result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command[0]}\n" +
                           ((result.stdout or "") + (result.stderr or "") if capture else "See output above."))
    return result


def download(spec, cache):
    """Resume partial transfers, but never use a file before its pinned SHA-256 matches."""
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / spec["url"].rsplit("/", 1)[1]
    if path.is_file() and sha256(path) == spec["sha256"]:
        print(f"Verified cached download: {path.name}")
        return path
    partial = path.with_suffix(path.suffix + ".part")
    if partial.is_file() and sha256(partial) == spec["sha256"]:
        os.replace(partial, path)
        return path
    start = partial.stat().st_size if partial.exists() else 0
    request = urllib.request.Request(spec["url"], headers={"User-Agent": "OpenGothic-Android-setup"})
    if start:
        request.add_header("Range", f"bytes={start}-")
    print(f"Downloading {spec['url']}")
    with urllib.request.urlopen(request, timeout=60) as response:
        resume = response.status == 206 and response.headers.get("Content-Range", "").startswith(f"bytes {start}-")
        if response.status == 206 and not resume:
            raise RuntimeError("Server returned an unexpected partial download")
        with partial.open("ab" if resume else "wb") as target:
            transferred = start if resume else 0
            last = time.monotonic()
            while block := response.read(1024 * 1024):
                target.write(block)
                transferred += len(block)
                if time.monotonic() - last > 5:
                    print(f"  {transferred // (1024 * 1024)} MiB downloaded", flush=True)
                    last = time.monotonic()
    if sha256(partial) != spec["sha256"]:
        partial.unlink()
        raise RuntimeError("Download checksum mismatch. The untrusted download was removed; rerun to retry.")
    os.replace(partial, path)
    return path


def unpack(archive, destination):
    destination = Path(destination)
    if destination.exists():
        raise RuntimeError(f"Refusing to replace an existing tool directory: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tool-extract-", dir=destination.parent) as temporary:
        stage = Path(temporary)
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as source:
                for entry in source.infolist():
                    target = (stage / entry.filename).resolve()
                    if not target.is_relative_to(stage.resolve()) or "\\" in entry.filename:
                        raise RuntimeError("Unsafe path in downloaded tool archive")
                    source.extract(entry, stage)
                    if not WINDOWS and entry.external_attr >> 16:
                        target.chmod((entry.external_attr >> 16) & 0o777)
        else:
            with tarfile.open(archive) as source:
                if not hasattr(tarfile, "data_filter"):
                    raise RuntimeError("Update Python to a security-patched version with tarfile.data_filter, then rerun.")
                source.extractall(stage, filter="data")
        stage.rename(destination)


def java17(path):
    executable = Path(path) / "bin" / ("java.exe" if WINDOWS else "java")
    if not executable.is_file():
        return False
    result = subprocess.run([str(executable), "-version"], capture_output=True, text=True)
    return result.returncode == 0 and bool(re.search(r'version "17[.\"]', result.stderr + result.stdout))


def toolchain(args):
    cache = Path(args.cache).expanduser().resolve()
    cache.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    candidates = [Path(args.jdk)] if args.jdk else []
    if env.get("JAVA_HOME"):
        candidates.append(Path(env["JAVA_HOME"]))
    java = shutil.which("java")
    if java:
        candidates.append(Path(java).resolve().parent.parent)
    candidates += list((cache / "jdk").glob("jdk*"))
    if WINDOWS:
        for base in (Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Eclipse Adoptium",
                     Path("C:/Android/jdk17")):
            candidates += list(base.glob("jdk*"))
    else:
        candidates += list(Path("/usr/lib/jvm").glob("*17*"))
    jdk = next((p.resolve() for p in candidates if java17(p)), None)
    if args.jdk and (not jdk or jdk != Path(args.jdk).resolve()):
        raise RuntimeError("--jdk must point to a working JDK 17 directory")
    if not jdk:
        if not ask("Download and install portable Temurin JDK 17.0.20.1 into the local tool cache?"):
            raise RuntimeError("JDK 17 is required. Rerun with --jdk PATH to use an existing installation.")
        unpack(download(LOCK["jdk"][HOST], cache / "downloads"), cache / "jdk")
        jdk = next((p for p in (cache / "jdk").iterdir() if java17(p)), None)
        if not jdk:
            raise RuntimeError("Downloaded JDK cannot run on this host. Linux requires x86-64 glibc.")
    env["JAVA_HOME"] = str(jdk)
    env["PATH"] = str(jdk / "bin") + os.pathsep + env.get("PATH", "")
    sdk_candidates = [Path(args.sdk)] if args.sdk else []
    if not args.sdk:
        sdk_candidates += [Path(env[name]) for name in ("ANDROID_HOME", "ANDROID_SDK_ROOT") if env.get(name)]
        sdk_candidates += ([Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk", Path("C:/Android/Sdk")]
                           if WINDOWS else [Path.home() / "Android/Sdk"])
    sdk = next((p.resolve() for p in sdk_candidates if p.is_dir()), cache / "sdk")
    if args.sdk:
        sdk = Path(args.sdk).expanduser().resolve()
    sdk.mkdir(parents=True, exist_ok=True)
    other_adb = sdk / "platform-tools" / ("adb" if WINDOWS else "adb.exe")
    host_adb = sdk / "platform-tools" / ("adb.exe" if WINDOWS else "adb")
    if other_adb.is_file() and not host_adb.is_file():
        raise RuntimeError("This SDK belongs to the other operating system. Windows and Linux/WSL need separate SDK directories; choose another --sdk path.")
    env["ANDROID_HOME"] = env["ANDROID_SDK_ROOT"] = str(sdk)
    print(f"JDK: {jdk}\nAndroid SDK: {sdk}")
    missing = [name for name in LOCK["packages"] if not (sdk / name.replace(";", "/") / "source.properties").is_file()]
    if missing:
        print("Missing Android packages: " + ", ".join(missing))
        if not ask("Install these Android packages? Downloads require several GB and SDK license acceptance"):
            raise RuntimeError("Android dependencies are required to build. Rerun when ready.")
        # Keep the sdkmanager version compatible with this project's JDK 17.
        command_tools = sdk / "cmdline-tools/12.0"
        manager = command_tools / "bin" / ("sdkmanager.bat" if WINDOWS else "sdkmanager")
        if not manager.is_file():
            print("Android SDK license: https://developer.android.com/studio/terms")
            if not ask("Have you read and accepted the Android SDK license so this script may download command-line tools 12.0?"):
                raise RuntimeError("SDK download cancelled; license was not accepted.")
            archive = download(LOCK["command_line_tools"][HOST], cache / "downloads")
            with tempfile.TemporaryDirectory(prefix="sdk-tools-", dir=cache) as temporary:
                unpack(archive, Path(temporary) / "unpacked")
                command_tools.parent.mkdir(parents=True, exist_ok=True)
                if command_tools.exists():
                    raise RuntimeError(f"Incomplete tools at {command_tools}; move that directory aside and retry.")
                shutil.move(str(Path(temporary) / "unpacked/cmdline-tools"), command_tools)
        print("The SDK manager will now display its licenses. Answer its own prompts to accept or decline.")
        run([manager, f"--sdk_root={sdk}", "--licenses"], env=env)
        run([manager, f"--sdk_root={sdk}", *missing], env=env)
        missing = [name for name in LOCK["packages"] if not (sdk / name.replace(";", "/") / "source.properties").is_file()]
        if missing:
            raise RuntimeError("SDK packages still missing: " + ", ".join(missing))
    # An existing local.properties overrides ANDROID_HOME in Gradle. Never rewrite it silently.
    build_dir = BUILD_DIRS[getattr(args, "edition", None) or "gothic2notr"]
    properties = ROOT / build_dir / "OpenGothic/local.properties"
    if properties.exists():
        match = re.search(r"^sdk\.dir\s*=\s*(.+)$", properties.read_text(), re.MULTILINE)
        if match:
            configured = match[1].strip().replace("\\:", ":").replace("\\\\", "\\")
            if Path(configured).resolve() != sdk.resolve():
                raise RuntimeError(f"{properties} selects a different SDK ({configured}). Use --sdk with that path or edit the file yourself.")
    return sdk, env


def steam_libraries(steam):
    libraries = [steam]
    file = steam / "steamapps/libraryfolders.vdf"
    if file.is_file():
        paths = re.findall(r'"path"\s*"([^"]+)"', file.read_text(encoding="utf-8", errors="replace"))
        for value in paths:
            value = value.replace("\\\\", "\\")
            if not WINDOWS and re.match(r"^[A-Za-z]:\\", value):
                value = "/mnt/" + value[0].lower() + "/" + value[3:].replace("\\", "/")
            libraries.append(Path(value))
    return libraries


def discover_games():
    roots = []
    steam = [Path.home() / ".steam/steam", Path.home() / ".local/share/Steam",
             Path.home() / ".var/app/com.valvesoftware.Steam/.local/share/Steam"]
    if WINDOWS:
        import winreg
        for hive, key, name in ((winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
                                (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath")):
            try:
                with winreg.OpenKey(hive, key) as handle:
                    steam.append(Path(winreg.QueryValueEx(handle, name)[0]))
            except OSError:
                pass
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\GOG.com\Games") as key:
                for index in range(winreg.QueryInfoKey(key)[0]):
                    with winreg.OpenKey(key, winreg.EnumKey(key, index)) as game:
                        try:
                            roots.append(Path(winreg.QueryValueEx(game, "path")[0]))
                        except OSError:
                            pass
        except OSError:
            pass
        drives = [Path(f"{letter}:/") for letter in "CDEFGH" if Path(f"{letter}:/").is_dir()]
    else:
        drives = [p for p in Path("/mnt").glob("[a-z]") if p.is_dir()]
    for drive in drives:
        steam += [drive / "Program Files (x86)/Steam", drive / "Steam", drive / "SteamLibrary"]
        roots += [drive / name for name in ("Games", "GOG Games", "Program Files (x86)/GOG Galaxy/Games")]
    roots += [Path.home() / "Games", Path.home() / "GOG Games"]
    for location in steam:
        for library in steam_libraries(location):
            roots.append(library / "steamapps/common")
    candidates = []
    for root in roots:
        if not root.is_dir():
            continue
        candidates.append(root)
        candidates += [p for p in root.iterdir() if p.is_dir() and any(name in p.name.lower() for name in ("gothic", "myrtana", "archolos"))]
    found = set()
    for candidate in candidates:
        try:
            found.add(validate_game(candidate))
        except (ValueError, OSError):
            pass
    return sorted(found, key=str)


def select_game(explicit, edition=None):
    if explicit:
        game = validate_game(explicit)
        if edition and game_edition(game) != EDITION_NAMES[edition]:
            raise ValueError("--edition does not match the selected installation. Use that edition's actual files, not renamed or partially deleted addon files.")
        return game
    print("\nSearching Steam libraries, GOG and common Games directories...")
    found = discover_games()
    if edition is None:
        print("Which game do you want to install?")
        edition = choose(list(EDITIONS), lambda item: EDITION_NAMES[item] + (" (found)" if any(game_edition(p) == EDITION_NAMES[item] for p in found) else " (choose its installation folder)"))
    found = [p for p in found if game_edition(p) == EDITION_NAMES[edition]]
    if found and ask("Use a discovered installation?"):
        return choose(found, lambda path: f"{game_edition(path)}: {path}")
    while True:
        try:
            return select_game(input(f"{EDITION_NAMES[edition]} installation path: ").strip().strip('"'), edition)
        except (ValueError, OSError) as error:
            print(error)


def source_ready():
    if not shutil.which("git"):
        raise RuntimeError("Git is missing from PATH. Install Git, open a new terminal, and rerun this guide.")
    status = run(["git", "submodule", "status", "--recursive"], capture=True).stdout
    if any(line.startswith(("+", "U")) for line in status.splitlines()):
        raise RuntimeError("A submodule differs from the pinned revision or has conflicts. Resolve it yourself; setup will not reset your work.\n" + status)
    if any(line.startswith("-") for line in status.splitlines()):
        if not ask("Download the repository's pinned submodules?"):
            raise RuntimeError("Submodules are required to compile.")
        run(["git", "submodule", "update", "--init", "--recursive"])
    print("Source revision: " + run(["git", "rev-parse", "HEAD"], capture=True).stdout.strip())


def optional_preferences(game):
    system = child_ci(game, "System")
    ini = child_ci(system, "Gothic.ini") if system else None
    if not ini:
        return None
    print("\nPC Gothic.ini is not copied wholesale. Android graphics, resolution, FPS limit, UI scale, paths and key bindings keep their defaults.")
    if not ask("Import supported gameplay/audio preferences (classic controls, subtitles, sensitivity, quick keys, volumes)?", False):
        return None
    preferences = safe_preferences(ini)
    print("Only these settings will be included, and only applied if the phone has no writable Gothic.ini:\n" + preferences.decode())
    return preferences if ask("Use these preferences?") else None


def optional_saves():
    print("\nOnly OpenGothic save_slot_N.sav and save_quick_N.sav files from the same game/mod are compatible, not original Gothic savegame folders.")
    if not ask("Include PC OpenGothic saves? Existing phone slots will be kept", False):
        return []
    folder = Path(input("PC OpenGothic save directory: ").strip().strip('"')).expanduser()
    saves = sorted([*folder.glob("save_slot_*.sav"), *folder.glob("save_quick_*.sav")])
    if not saves:
        raise ValueError("No OpenGothic saves found in that directory")
    for save in saves:
        validate_save(save)
    print("Available slots: " + ", ".join(p.name for p in saves))
    selection = input("Save filenames or manual slot numbers separated by spaces, or Enter for all: ").split()
    if selection:
        selected = {f"save_slot_{name}.sav" if name.isdecimal() else name for name in selection}
        if not selected <= {p.name for p in saves}:
            raise ValueError("A selected save slot does not exist")
        saves = [p for p in saves if p.name in selected]
    return saves


def inspect_apk(apk, bundled):
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if "lib/arm64-v8a/libopengothic.so" not in names or "AndroidManifest.xml" not in names:
            raise RuntimeError("APK is missing the ARM64 library or Android manifest")
        if any(name.startswith("lib/") and not name.startswith("lib/arm64-v8a/") for name in names):
            raise RuntimeError("Unexpected non-ARM64 library in APK")
        parts = [name for name in names if re.fullmatch(r"assets/private-game-[0-9]{5}\.ogpart", name)]
        if bool(parts) != bundled or any(name in names for name in ("assets/private-game.zip", "assets/game-data.zip")):
            raise RuntimeError("APK game-data contents do not match the selected packaging mode")
        if any(archive.getinfo(name).compress_type != zipfile.ZIP_STORED for name in parts):
            raise RuntimeError("APK compresses the already-compressed game archive")
        # Incremental ZIP rewriting can hide old private bytes in unreferenced gaps.
        # A fresh package needs only modest overhead for headers, alignment and signing.
        overhead = apk.stat().st_size - sum(entry.compress_size for entry in archive.infolist())
        if overhead > 1024 * 1024:
            raise RuntimeError("APK has excessive unreferenced space; force a fresh package before sharing it")
    if apk.stat().st_size >= 0xFFFFFFFF:
        raise RuntimeError("APK exceeds the ZIP32 limit; rerun with --split")


def stage_chunks():
    """AGP's asset task uses Java arrays: keep each entry well below 2 GiB."""
    assets = OUTPUT / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    for old in assets.iterdir():
        if old.is_file() and re.fullmatch(r"private-game-[0-9]{5}\.ogpart", old.name):
            old.unlink()
        else:
            raise RuntimeError(f"Unexpected file in game-data staging directory: {old}. Move it aside before packaging.")
    with (OUTPUT / "game-data.zip").open("rb") as source:
        index = 0
        while block := source.read(128 * 1024 * 1024):
            (assets / f"private-game-{index:05d}.ogpart").write_bytes(block)
            index += 1


def build(sdk, env, bundled, build_type="release", edition="gothic2notr"):
    if build_type not in ("release", "debug"):
        raise ValueError("Build type must be release or debug")
    app = EDITIONS[edition]
    variant = build_type.capitalize()
    print(f"\nBuilding {build_type} ARM64 APK" + (" with native ThinLTO and Java shrinking." if build_type == "release" else " with debugger support."))
    if bundled:
        stage_chunks()
    cmake_bin = sdk / "cmake/3.22.1/bin"
    build_root = ROOT / BUILD_DIRS[edition]
    run([cmake_bin / ("cmake.exe" if WINDOWS else "cmake"), "-S", ROOT / "android",
         "-B", build_root, "-G", "Ninja",
         f"-DCMAKE_MAKE_PROGRAM={cmake_bin / ('ninja.exe' if WINDOWS else 'ninja')}",
         f"-DTEMPEST_ANDROID_BUILD_TYPE={variant}",
         f"-DOPENGOTHIC_ANDROID_GAME={edition}"], env=env)
    project = build_root / "OpenGothic"
    wrapper = project / ("gradlew.bat" if WINDOWS else "gradlew")
    command = [wrapper] if WINDOWS else ["sh", wrapper]
    command += ["-p", project, "--no-daemon", f"assemble{variant}", f"lint{variant}", "--max-workers=2",
                "-Dorg.gradle.jvmargs=-Xmx3g -Dfile.encoding=UTF-8"]
    if bundled:
        command.append(f"-PprivateGameAssets={OUTPUT / 'assets'}")
    run(command, env=env)
    outputs = project / "app/build/outputs"
    apk = outputs / f"apk/{build_type}/app-{build_type}.apk"
    inspect_apk(apk, bundled)
    build_tools = sdk / "build-tools/35.0.0"
    run([build_tools / ("apksigner.bat" if WINDOWS else "apksigner"), "verify", "--verbose", "--print-certs", apk], env=env)
    manifest = run([build_tools / ("aapt.exe" if WINDOWS else "aapt"), "dump", "badging", apk], env=env, capture=True).stdout
    debuggable = "application-debuggable" in manifest
    if not re.search(r"package: name='" + re.escape(app) + r"'", manifest):
        raise RuntimeError("APK application ID does not match the selected game")
    if debuggable != (build_type == "debug"):
        raise RuntimeError("APK debuggable flag does not match the selected build type")
    stem = APK_NAMES[edition] + ("-with-data" if bundled else "") + ("-debug" if build_type == "debug" else "") + "-arm64"
    destination = OUTPUT / f"{stem}.apk"
    symbols = []
    if build_type == "release":
        for source, suffix in ((outputs / "native-debug-symbols/release/native-debug-symbols.zip", "native-symbols.zip"),
                               (outputs / "mapping/release/mapping.txt", "mapping.txt")):
            target = OUTPUT / f"{stem}-{suffix}"
            shutil.copy2(source, target)
            symbols.append(target.name)
    shutil.copy2(apk, destination)
    revision = run(["git", "rev-parse", "HEAD"], capture=True).stdout.strip()
    report = {"revision": revision, "apk": destination.name, "bytes": destination.stat().st_size,
              "sha256": sha256(destination), "bundled_game_files": bundled,
              "build_type": build_type, "debuggable": debuggable, "debug_artifacts": symbols,
              "edition": edition, "application_id": app,
              "signing": "custom" if build_type == "release" and env.get("OPENGOTHIC_KEYSTORE") else "local_debug_key",
              "source_modified": bool(run(["git", "status", "--porcelain"], capture=True).stdout.strip()),
              "submodules": run(["git", "submodule", "status", "--recursive"], capture=True).stdout.splitlines(),
              "warning": "Do not redistribute Gothic game files or APKs containing them."}
    (OUTPUT / f"{edition}-build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"\nVerified signed ARM64 APK: {destination}\nSHA-256: {report['sha256']}")
    print("Keep your signing key private and backed up (default: ~/.android/debug.keystore). A different key cannot update the same app in place.")
    if symbols:
        print("Keep the matching native symbols and Java mapping for crash reports; they are not included in the APK.")
    return destination


def device(adb):
    while True:
        result = run([adb, "devices", "-l"], capture=True)
        print(result.stdout)
        devices = []
        for line in result.stdout.splitlines()[1:]:
            fields = line.split()
            if len(fields) >= 2 and fields[1] == "device":
                devices.append((fields[0], line))
        if devices:
            return choose(devices, lambda value: value[1])[0]
        print("Unlock the phone, enable Developer options / USB debugging, use a data-capable cable, and approve the RSA prompt.")
        if not WINDOWS:
            print("Linux may require USB udev rules. WSL needs USB passthrough (usbipd-win); otherwise use --no-install and copy the APK over a network.")
        if not ask("Retry device detection?"):
            return None


def backup_saves(adb, serial, edition="gothic2notr"):
    app = EDITIONS[edition]
    phone = f"/sdcard/Android/data/{app}/files"
    prefix = [adb, "-s", serial]
    run([*prefix, "shell", "am", "force-stop", app])
    listing = run([*prefix, "shell", "ls", phone], capture=True)
    slots = [line.strip() for line in listing.stdout.splitlines() if re.fullmatch(r"save_(?:slot_[0-9]+|quick_(?:[1-9]|1[0-9]|20))\.sav", line.strip())]
    if not slots:
        print("No OpenGothic saves found on this phone.")
        return
    # Unique backup directories never replace PC saves, even when slot numbers match.
    backup = Path(tempfile.mkdtemp(prefix="phone-saves-" + edition + "-" + datetime.now().strftime("%Y%m%d-%H%M%S-"), dir=OUTPUT))
    for name in slots:
        run([*prefix, "pull", f"{phone}/{name}", backup / name])
        validate_save(backup / name)
    print(f"Backed up {len(slots)} saves to {backup}\nFor backport: close PC OpenGothic, then copy to its working directory using empty slots. Keep the backups.")


def install_apk(prefix, apk):
    print(f"Transferring {apk.stat().st_size / 1024**3:.2f} GiB, then installing. Keep the USB cable connected.")
    while True:
        # Separate transfer from Package Manager so large-file failures are easier to diagnose.
        result = run([*prefix, "install", "--no-streaming", "-r", apk], capture=True, check=False)
        print(result.stdout + result.stderr)
        if result.returncode == 0:
            return
        print("Installation failed. Check USB debugging/authorization and free space. For large-APK failures, try --split or manual network transfer.")
        print("For INSTALL_FAILED_UPDATE_INCOMPATIBLE, restore the previous signing key. Do not uninstall without backing up assets, settings and saves.")
        run([prefix[0], "devices", "-l"], check=False)
        if not ask("Retry installation on the same phone after correcting the problem?"):
            raise RuntimeError("Installation cancelled. Built files are still available for manual transfer; no app was uninstalled.")


def install(sdk, apk, bundled, env, edition="gothic2notr"):
    app = EDITIONS[edition]
    phone = f"/sdcard/Android/data/{app}/files"
    adb = sdk / "platform-tools" / ("adb.exe" if WINDOWS else "adb")
    print("\nInstalling over ADB requires USB or wireless debugging. Otherwise, transfer the APK and (for split mode) game-data.zip to Downloads.")
    if not ask("Install on a connected phone now?"):
        return
    serial = device(adb)
    if not serial:
        return
    prefix = [adb, "-s", serial]
    abi = run([*prefix, "shell", "getprop", "ro.product.cpu.abilist"], capture=True).stdout
    if "arm64-v8a" not in abi:
        raise RuntimeError("Selected device does not advertise arm64-v8a")
    print("The selected phone's game will be closed for installation. Save progress before continuing.")
    if not ask("Continue with this phone?"):
        return
    exists = run([*prefix, "shell", "test", "-d", phone], capture=True, check=False)
    if exists.returncode == 0 and ask("Back up phone OpenGothic saves to this PC before updating?", False):
        backup_saves(adb, serial, edition)
    run([*prefix, "shell", "am", "force-stop", app])
    run([*prefix, "shell", "df", "-h", "/data"])
    print("Allow room for the APK, extracted game files and installer overhead (about 8–10 GB free for a typical installation).")
    install_apk(prefix, apk)
    if not bundled:
        run([*prefix, "push", OUTPUT / "game-data.zip", "/sdcard/Download/game-data.zip"])
        print("On the phone, choose game-data.zip from Downloads. Matching game files will be reused and existing saves/settings kept.")
    launch = [*prefix, "shell", "am", "start", "-W", "-n", f"{app}/org.opengothic.app.SetupActivity"]
    if not bundled:
        launch += ["-a", f"{app}.IMPORT_GAME_FILES"]
    run(launch)
    print("APK installed. Keep the phone unlocked while extraction runs, then check that the game reaches its menu.")
    print(f"Logs: {adb} -s {serial} logcat -v threadtime Tempest:I AndroidRuntime:E libc:F '*:S'")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", help="Gothic 1, Gothic II Classic or Night of the Raven installation directory")
    parser.add_argument("--edition", choices=tuple(EDITIONS), help="Launcher/save-backup edition; builds normally detect it from --game")
    parser.add_argument("--jdk", help="Existing JDK 17 directory")
    parser.add_argument("--sdk", help="Existing or new Android SDK directory")
    parser.add_argument("--cache", default=str(Path.home() / ".cache/opengothic-android"), help="Portable tool cache")
    parser.add_argument("--split", action="store_true", help="Build an asset-free APK and a separate importable ZIP")
    parser.add_argument("--build-type", choices=("release", "debug"), default="release", help="Optimized release (default), or debug with debugger support")
    parser.add_argument("--no-install", action="store_true", help="Build for manual/network transfer without ADB installation")
    parser.add_argument("--prepare-only", action="store_true", help="Discover/install tools, but do not package, build or install")
    parser.add_argument("--package-only", action="store_true", help="Package game data without installing tools or building")
    parser.add_argument("--backup-saves", action="store_true", help="Only back up phone saves; do not build or install")
    args = parser.parse_args(argv)
    if platform.machine().lower() not in ("amd64", "x86_64") or sys.platform not in ("win32", "linux"):
        raise RuntimeError("These scripts support x86-64 Windows and glibc Linux (including Arch WSL).")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    print("OpenGothic Android setup\nEnter accepts [Y/n]; optional data imports default to [y/N].\nDo not redistribute Gothic game files or APKs containing them. Keep the original PC installation and saves.")
    if args.backup_saves:
        sdk = Path(args.sdk or os.environ.get("ANDROID_HOME", ""))
        adb = str(sdk / "platform-tools" / ("adb.exe" if WINDOWS else "adb"))
        if not Path(adb).is_file():
            adb = shutil.which("adb")
        if not adb:
            raise RuntimeError("Set --sdk to an SDK with platform-tools, or put adb on PATH")
        serial = device(adb)
        if serial:
            edition = args.edition or choose(list(EDITIONS), lambda item: EDITION_NAMES[item])
            backup_saves(adb, serial, edition)
        return
    if args.prepare_only:
        source_ready()
        sdk, env = toolchain(args)
        print("Prerequisites ready. Rerun without --prepare-only to build and install.")
        return
    game = select_game(args.game, args.edition)
    edition = next(key for key, name in EDITION_NAMES.items() if name == game_edition(game))
    args.edition = edition
    print(f"Selected installation (read only): {game}")
    print(f"Game: {game_edition(game)}")
    print("Use a clean installation. Union and Windows DLL plugins do not run here; their modified scripts or assets can break the game.")
    plugins = unsupported_plugins(game)
    if plugins:
        print("Unsupported plugin files found:\n" + "\n".join(str(p.relative_to(game)) for p in plugins))
        if not ask("Continue with these modified files anyway? A clean installation is recommended", False):
            raise RuntimeError("Choose a clean game installation and rerun")
    if not args.package_only:
        source_ready()
        sdk, env = toolchain(args)
    if not ask("Package this legally owned installation for your own devices?"):
        raise RuntimeError("Game-data packaging cancelled")
    preferences = optional_preferences(game)
    saves = optional_saves()
    # Shader/native intermediates plus compressed archives need considerably more room than the game itself.
    if shutil.disk_usage(OUTPUT).free < 20 * 1024**3:
        raise RuntimeError("Keep at least 20 GiB free on the checkout drive for building and packaging.")
    last = [0.0]
    def progress(message):
        if time.monotonic() - last[0] > 2:
            print(message, flush=True)
            last[0] = time.monotonic()
    metadata = package(game, OUTPUT / "game-data.zip", preferences, saves, progress)
    (OUTPUT / "asset-report.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Game archive: {OUTPUT / 'game-data.zip'}\nExtracted size: {metadata['bytes'] / 1024**3:.2f} GiB")
    if args.package_only:
        return
    bundled = not args.split
    if (OUTPUT / "game-data.zip").stat().st_size > 3800 * 1024**2:
        print("Archive approaches the APK's ZIP32 limit. Switching to separate APK + ZIP.")
        bundled = False
    apk = build(sdk, env, bundled, args.build_type, edition)
    if not args.no_install:
        install(sdk, apk, bundled, env, edition)
    print(f"\nOutput: {OUTPUT}\nManual transfer: install the APK from Android's file manager (allow installs from that source). For split mode, select the ZIP in OpenGothic. No USB is required for manual transfer.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, EOFError, KeyboardInterrupt) as error:
        print(f"\nSetup stopped: {error}\nExisting game files and installed apps were not removed. Correct the issue and rerun.", file=sys.stderr)
        sys.exit(1)
