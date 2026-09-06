# Controller milestone verification — 2026-09-06

## Latest follow-up

### Tempest master update and history cleanup

Tempest is now pinned to `b8c952ef1c723082a23c5e373e95f5ad65027419`, published on the user's fork. That merge contains all eight previously missing master commits through `02e1e053`; both local Tempest checkouts were fast-forwarded to it without conflicts. Android `assembleDebug lintDebug`, the Windows game target, and all three controller/save/camera regression tests passed. APK signature and 16 KB ZIP alignment checks passed, with only the ARM64 library and no game assets packaged. The rebuilt APK was subsequently installed successfully at the user's request and cold-launched with `Status: ok`. See [the S24 performance baseline](PERFORMANCE.md) for the subsequent in-game measurement.

Current APK SHA-256: `F01D1F1523303AE1610E5FC9893209617EC9C547F7B096ED025791B39198E67D`.

Before updating the engine, OpenGothic's local history was cleaned up: 19 dependency-only commits were consolidated into one fork/submodule commit, preserving all 41 OpenGothic code/documentation snapshots. The final tree was verified identical before the subsequent master update. Earlier OpenGothic hashes below describe the original verification history; the old history remains locally at `refs/backup/android-before-tempest-cleanup-20260906-192737`, with a local hash map in `build/history-cleanup/result.json`. Tempest history was not rewritten, OpenGothic was not pushed, and the ignored D-pad TODO was preserved.

### Sidestep animation selection

The user confirmed the animation problem persisted after the input-axis correction and identified excessive enemy distance as the separate lock-loss cause. Lock range/retention was not changed. OpenGothic `9c4747f5` disables automatic low-stick walk mode for locked ground sidesteps: crossing the low-speed range previously selected `WALKWSTRAFE` instead of `RUNSTRAFE`, and the animation layer can retain a sequence until its interruption window. Intentional LB+L3 walk mode, sneak, swimming/diving, and animation interruption rules are preserved. Locked forward/back movement and unlocked movement still support automatic walking.

Android `assembleDebug lintDebug`, the Windows game build, and CTest (3/3) passed. Regression checks cover automatic-walk suppression throughout sideways reversals and walk-threshold crossings, plus retained forward/back and unlocked walking. APK signature, ARM64-only contents, and 16 KB ZIP alignment checks passed; no game assets are packaged. Device animation verification remains pending; the APK has not been installed yet because confirmation that the game is saved/closed is outstanding.

Current APK SHA-256: `7C8965F193FAA5CF4249DC6BEE301DAC722820422196F4B7E37E65BB832BDDE1`.

Subsequently installed successfully on `RFCX10M60QT` at the user's request; cold launch returned `Status: ok`. This supersedes the pending-install note above. Gameplay verification remains with the user; no saves or controller inputs were modified during installation.

### Locked strafe reversals

After confirming the previous camera update works, the user reported temporary walking during rapid locked left/right reversals. OpenGothic `94accbd6` fixes a concrete input mismatch: sideways motion had an extra 0.2 component threshold after the radial dead zone/curve, while forward motion accepted any remaining vertical component. Locked ground input now chooses the dominant axis and accepts its effective sideways amount without a second threshold. Neutral input does not retain movement. Keyboard, touch, swimming/diving, and animation interruption rules are unchanged.

Android `assembleDebug lintDebug` and the Windows game target built successfully. CTest passed 3/3, including replayed left/right reversals with vertical noise, intentional forward/back input with horizontal noise, neutral input, and dominant-axis diagonals. APK v2 signing and 16 KB ZIP alignment checks passed; the APK contains only the ARM64 native library and no game assets. This validates the input correction, not yet the reported in-game animation timing. Installation and user play testing are pending confirmation that progress is saved and the game is closed.

Current APK SHA-256: `67B7F9CE28FBD3B845A5233CC2FA668FB95887B60FCAD9F5C42E86D72CC4426B`.

Subsequently installed successfully on `RFCX10M60QT` after the user closed the game; cold launch returned `Status: ok`. Gameplay testing remains with the user. D-pad quick-slot implementation was deferred for further wheel-binding discussion; the installed APK retains the existing D-pad layout.

### Locked camera and analog assistance

OpenGothic `ebe4101b` allows right-stick vertical look while locked, retaining Mouse speed, inversion, and pitch limits. Horizontal flicks still switch targets. The shared camera now wraps yaw near the character before applying pitch limits; previously an unwrapped yaw could be clamped to the wrong side of the character at an angle boundary. Unlocked automatic recentering scales with effective movement-stick magnitude. Locked tracking remains active at rest. Following uses exponential smoothing to keep its response consistent across frame rates.

Android `assembleDebug lintDebug` and the Windows game target built successfully. CTest passed 3/3, including new zero/±180-degree boundary tests, accumulated turns, gentle versus full-stick assistance, no-input behavior, overshoot, and 30 versus 120 Hz following. APK v2 signature and 16 KB ZIP alignment checks passed. The APK contains only the ARM64 native library and no game assets.

Current APK SHA-256: `25894A6EF1E8356E9713AD9B692B2957588006EC71DE607F67BDD615DB2CD038`.

After the user closed the game, this APK was installed successfully on `RFCX10M60QT`; cold launch returned `Status: ok`. It includes the save deletion change below. Gameplay verification of both changes is left to the user as requested; no controller inputs or save deletions were performed on the phone by the agent. The older pending-install notes below are historical.

### Save deletion

OpenGothic `05ad0fcd` adds `[UI] DeleteSave=X`. Only occupied save/load slots open the named confirmation; A confirms permanent deletion and B cancels. Keyboard Delete/Enter/Escape are supported too. Inventory X still drops items. Empty slots, directories, and symbolic links cannot be deleted as save files. The original assets are unchanged.

Android `assembleDebug lintDebug`, the Windows game build, and CTest (2/2) passed. The new deletion test uses synthetic temporary files and checks that neighboring saves and directories survive. APK v2 signing and 16 KB ZIP alignment checks passed; only the ARM64 native library is packaged, with no game assets. No user save was deleted in testing. Device testing is left to the user as requested; installation is pending confirmation that the running game is saved and ready to restart.

Current APK SHA-256: `A4FB141B426E9BF6BF167ADCB641AD12829CBC8CF0E70CB6C61CE57404DD1B01`.

### Save-name rendering

The earlier `832a86a9` APK was subsequently installed successfully on the S24 and cold-launched to the main menu. The user confirmed that live save-name editing works. The following rendering-fix record describes its build-time status; its pending-install note and APK hash are historical.

OpenGothic `832a86a9` adds live save-name rendering after the controller fixes in `15e1ec0c` and the dropped-item crash fix in `2a29bc29`. Tempest remains pinned to `b8f8b053` on the user's fork. These OpenGothic commits have not been pushed.

The user has now tested a physical controller and confirmed that target lock works; the subsequent missing-lock report was a test misunderstanding. No further targeting changes were made. The user also confirmed the previous fix worked, while reporting that save-name deletion only became visible after closing and reopening the menu.

Controller polling now runs from a timer outside rendering. Previously, controller menu input could enter a modal save-name dialog inside a render callback, preventing Android from rendering again until the dialog closed. The input dialog also explicitly invalidates the underlying menu that paints the name. This follow-up is built but has not yet been installed or visually verified on the phone: installation is waiting for the user to save progress and leave the running game.

Verification for this APK: Android `assembleDebug lintDebug` succeeded, the Windows `Gothic2Notr` target built, and the controller CTest passed (1/1). APK signature and 16 KB ZIP alignment checks passed. The APK contains only `lib/arm64-v8a/libopengothic.so` and no game assets.

APK: `android/app/build/outputs/apk/debug/app-debug.apk` (relative to the repository root).

SHA-256: `AD73A37FD8E3AE32190F72B7278226F6E0BF8A1C7BFED372A60230FE0BFA42D7`.

Next check: open an existing save-name editor using the controller, delete and replace text, and verify changes are visible before pressing Done. Cancel the edit to avoid overwriting a save during verification. User-assignable D-pad slots, weapon-slot draw behavior, and an explicit Fists slot remain proposals, not implemented controls.

The original milestone record below is historical; its APK hash and no-controller test status are superseded by this follow-up.

## Original milestone

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

APK: `android/app/build/outputs/apk/debug/app-debug.apk` (18,136,329 bytes).

SHA-256: `DF75CF39A9FD9EB084EB540E7B17C4A085BF6E99CD218E93780A264429872276`.

## Device verification

After the user reconnected the phone, ADB listed `RFCX10M60QT` as online. `adb install -r` succeeded at 17:53 on 2026-09-06, and a cold activity launch returned `Status: ok`. Package inspection confirmed `primaryCpuAbi=arm64-v8a`. The updated game reached the playable scene inside Xardas's tower in landscape (2340x1080); a screenshot showed the padded integer FPS counter reading 38 FPS. This is one observed frame-rate sample, not a sustained-performance measurement.

The app generated `Gamepad.ini` with the new bindings, including `Walk=LB+L3`. The writable `Gothic.ini` still contains `useQuickSaveKeys=1`, `usePotionKeys=1`, and `showFps=1`. Installation did not uninstall the app or remove its game files/saves.

Startup logs reported window initialization, resume, and focus gained. No AndroidRuntime exception or fatal native error appeared in the inspected process logs. The game log did contain asset-parser warnings and missing sound effects (`ENV_NIGHT_TONSOFINSECTS`, `OW_BIRD11`); these did not prevent reaching gameplay. Audio output was not assessed by listening in this check.

Android's input-device listing contained no physical gamepad at the time of inspection, so the app remained on its existing touch path. The new radial layout, directional controller movement, target lock, combat, and trade interactions still need the physical-controller acceptance checks in CONTROLLER.md. The touch redesign remains a separate milestone.

## Earlier USB connection failure

The S24 (`RFCX10M60QT`, SM-S921B) appeared briefly in ADB, then the transport dropped before this controller APK could be installed. Windows continued to list the Samsung USB composite device and ADB interface with status OK. Host logs in `%TEMP%\adb.log` recorded:

```text
RFCX10M60QT: connection terminated: read failed
AdbUsbConnection: 1 - write terminated: No error
<unknown>: connection terminated: write failed
```

Restarting the ADB server and reconnecting offline transports did not restore an online device. Listings alternated between empty and `(no serial number) offline`. A restart of the specific ADB PnP interface was denied by Windows (`Access is denied`); no driver was changed. Wireless ADB discovery returned no services.

The later successful install and gameplay check above supersede this installation blocker; the underlying intermittent USB failure has not been diagnosed conclusively.

## Smallest next steps

Connect a Bluetooth/USB gamepad to the phone and start with menu A/B, directional movement, LB+L3, inventory panels, and wheel cancellation; then test combat/targeting from a normal save. The new APK is already installed. For later reinstalls or diagnostics, save/exit the running game and use:

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
$serial = 'RFCX10M60QT'
& $adb devices -l
& $adb -s $serial install -r 'android/app/build/outputs/apk/debug/app-debug.apk'
if ($LASTEXITCODE -ne 0) { throw 'Installation failed.' }
& $adb -s $serial shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
& $adb -s $serial shell cat '/sdcard/Android/data/org.opengothic.app/files/Gamepad.ini'
& $adb -s $serial shell cat '/sdcard/Android/data/org.opengothic.app/files/Gothic.ini'
& $adb -s $serial shell dumpsys input
& $adb -s $serial logcat -d -v threadtime 'Tempest:I' 'AndroidRuntime:E' 'libc:F' '*:S'
```

The user's writable shortcut and FPS settings were rechecked after this install and remain enabled; they are not packaged defaults. Physical-controller play tests remain distinct from successful compilation and the no-gamepad startup check.
