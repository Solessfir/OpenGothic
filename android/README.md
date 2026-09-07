# OpenGothic for Android

Play Gothic II: Night of the Raven on an ARM64 Android device with Vulkan 1.1.
You need a legally owned installation; the normal APK contains no game files.
The app is named **Gothic II** (`org.opengothic.app`).

## Start here

Run `setup-android.bat` on Windows or `bash setup-android.sh` on Linux.
The guide can install missing tools, find your game files, build the APK and install it.

- [Guided setup and installation without USB](PRIVATE-SETUP.md)
- [Touch controls](TOUCH.md)
- [Gamepad controls and remapping](CONTROLLER.md)
- [Graphics, camera, UI size and other settings](CONFIGURATION.md)

Android defaults include landscape gameplay, 75% render resolution, half-resolution SSAO/fog,
automatic HDR where supported, and a 60 FPS cap. These are configurable; ray tracing and mesh shaders are off.
Audio pauses in the background, gameplay keeps the screen awake, and volume buttons control media volume.

**Update rather than uninstall:** uninstalling also removes game files, settings and saves.
Back up saves first. Never redistribute packages containing Gothic assets.

## Manual build

Requires JDK 17, Android SDK 35, Build Tools 35.0.0, NDK 27.0.12077973 and CMake 3.22.1.
The wrapper downloads Gradle 8.9; CMake downloads pinned Khronos Vulkan headers.
For automatic dependency installation, use the guided setup above.

From the repository root in PowerShell:

```powershell
git submodule update --init --recursive
$env:JAVA_HOME = 'C:/Path/To/jdk-17'
$env:ANDROID_HOME = 'C:/Path/To/Android/Sdk'
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" 'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' 'ndk;27.0.12077973' 'cmake;3.22.1'
& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" --licenses
./android/gradlew.bat -p android --no-daemon assembleRelease lintRelease
```

Output: `android/app/build/outputs/apk/release/app-release.apk` (ARM64, no Gothic assets).
On Linux, set `JAVA_HOME`/`ANDROID_HOME` and use `bash android/gradlew -p android assembleRelease lintRelease`.

Release uses native `-O3` and ThinLTO, Java/resource shrinking, and disables debugger support.
For native debugging, use `assembleDebug lintDebug`; output is `android/app/build/outputs/apk/debug/app-debug.apk`.
Both use the same local signing key by default. See [custom signing](PRIVATE-SETUP.md#signing) for distribution.

## Connect and install with ADB

Enable Developer options and USB debugging, connect the phone and approve its authorization prompt:

```powershell
$adb = "$env:ANDROID_HOME/platform-tools/adb.exe"
& $adb devices -l
$device = 'YOUR_DEVICE_SERIAL'
& $adb -s $device shell am force-stop org.opengothic.app
& $adb -s $device install --no-streaming -r android/app/build/outputs/apk/release/app-release.apk
if ($LASTEXITCODE -ne 0) { throw 'APK installation failed' }
& $adb -s $device shell am start -W -n org.opengothic.app/.SetupActivity
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

The easiest option is the [guided APK + ZIP installation](PRIVATE-SETUP.md#install-without-usb):
install the APK, launch it, choose `private-game.zip`, and wait for extraction.
Existing installations can reopen import through the launcher icon's **Import files** shortcut.
No ZArchiver or manual access to `Android/data` is needed.

For a direct ADB copy, launch once, close the app, and copy your installation:

```powershell
& $adb -s $device shell am force-stop org.opengothic.app
& $adb -s $device shell mkdir -p /sdcard/Android/data/org.opengothic.app/files/Gothic2
& $adb -s $device push 'C:/Path/To/Gothic II/.' /sdcard/Android/data/org.opengothic.app/files/Gothic2/
& $adb -s $device shell am start -W -n org.opengothic.app/org.tempest.TempestNativeActivity
```

`Gothic2` must directly contain `Data`, `_work` and `System` (or `system`).
If your device restricts direct access, use the archive importer.
Windows plugins such as Union DLLs do not run on Android.

## Transfer saves from PC

Only **OpenGothic** `save_slot_N.sav` files transfer; original Gothic II saves cannot be converted by renaming.
Use matching OpenGothic versions and game/mod data. Keep backups until the imported save loads successfully.

PC saves are in OpenGothic's working directory, usually beside `log.txt`.
Android saves are in `/sdcard/Android/data/org.opengothic.app/files/`, not its `Gothic2` subfolder.
Slot `0` is the quicksave. Close both games and choose an empty destination slot:

```powershell
$pcSave = 'C:/Path/To/OpenGothic/save_slot_1.sav'
$phoneSave = '/sdcard/Android/data/org.opengothic.app/files/save_slot_1.sav'
if (!(Test-Path -LiteralPath $pcSave -PathType Leaf)) { throw 'PC save not found' }
& $adb -s $device shell am force-stop org.opengothic.app
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the game' }
& $adb -s $device shell test ! -e $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Slot exists or ADB failed; choose an empty slot' }
& $adb -s $device push $pcSave $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Save transfer failed' }
& $adb -s $device shell am start -W -n org.opengothic.app/org.tempest.TempestNativeActivity
```

For phone-to-PC backups, use `setup-android.bat --backup-saves` (or the `.sh` equivalent).
Copy the backed-up files into PC OpenGothic's working directory without overwriting existing slots.
The guided setup can also include PC OpenGothic saves in an archive for transfer without USB.

## Troubleshooting

- Missing game files: check the directory layout above or import a ZIP produced by the setup guide.
- Controls: release fingers/center sticks after closing menus or reconnecting a controller.
- Low FPS or heat: lower render quality or set a 30 FPS cap in [configuration](CONFIGURATION.md).
- Save names: use the Android keyboard and press **Done**.
- ADB offline/unauthorized: reconnect and accept the phone prompt; for Wi-Fi, check the current connection port.

To collect logs:

```powershell
& $adb -s $device pull /sdcard/Android/data/org.opengothic.app/files/log.txt ./opengothic-android-log.txt
& $adb -s $device logcat -d -v threadtime 'app:I' 'Tempest:I' 'AndroidRuntime:E' 'libc:F' '*:S'
```

Include the OpenGothic revision, device/Android version, control mode and reproduction steps in a bug report.
Review logs for personal data before sharing. [Build checks and profiling](tools/README.md) are for contributors.
