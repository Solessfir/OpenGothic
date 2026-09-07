# Guided Android setup

Build and install using your legally owned **Gothic II: Night of the Raven** files.
Packages containing game assets are for private use only - never upload them to releases or CI artifacts.

## Run the guide

Clone this Android branch with Git, then run from the repository root:

```bat
setup-android.bat
```

On x86-64 glibc Linux, including WSL:

```sh
bash setup-android.sh
```

Windows requires Windows 10/11 on x86-64. Linux builds need a separate Linux checkout and SDK.
Allow roughly 20 GiB on the build drive and 8–10 GB on the phone; larger installations need more.

The guide asks before installing missing prerequisites, packaging personal files or accessing a phone.
**Enter means yes at [Y/n]**; optional preferences and saves default to no at [y/N].

It finds Steam/GOG/common Games folders, lets you choose another path, installs missing build tools,
builds and checks an ARM64 APK, and offers phone installation.
Android SDK licenses remain interactive. Downloads and completed build work are reused on reruns.
If a required dependency is declined, install it yourself and rerun.

## Install without USB

For the more portable APK + ZIP option:

```bat
setup-android.bat --split --no-install
```

Linux accepts the same options: `bash setup-android.sh --split --no-install`.

Outputs are in `build/private-android/`:

| Output | Use |
| --- | --- |
| `OpenGothic-arm64.apk` + `private-game.zip` | Install APK, then import ZIP |
| `OpenGothic-PRIVATE-arm64.apk` | Single-file package with game assets, when it fits |
| `*-report.json` | Build/package details and checksums |
| `phone-saves-*` | Optional save backups |

1. Transfer the APK and, in split mode, ZIP to the phone's Downloads folder privately.
2. Open the APK in the file manager and allow that source to install unknown apps.
3. Launch **Gothic II**. For split mode, tap **Choose private-game.zip** and select the ZIP.
4. Keep setup open until extraction completes. Gameplay starts in landscape afterward.

Use the ZIP produced by this guide, not an arbitrary zipped installation.
ZArchiver is unnecessary; the app imports through Android's document picker.
If a bundled APK is rejected or too large, rerun with `--split`.
After successful import, delete the transferred files from Downloads to reclaim space.

To import into an existing installation, quit the game and long-press its launcher icon → **Import files**.
If the launcher lacks shortcuts:

```sh
adb shell am start -n org.opengothic.app/.SetupActivity -a org.opengothic.app.IMPORT_GAME_FILES
```

For direct installation over Wi-Fi, see [wireless debugging](README.md#wireless-debugging).

## Updates and data safety

- Game assets go to `Android/data/org.opengothic.app/files/Gothic2`.
- Import preserves existing saves/settings. Conflicting modified assets stop import instead of being overwritten.
- Interrupted imports can be retried; completed matching files are reused.
- Update with the same signing key. Back up `~/.android/debug.keystore` privately for builds on another PC.
- **Uninstalling removes extracted assets, saves and settings.** An in-place APK update preserves them.
- Windows executable plugins are not supported. Start with the base Night of the Raven installation.

A bundled APK keeps both compressed and extracted assets. APK + ZIP avoids retaining the archive once Downloads is cleaned up.

## PC preferences and saves

Do not copy a complete PC `Gothic.ini`. The guide can import only these supported preferences:

- Combat mode, quicksave/potion shortcuts, dialogue subtitles and mouse/camera sensitivity.
- Music enabled, music volume and sound volume.

Graphics, resolution, paths, keyboard mappings and `SystemPack.ini` are not imported.
Existing writable phone settings are never replaced. See [configuration](CONFIGURATION.md) for Android options.

Only OpenGothic `save_slot_N.sav` saves transfer, not original Gothic II saves.
Use matching game/mod data and OpenGothic versions. Existing phone slots are not overwritten.

Back up phone saves without rebuilding:

```bat
setup-android.bat --backup-saves --sdk "C:/Path/To/Android/Sdk"
```

The `.sh` accepts the same arguments. With PC OpenGothic closed, copy the backups into its working directory using empty slots.
See [manual save transfer](README.md#transfer-saves-from-pc).

## Useful options

```bat
setup-android.bat --game "D:/Games/Gothic II" --jdk "C:/Path/To/jdk-17" --sdk "C:/Path/To/Android/Sdk"
setup-android.bat --prepare-only
setup-android.bat --package-only --game "D:/Games/Gothic II"
setup-android.bat --no-install
setup-android.bat --help
```

`--cache PATH` selects the tool/download cache; the default is `~/.cache/opengothic-android`.
`--no-install` skips phone access, not build/package prompts.
On Windows without Winget, install Python 3.10+ manually and rerun.
Linux USB may require Android udev rules; WSL USB requires forwarding. File transfer or wireless ADB avoids USB setup.
