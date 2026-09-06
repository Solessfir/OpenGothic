# Controller milestone verification — 2026-09-06

Implementation: OpenGothic `bde76d57`, Tempest `98449c95` (published on the user's Tempest fork, branch `android`). OpenGothic commits remain local. See [the controller guide](CONTROLLER.md) for the implemented controls and remapping.

## Build and automated checks

- JDK 17, Gradle 8.9, SDK/target 35, minimum SDK 24, NDK 27.0.12077973, Android CMake 3.22.1.
- `assembleDebug lintDebug`: successful, including the ARM64 native link and APK assembly. Lint reports 0 errors and 2 warnings.
- Controller binding regression executable: passed through CTest. Covers chords, modifier release, remapping, invalid settings, UI isolation, tap/hold, and reconnect suppression.
- APK signature verification: passed (v2).
- APK ABI inspection: only `lib/arm64-v8a/libopengothic.so`; no Gothic assets packaged.
- Manifest: debuggable `org.opengothic.app`, `org.tempest.TempestNativeActivity`, sensor-landscape, game category, Vulkan 1.1.
- `zipalign -c -P 16 4`: passed.
- Windows game target: built with Visual Studio 2026 and the installed Vulkan SDK. This is a compile/link regression check, not a Windows controller test. The full unrelated Spacer target is not part of this check.

APK: `C:\Git\OpenGothic\android\app\build\outputs\apk\debug\app-debug.apk` (18,136,329 bytes).

SHA-256: `DF75CF39A9FD9EB084EB540E7B17C4A085BF6E99CD218E93780A264429872276`.

## Device verification blocker

The S24 (`RFCX10M60QT`, SM-S921B) appeared briefly in ADB, then the transport dropped before this controller APK could be installed. Windows continued to list the Samsung USB composite device and ADB interface with status OK. Host logs in `%TEMP%\adb.log` recorded:

```text
RFCX10M60QT: connection terminated: read failed
AdbUsbConnection: 1 - write terminated: No error
<unknown>: connection terminated: write failed
```

Restarting the ADB server and reconnecting offline transports did not restore an online device. Listings alternated between empty and `(no serial number) offline`. A restart of the specific ADB PnP interface was denied by Windows (`Access is denied`); no driver was changed. Wireless ADB discovery returned no services.

The earlier APK ran a new game on this S24, as confirmed by the user. That does **not** verify the new controller implementation. This build has not yet been installed or exercised with physical controller input. The new radial layout, directional movement, target lock, combat, and trade interactions need the device acceptance checks in CONTROLLER.md. The existing touch redesign remains a separate milestone.

## Smallest next steps

Unlock the phone, toggle Developer options → USB debugging off/on, and accept authorization if prompted. Do not uninstall the game or revoke all debugging authorizations just to update the APK. Once ADB lists the serial as `device`, save/exit the running game and use:

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
$serial = 'RFCX10M60QT'
& $adb devices -l
& $adb -s $serial install -r 'C:\Git\OpenGothic\android\app\build\outputs\apk\debug\app-debug.apk'
if ($LASTEXITCODE -ne 0) { throw 'Installation failed.' }
& $adb -s $serial shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
& $adb -s $serial shell cat '/sdcard/Android/data/org.opengothic.app/files/Gamepad.ini'
& $adb -s $serial shell cat '/sdcard/Android/data/org.opengothic.app/files/Gothic.ini'
& $adb -s $serial shell dumpsys input
& $adb -s $serial logcat -d -v threadtime 'Tempest:I' 'AndroidRuntime:E' 'libc:F' '*:S'
```

Confirm that the user's writable `[GAME] useQuickSaveKeys=1`, `usePotionKeys=1`, and `showFps=1` remain intact. They were enabled on an earlier connected-device check and are not packaged defaults. Start with menu A/B, directional movement, LB+L3, inventory panels, and wheel cancellation; then test combat/targeting from a normal save. Do not use successful compilation as a substitute for these play tests.
