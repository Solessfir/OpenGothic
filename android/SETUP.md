# Android setup

Build and install using your legally owned **Gothic 1**, **Gothic II Classic** or **Gothic II: Night of the Raven** files.
Do not redistribute game assets or APKs containing them.
Use a clean installation without Union or Windows DLL plugins; unsupported plugin-dependent scripts or modified assets can break startup or gameplay.
Removing a plugin DLL alone does not undo its script/data changes.

## Package game files only

Already have the APK? Clone this branch and run the following on your computer:

```bat
setup-android.bat --package-only --game "D:/Games/Gothic II"
```

On Linux: `bash setup-android.sh --package-only --game "/path/to/Gothic II"`.
For Gothic 1, use `setup-android.bat --package-only --game "D:/Games/Gothic"` (the same `--game` option works on Linux).
The game is detected automatically from its world archives, independently of dialogue language.
Omit `--game` to search Steam/GOG/common installation folders and choose which edition to install.
No Android SDK, NDK or Java is needed for packaging.
Classic requires a complete Classic installation; removing addon archives from NotR or using Steam's Classic-mode mods is not a standalone Classic installation for these scripts.
The script creates `build/android-setup/game-data.zip`. Copy it to your phone, select it in the app,
and delete the transferred ZIP after the game starts.

Select the installation root containing `Data`, `_work` and `System`, not just the `Data` folder.
The script packages `Data`, `_work`, and `System/GothicGame.ini` when present, plus an index the importer requires.
Windows executables and DLLs are excluded. Saves and selected preferences are optional.
`game-data.zip` is only the output filename; renaming it is fine, but manually creating a ZIP is not supported.

## Build and install from source

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

The scripts ask before installing missing prerequisites, packaging game files or accessing a phone.
**Enter means yes at [Y/n]**; optional preferences and saves default to no at [y/N].

They search Steam/GOG/common Games folders, let you choose another path, install missing build tools,
build and check an ARM64 APK, and offer phone installation.
Android SDK licenses remain interactive. Downloads and completed build work are reused on reruns.
If a required dependency is declined, install it yourself and rerun.

Builds default to **optimized release**: native `-O3` and ThinLTO, Java/resource shrinking, and no debugger support.
For debugging, pass `--build-type debug` to either launcher. Debug APK names include `-debug`.

## Install without USB

For the more portable APK + ZIP option:

```bat
setup-android.bat --split --no-install
```

Linux accepts the same options: `bash setup-android.sh --split --no-install`.

Outputs are in `build/android-setup/`:

| Output | Use |
| --- | --- |
| `OpenGothic-*-arm64.apk` | Install the APK built for your selected game |
| `game-data.zip` | Import into the matching app |
| `*-with-data-arm64.apk` | Single-file package with that edition's game assets, when it fits |
| `*-report.json` | Build/package details and checksums |
| `*-native-symbols.zip`, `*-mapping.txt` | Release crash symbols; keep with the matching APK |
| `phone-saves-*` | Optional save backups |

1. Transfer the APK and, in split mode, ZIP to any folder on your phone.
2. Open the APK in the file manager and allow that source to install unknown apps.
3. Launch the matching app: **Gothic**, **Gothic II Classic** or **Gothic II: NotR**. For split mode, tap **Choose game archive** and select the ZIP.
4. Keep setup open until extraction completes. Gameplay starts in landscape afterward.

Use the ZIP produced by the setup scripts, not an arbitrary zipped installation.
ZArchiver is unnecessary; the app imports through Android's document picker.
If a bundled APK is rejected or too large, rerun with `--split`.
After successful import, delete the transferred APK and ZIP from your phone to reclaim space.

To import into an existing installation, quit the game and long-press its launcher icon → **Import files**.
If the launcher lacks shortcuts:

```sh
adb shell am start -n org.opengothic.gothic2notr/org.opengothic.app.SetupActivity -a org.opengothic.gothic2notr.IMPORT_GAME_FILES
```

For direct installation over Wi-Fi, see [wireless debugging](README.md#wireless-debugging).

## Updates and data safety

- Assets go to `Android/data/<app-id>/files/Gothic2`; saves and settings are directly under `files`.
- Import preserves existing saves/settings. Conflicting modified assets stop import instead of being overwritten.
- Interrupted imports can be retried; completed matching files are reused.
- Update with the same signing key. Back up `~/.android/debug.keystore` privately for builds on another PC.
- **Uninstalling removes extracted assets, saves and settings.** An in-place APK update preserves them.
- Windows executable plugins are not supported. Start with an unmodified installation of your chosen game.

A bundled APK keeps both compressed and extracted assets. APK + ZIP avoids retaining the archive once the transferred ZIP is deleted.

### Game editions and storage

Install any or all three APKs and import each game's archive into its matching app.
The apps have separate storage, so neither replaces the other's saves, settings or game files.

| Game | App ID |
| --- | --- |
| Gothic 1 | `org.opengothic.gothic1` |
| Gothic II Classic | `org.opengothic.gothic2` |
| Gothic II: Night of the Raven | `org.opengothic.gothic2notr` |

All retain the internal `Gothic2` game-data directory name for compatibility with existing archives.
ADB examples elsewhere in these docs use NotR's app ID.
To launch Gothic 1, use `adb shell am start -n org.opengothic.gothic1/org.opengothic.app.SetupActivity`.
To reopen its importer, append `-a org.opengothic.gothic1.IMPORT_GAME_FILES`.
Use `org.opengothic.gothic1` in its ADB storage paths too.
Never mix the games' files or saves.

### Signing

Both build types use your local `~/.android/debug.keystore` by default, so you can switch without reinstalling.
For distribution with your own key, set all four environment variables before building release:
`OPENGOTHIC_KEYSTORE` (absolute keystore path), `OPENGOTHIC_KEY_ALIAS`, `OPENGOTHIC_STORE_PASSWORD`, and `OPENGOTHIC_KEY_PASSWORD`.
Keep keys/passwords outside the repository. Changing the key prevents in-place updates to an already installed app.
Only asset-free APKs may be shared publicly.

## PC preferences and saves

Do not copy a complete PC `Gothic.ini`. The scripts can import these supported preferences:

- Combat mode, quicksave/potion shortcuts, dialogue subtitles and mouse/camera sensitivity.
- Music enabled, music volume and sound volume.

Graphics, resolution, paths, keyboard mappings and `SystemPack.ini` are not imported.
Existing writable phone settings are never replaced. See [configuration](CONFIGURATION.md) for Android options.

Only OpenGothic `save_slot_N.sav` saves transfer, not original Gothic 1 or Gothic II saves.
Use matching game/mod data and OpenGothic versions. Existing phone slots are not overwritten.

Back up phone saves without rebuilding:

```bat
setup-android.bat --backup-saves --sdk "C:/Path/To/Android/Sdk"
```

The `.sh` accepts the same arguments. With PC OpenGothic closed, copy the backups into its working directory using empty slots.
Save backup asks which game to use; pass `--edition gothic1`, `--edition gothic2` (Classic) or `--edition gothic2notr` to select it directly.
See [manual save transfer](README.md#transfer-saves-from-pc).

## Useful options

```bat
setup-android.bat --game "D:/Games/Gothic II" --jdk "C:/Path/To/jdk-17" --sdk "C:/Path/To/Android/Sdk"
setup-android.bat --prepare-only
setup-android.bat --package-only --game "D:/Games/Gothic II"
setup-android.bat --no-install
setup-android.bat --build-type debug --split --no-install
setup-android.bat --help
```

`--cache PATH` selects the tool/download cache; the default is `~/.cache/opengothic-android`.
`--no-install` skips phone access, not build/package prompts.
On Windows without Winget, install Python 3.10+ manually and rerun.
Linux USB may require Android udev rules; WSL USB requires forwarding. File transfer or wireless ADB avoids USB setup.
