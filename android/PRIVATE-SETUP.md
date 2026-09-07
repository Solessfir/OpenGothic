# Guided private Android setup

Use a legally owned **Gothic II: Night of the Raven** installation.
This guide builds a debug ARM64 APK for private testing on your own devices.
Do not publish or redistribute an APK/archive containing Gothic files, including in GitHub releases or CI artifacts.
The normal Gradle build remains asset-free.

## Start here

On x86-64 Windows 10/11, clone this repository, then double-click **setup-android.bat** in the checkout.
Alternatively, from a terminal at the repository root:

```bat
setup-android.bat
```

On x86-64 glibc Linux, including Arch Linux under WSL:

```sh
bash setup-android.sh
```

The guide asks before installing missing prerequisites, packaging personal data, or touching a phone.
**Enter means yes at `[Y/n]`**. Optional preferences and saves use `[y/N]`, so Enter leaves them out.
Declining a required dependency stops with an explanation; rerun after providing that dependency yourself.
Keep roughly 20 GiB free on the build drive and 8–10 GB free on the phone for a typical installation.
Larger/modded installations may need more. First-time downloads and compilation can take a while.

The steps are:

1. Find Python 3.10+ and Git. Windows offers a per-user Python install through Winget; Linux offers its package manager.
   If Winget is missing, install Microsoft's App Installer or Python from python.org and rerun.
   Git must already be available from the clone step and on PATH.
2. Initialize missing pinned submodules without resetting modified revisions.
3. Reuse a working JDK 17/Android SDK, or offer portable downloads into `~/.cache/opengothic-android`.
   The guide pins Temurin 17.0.20.1, command-line tools 12.0, SDK 35, Build Tools 35.0.0, NDK 27.0.12077973, CMake 3.22.1 and Gradle 8.9.
   Platform-tools follows Google's stable package because ADB needs current device support.
   Official bootstrap downloads and the Gradle distribution are SHA-256 checked; SDK packages are installed by Google's SDK manager.
   Licenses remain interactive; there is no automatic `yes` pipe or system-wide Java/SDK environment change.
4. Discover Steam libraries (including custom libraries), GOG registry entries on Windows, and common `Games` directories.
   WSL discovery also checks mounted Windows drives. Choose a result or enter an installation path.
5. Optionally import supported PC preferences and **OpenGothic** saves.
6. Hash/compress the game files, compile the native library and APK, run Android lint, check its ARM64 contents and signature.
7. Select an ADB device, optionally back up its saves, confirm closing the game, update the APK and launch setup.
   On first launch the app checks/extracts files with progress before starting the engine.

SDK manager/package-manager license prompts are their own prompts and may have different defaults.
Portable tools are shared between reruns; partial downloads resume and are verified before use.
If archive preparation is interrupted, rerunning rebuilds the archive without deleting a previous completed copy.
Gradle reuses completed compilation work.

## Output and installation without USB

The output directory is **`build/private-android`** (ignored by Git):

- `OpenGothic-PRIVATE-arm64.apk`: the signed single-file APK with game files, when it fits.
- `private-game.zip`: the same indexed game payload, also usable separately.
- `build-report.json` and `asset-report.json`: source revision, sizes and SHA-256 checksums.
- `phone-saves-*`: optional, separate save backups; never copied over PC slots automatically.

For another device, copy the private APK over your own network or other private file-transfer method.
Open it in Android's file manager and allow that source to install unknown apps when Android asks.
Launch OpenGothic and keep its setup screen open until extraction finishes.
No USB connection, broad storage permission, or legacy external-storage setting is required.

If a single APK is too large for packaging, transfer or installation, use **`--split`**:

```bat
setup-android.bat --split --no-install
```

```sh
bash setup-android.sh --split --no-install
```

Transfer `OpenGothic-arm64.apk` and `private-game.zip` to the phone.
Install the APK, launch it, press **Choose private-game.zip**, and select the archive from Downloads.
This uses Android's document picker and extracts into the app's own storage.
If game files are already installed, quit the game and long-press its launcher icon → **Import files** to import additional saves from a private archive.
On launchers without shortcut support, use `adb shell am start -n org.opengothic.app/.SetupActivity -a org.opengothic.app.IMPORT_GAME_FILES`.
ZArchiver can inspect/extract the standard ZIP, but **manual access to `Android/data` is restricted on modern Android**;
using OpenGothic's import button is the supported fallback.
This is an APK plus a data archive, not Android split APKs/APKS requiring a separate package installer.
The guide switches to separate files if the compressed payload approaches the APK's ZIP32 size limit.
Within a bundled APK, the compressed payload is split into 128 MiB asset entries to avoid Gradle's per-asset Java array limit.
The phone streams those internal chunks as one archive; users still transfer/install just one APK.
Debug APK packaging is deliberately non-incremental to avoid the packager's offset overflow when updating large ZIP entries.
It also removes orphaned private data when returning to an asset-free build; the guide checks for excessive unreferenced APK bytes.
Native compilation still uses its normal caches.
If Gradle or the phone rejects a smaller large APK, rerun explicitly with `--split`.

With USB debugging, split mode also copies the ZIP to Downloads and opens the import screen; select it on the phone.
After a successful import you can remove the **Downloads ZIP** to reclaim space.
A bundled APK necessarily retains its compressed payload as well as the extracted game files.

## Existing installations and safety

Game files go to `Android/data/org.opengothic.app/files/Gothic2`.
The package includes `Data`, `_work` and `System/GothicGame.ini`, not Windows executables, DLLs, original save folders or unrelated installation files.
Executable extensions inside the asset directories are excluded too.
Native Windows plugins are not supported by copying their files; this guide is for the base Night of the Raven installation.

Extraction validates paths, sizes and SHA-256 hashes, then renames each completed temporary file.
After interruption, reselect the ZIP (split mode) or retry bundled extraction; completed matching files are reused.
The app never replaces existing saves or writable settings.
If an existing game asset differs, setup stops instead of overwriting a modified installation.
Back up your data and move the existing `Gothic2` folder aside before importing a different installation/mod.
For a normal update using the same payload, the completed index avoids re-extracting it.

The guide uses Gradle's existing debug signing key at `~/.android/debug.keystore`.
Back it up privately if you want to build updates on another PC.
A different key causes `INSTALL_FAILED_UPDATE_INCOMPATIBLE`; the guide **does not uninstall the app to bypass this**.
Uninstalling OpenGothic deletes its app-specific game files, settings and saves, not just the APK.
An in-place `adb install -r` update preserves those files.

## PC Gothic.ini and saves

Copying a whole PC `Gothic.ini` is not a safe default: desktop resolution, graphics, FPS limits and key bindings are unsuitable for a phone.
The optional import previews only these supported preferences:

- `GAME`: `useGothic1Controls`, `useQuickSaveKeys`, `usePotionKeys`, `subTitles`, `subTitlesPlayer`, `mouseSensitivity`.
- `SOUND`: `musicEnabled`, `musicVolume`, `soundVolume`.

Invalid/out-of-range values are ignored. Graphics, UI scale, paths, keyboard mappings and `SystemPack.ini` are not imported.
The generated writable `Gothic.ini` is applied **only if that file does not already exist on the phone**.
Android defaults continue to provide the mobile graphics settings; edit supported Android options later as described in [README.md](README.md).

Only **OpenGothic** `save_slot_N.sav` files can be transferred between PC and Android.
Original Gothic II saves use a different format; renaming them does not convert them.
Prefer the same OpenGothic revision and game/mod data on both devices.
The guide validates the OpenGothic save header, but that alone cannot guarantee compatibility of every historical save version.
Existing phone slots win; import does not overwrite them.

To back up phone saves or take them back to PC without rebuilding:

```bat
setup-android.bat --backup-saves --sdk "C:\Path\To\Android\Sdk"
```

```sh
bash setup-android.sh --backup-saves --sdk "$HOME/Android/Sdk"
```

With PC OpenGothic closed, copy the resulting `.sav` files into its working directory, selecting empty slot numbers.
This last step is manual so PC saves are not accidentally overwritten.
See [PC save transfer details](README.md#transfer-saves-from-pc).

## Useful overrides and troubleshooting

```bat
setup-android.bat --game "D:\Games\Gothic II" --jdk "C:\Path\To\jdk-17" --sdk "C:\Path\To\Android\Sdk"
setup-android.bat --prepare-only
setup-android.bat --package-only --game "D:\Games\Gothic II"
setup-android.bat --no-install
```

The `.sh` accepts the same arguments. `--help` lists them; `--cache PATH` chooses the portable tool/download cache.
`--no-install` skips device access, not the build or packaging prompts.

On Linux, the official Android host tools require **x86-64 glibc**, not Alpine/musl or an ARM Linux host.
Use a separate Linux checkout and SDK: do not reuse a Windows CMake/Gradle build directory or Windows SDK inside WSL.
For WSL compilation, a checkout in the Linux filesystem is faster than `/mnt/c`.
Linux USB access may require your distribution's Android udev rules.
WSL USB access requires Windows USB forwarding (usbipd-win); alternatively use `--no-install` and transfer files without USB.
An offline/unauthorized device is never selected as a working device; the guide explains authorization and offers retry.
ADB installation transfers the APK before invoking Package Manager (`--no-streaming`).
If USB drops during installation, reconnect/authorize the same device and use the retry prompt; a failed transfer never triggers an uninstall.

The source revision and pinned submodule commits must exist on the relevant Git remotes for a fresh clone to reproduce a build.
The guide never pushes your local source commits automatically.

Technical references: [Google SDK manager](https://developer.android.com/tools/sdkmanager),
[app-specific storage and uninstall behavior](https://developer.android.com/training/data-storage/app-specific),
[document picker](https://developer.android.com/training/data-storage/shared/documents-files),
[Temurin download API](https://api.adoptium.net/).

## Maintainer checks

```sh
python3 -m unittest discover -s android/tools/tests -v
```

Set `JAVA_HOME` to also run the host Java extractor checks (path traversal, corruption, interruption and preservation).
The same command works as `python -m unittest ...` on Windows.
Always inspect a normal `assembleDebug` after a private build: it must not contain any `assets/private-game-*` entries.
Do not upload test outputs containing real game files.
