"""Build an indexed private archive without modifying the owner's installation."""

import configparser
import hashlib
import io
import os
from pathlib import Path
import re
import zipfile

INDEX = "opengothic-private-v1.tsv"
BLOCKED_SUFFIXES = {".exe", ".dll", ".bat", ".cmd", ".ps1", ".lnk", ".sav", ".log"}
SAFE_INI = {
    "GAME": {
        "usegothic1controls": ("useGothic1Controls", 0, 1),
        "usequicksavekeys": ("useQuickSaveKeys", 0, 1),
        "usepotionkeys": ("usePotionKeys", 0, 1),
        "subtitles": ("subTitles", 0, 1),
        "subtitlesplayer": ("subTitlesPlayer", 0, 1),
        "mousesensitivity": ("mouseSensitivity", 0, 1),
    },
    "SOUND": {
        "musicenabled": ("musicEnabled", 0, 1),
        "musicvolume": ("musicVolume", 0, 1),
        "soundvolume": ("soundVolume", 0, 1),
    },
}


def child_ci(path, name):
    return next((p for p in Path(path).iterdir() if p.name.casefold() == name.casefold()), None)


def validate_game(root):
    root = Path(root).expanduser().resolve()
    if not root.is_dir():
        raise ValueError(f"Game directory does not exist: {root}")
    data, work = child_ci(root, "Data"), child_ci(root, "_work")
    if not data or not data.is_dir() or not work or not work.is_dir():
        raise ValueError("Select the installation root containing Data and _work, not its System folder.")
    names = {p.name.casefold() for p in data.iterdir()}
    if not {"worlds.vdf", "worlds_addon.vdf"} <= names:
        raise ValueError("Expected Data/Worlds.vdf and Worlds_Addon.vdf (Gothic II: Night of the Raven). Classic-only installs are not supported by this guide.")
    return root


def game_files(root):
    root = validate_game(root)
    result = []
    for name in ("Data", "_work"):
        folder = child_ci(root, name)
        for path in sorted(folder.rglob("*")):
            if path.is_symlink():
                raise ValueError(f"Resolve symlinks before packaging: {path}")
            if not path.is_file() or path.suffix.lower() in BLOCKED_SUFFIXES:
                continue
            if path.name.endswith(".og-extract-part"):
                raise ValueError(f"Reserved extraction temporary filename: {path}")
            if not path.resolve().is_relative_to(root):
                raise ValueError(f"File leaves game directory: {path}")
            relative = path.relative_to(folder).as_posix()
            if any(c in relative for c in "\t\r\n\\:"):
                raise ValueError(f"Unsupported asset filename: {path}")
            result.append((f"Gothic2/{name}/{relative}", path))
    system = child_ci(root, "System")
    config = child_ci(system, "GothicGame.ini") if system else None
    if config and config.is_file():
        if config.is_symlink() or not config.resolve().is_relative_to(root):
            raise ValueError(f"System configuration must be inside the installation: {config}")
        result.append(("Gothic2/System/GothicGame.ini", config))
    # No executables, Saves, SystemPack.ini or desktop Gothic.ini are copied.
    return result


def safe_preferences(path):
    source = configparser.ConfigParser(interpolation=None, strict=False, inline_comment_prefixes=(";",))
    source.read_string(Path(path).read_text(encoding="utf-8-sig", errors="replace"))
    result = configparser.ConfigParser(interpolation=None)
    result.optionxform = str
    for section in source.sections():
        allowed = SAFE_INI.get(section.upper(), {})
        for key, value in source.items(section):
            if key not in allowed:
                continue
            name, minimum, maximum = allowed[key]
            try:
                number = float(value)
            except ValueError:
                continue
            if not minimum <= number <= maximum:
                continue
            if name not in ("mouseSensitivity", "musicVolume", "soundVolume") and number not in (0, 1):
                continue
            if not result.has_section(section.upper()):
                result.add_section(section.upper())
            result[section.upper()][name] = f"{number:g}"
    output = io.StringIO()
    result.write(output, space_around_delimiters=False)
    return output.getvalue().encode("utf-8")


def validate_save(path):
    if not re.fullmatch(r"save_slot_[0-9]+\.sav", Path(path).name):
        raise ValueError(f"Not an OpenGothic slot filename: {path}")
    try:
        with zipfile.ZipFile(path) as archive:
            with archive.open("header") as header:
                if header.read(16) != b"OpenGothic/Save\x00":
                    raise ValueError("Not an OpenGothic save header")
    except (zipfile.BadZipFile, KeyError) as error:
        raise ValueError(f"Not a supported OpenGothic save: {path}") from error


def sha256(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def package(root, output, preferences=None, saves=(), progress=print):
    """Manifest first, regular ZIP entries after; streamable by the phone and ZArchiver."""
    files = game_files(root)
    for save in saves:
        validate_save(save)
        files.append((Path(save).name, Path(save)))
    if len({name.casefold() for name, _ in files}) != len(files):
        raise ValueError("Case-insensitive duplicate filenames in the installation")
    inline = {"Gothic.ini": preferences} if preferences else {}
    manifest = []
    for name, path in files:
        progress(f"Hashing {name}")
        manifest.append(f"{path.stat().st_size}\t{sha256(path)}\t{name}\n")
    for name, contents in inline.items():
        manifest.append(f"{len(contents)}\t{hashlib.sha256(contents).hexdigest()}\t{name}\n")
    index = "".join(manifest).encode("utf-8")
    if len(index) > 4 * 1024 * 1024:
        raise ValueError("Too many files for the private asset index")
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".zip.part")
    try:
        # ZIP64 supports a separate archive exceeding 4 GiB; the APK itself must stay below that limit.
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
            archive.writestr(INDEX, index)
            for name, path in files:
                progress(f"Compressing {name}")
                archive.write(path, name)
            for name, contents in inline.items():
                archive.writestr(name, contents)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    return {"bytes": sum(p.stat().st_size for _, p in files) + sum(map(len, inline.values())),
            "sha256": sha256(output), "entries": len(files) + len(inline)}
