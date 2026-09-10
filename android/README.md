# OpenGothic for Android

Native Android port of OpenGothic for Gothic 1, Gothic II Classic and Gothic II: Night of the Raven.
Requires an ARM64 device with Vulkan 1.1 and a legally owned game installation.
The normal APK contains no game files.

## Don't care - let me play

1. Download your game's APK from [Releases](https://github.com/Solessfir/OpenGothic/releases).
2. Run `setup-android.bat --package-only` on Windows or `bash setup-android.sh --package-only` on Linux. Choose the game and its installation, then copy `build/android-setup/game-data.zip` to your phone.
3. Open the app, tap **Choose game archive**, and select the archive.
4. Wait for the import to finish and the game to start. You can then delete the ZIP from your phone.

The script collects the required files and adds an import index. Zipping only `Data`, or manually zipping the installation, will not work. No USB is needed.
All three apps can be installed together, with separate game files, saves and settings.

Use a clean installation without Union or Windows DLL plugins: they are unsupported, and their script/asset changes can break the game.
Classic needs actual Classic files; removing NotR's addon archives or selecting a different APK does not convert the installation.
To build the APK yourself instead, run `setup-android.bat` on Windows or `bash setup-android.sh` on Linux and follow the prompts.
Both scripts detect installed games and ask which edition to install.

- [Setup options and installation without USB](SETUP.md)
- [Touch controls](TOUCH.md)
- [Gamepad controls and remapping](CONTROLLER.md)
- [Graphics, camera, UI size and other settings](CONFIGURATION.md)

Android defaults include landscape gameplay, full render resolution, half-resolution SSAO/fog, automatic HDR where supported, and a 60 FPS cap. Video settings include a 50-100% resolution slider and fog, SSAO and shadow quality controls; ray tracing and mesh shaders are off.
Audio pauses in the background, gameplay keeps the screen awake, and volume buttons control media volume.

**Update rather than uninstall:** uninstalling also removes game files, settings and saves.
Back up saves first. Never redistribute packages containing Gothic assets.

## Manual build

Requires JDK 17, Android SDK 35, Build Tools 35.0.0, NDK 27.0.12077973 and CMake 3.22.1.
The wrapper downloads Gradle 8.9; CMake downloads pinned Khronos Vulkan headers.
The setup scripts can install these dependencies automatically.

From the repository root in PowerShell:

```powershell
git submodule update --init --recursive
$env:JAVA_HOME = 'C:/Path/To/jdk-17'
$env:ANDROID_HOME = 'C:/Path/To/Android/Sdk'
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" 'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' 'ndk;27.0.12077973' 'cmake;3.22.1'
& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" --licenses
& "$env:ANDROID_HOME/cmake/3.22.1/bin/cmake.exe" -S android -B build/android -G Ninja "-DCMAKE_MAKE_PROGRAM=$env:ANDROID_HOME/cmake/3.22.1/bin/ninja.exe"
& "$env:ANDROID_HOME/cmake/3.22.1/bin/cmake.exe" --build build/android --target OpenGothic-apk
```

Output: `build/android/OpenGothic/app/build/outputs/apk/release/app-release.apk` (ARM64, no Gothic assets).
NotR is the default build.
For Gothic 1, configure with `-B build/android-g1 -DOPENGOTHIC_ANDROID_GAME=gothic1`; for Classic, use `-B build/android-g2-classic -DOPENGOTHIC_ANDROID_GAME=gothic2`.
Build that directory instead; its APK is under `OpenGothic/app/build/outputs/apk/release/app-release.apk`.
The setup scripts copy APKs into `build/android-setup` using the edition-specific names above.
On Linux, set `JAVA_HOME`/`ANDROID_HOME`, then use `cmake -S android -B build/android -G Ninja`
and `cmake --build build/android --target OpenGothic-apk` with CMake 3.22+ and Ninja on PATH.
Tempest generates the Gradle project under `build/android/OpenGothic`; do not edit generated files.

Release uses native `-O3` and ThinLTO, Java/resource shrinking, and disables debugger support.
For native debugging, add `-DTEMPEST_ANDROID_BUILD_TYPE=Debug` to the configure command.
The APK is then under `build/android/OpenGothic/app/build/outputs/apk/debug/app-debug.apk`.
Both use the same local signing key by default. See [custom signing](SETUP.md#signing) for distribution.

## Connect and install with ADB

Enable Developer options and USB debugging, connect the phone and approve its authorization prompt:

```powershell
$adb = "$env:ANDROID_HOME/platform-tools/adb.exe"
& $adb devices -l
$device = 'YOUR_DEVICE_SERIAL'
& $adb -s $device shell am force-stop org.opengothic.gothic2notr
& $adb -s $device install --no-streaming -r build/android/OpenGothic/app/build/outputs/apk/release/app-release.apk
if ($LASTEXITCODE -ne 0) { throw 'APK installation failed' }
& $adb -s $device shell am start -W -n org.opengothic.gothic2notr/org.opengothic.app.SetupActivity
```

Keep the same debug signing key (`~/.android/debug.keystore`) for updates.
If installation reports `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, restore the original key; do not uninstall without backing up app data.

### Wireless debugging

On Android 11+, put both devices on the same trusted Wi-Fi network.
Enable **Developer options → Wireless debugging → Pair device with pairing code**.

```powershell
& $adb pair 'PHONE_IP:PAIRING_PORT'
# Enter the code shown on the phone.
& $adb mdns services
& $adb connect 'PHONE_IP:CONNECTION_PORT'
& $adb devices -l
$device = 'PHONE_IP:CONNECTION_PORT'
```

The pairing port and connection port are different.
Use the `_adb-tls-connect._tcp` address from discovery, or **IP address & port** on the main Wireless debugging screen.
Pairing is retained, but the connection port can change after reconnecting. Then use the same install commands above.

If available, **Disable adb authorization timeout** keeps a trusted computer authorized.
Use it only with trusted computers; remove old pairings from the phone when no longer needed.

## Import game files

For [APK + ZIP installation](SETUP.md#install-without-usb),
install the APK, launch it, choose `game-data.zip`, and wait for extraction.
Existing installations can reopen import through the launcher icon's **Import files** shortcut.
No ZArchiver or manual access to `Android/data` is needed.

For a direct ADB copy, launch once, close the app, and copy your installation:

```powershell
& $adb -s $device shell am force-stop org.opengothic.gothic2notr
& $adb -s $device shell mkdir -p /sdcard/Android/data/org.opengothic.gothic2notr/files/Gothic2
& $adb -s $device push 'C:/Path/To/Gothic II/.' /sdcard/Android/data/org.opengothic.gothic2notr/files/Gothic2/
& $adb -s $device shell am start -W -n org.opengothic.gothic2notr/org.tempest.TempestNativeActivity
```

`Gothic2` must directly contain `Data`, `_work` and `System` (or `system`).
The same folder is used for Gothic 1; the game is detected from its world files, not the folder name or language.
Use each game's own app and do not mix assets or saves; see [game editions and storage](SETUP.md#game-editions-and-storage).
If your device restricts direct access, use the archive importer.
Windows plugins such as Union DLLs do not run on Android.

## Transfer saves from PC

Only **OpenGothic** `save_slot_N.sav` files transfer; original Gothic 1 or Gothic II saves cannot be converted by renaming.
Use matching OpenGothic versions and game/mod data. Keep backups until the imported save loads successfully.

PC saves are in OpenGothic's working directory, usually beside `log.txt`.
Android saves are in `/sdcard/Android/data/org.opengothic.gothic2notr/files/`, not its `Gothic2` subfolder.
For Gothic 1 or Classic, substitute the corresponding app ID from the [edition table](SETUP.md#game-editions-and-storage).
Slot `0` is the quicksave. Close both games and choose an empty destination slot:

```powershell
$pcSave = 'C:/Path/To/OpenGothic/save_slot_1.sav'
$phoneSave = '/sdcard/Android/data/org.opengothic.gothic2notr/files/save_slot_1.sav'
if (!(Test-Path -LiteralPath $pcSave -PathType Leaf)) { throw 'PC save not found' }
& $adb -s $device shell am force-stop org.opengothic.gothic2notr
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the game' }
& $adb -s $device shell test ! -e $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Slot exists or ADB failed; choose an empty slot' }
& $adb -s $device push $pcSave $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Save transfer failed' }
& $adb -s $device shell am start -W -n org.opengothic.gothic2notr/org.tempest.TempestNativeActivity
```

For phone-to-PC backups, use `setup-android.bat --backup-saves` (or the `.sh` equivalent).
Copy the backed-up files into PC OpenGothic's working directory without overwriting existing slots.
The setup scripts can also include PC OpenGothic saves in an archive for transfer without USB.

## Troubleshooting

- Missing game files: check the directory layout above or import a ZIP produced by the setup guide.
- Controls: release fingers/center sticks after closing menus or reconnecting a controller.
- Low FPS or heat: lower render quality or set a 30 FPS cap in [configuration](CONFIGURATION.md).
- Save names: use the Android keyboard and press **Done**.
- ADB offline/unauthorized: reconnect and accept the phone prompt; for Wi-Fi, check the current connection port.

To collect logs:

```powershell
& $adb -s $device pull /sdcard/Android/data/org.opengothic.gothic2notr/files/log.txt ./opengothic-android-log.txt
& $adb -s $device logcat -d -v threadtime 'app:I' 'Tempest:I' 'AndroidRuntime:E' 'libc:F' '*:S'
```

Include the OpenGothic revision, device/Android version, control mode and reproduction steps in a bug report.
Review logs for personal data before sharing. [Build checks and profiling](tools/README.md) are for contributors.
