# OpenGothic for Android

This project builds a sideloadable NativeActivity debug APK for 64-bit ARM Android devices with Vulkan 1.1. It does not package Gothic II game files. You must copy a legally owned Gothic II: Night of the Raven installation after installing the APK.

For measured S24 performance and repeatable profiling commands, see [PERFORMANCE.md](PERFORMANCE.md).

## Requirements

- Windows 10 or newer with PowerShell and Git.
- JDK 17.
- Android SDK command-line tools and platform-tools.
- Android SDK Platform 35, Build Tools 35.0.0, NDK 27.0.12077973, and CMake 3.22.1.
- A Vulkan 1.1 Android device. The Gradle project builds only `arm64-v8a`.

The Gradle wrapper downloads Gradle 8.9. The Android build also downloads a pinned official Khronos Vulkan-Headers archive during CMake configuration.

## Prepare the source and SDK

Clone with submodules, or initialize them in an existing checkout:

```powershell
git submodule update --init --recursive
```

Set the JDK and Android SDK paths for the current PowerShell session. Change these paths if your installations are elsewhere:

```powershell
$env:JAVA_HOME = 'C:\Android\jdk17\jdk-17.0.20.1+1'
$env:ANDROID_HOME = 'C:\Android\Sdk'
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
```

Install the required Android packages and accept their licenses:

```powershell
& "$env:ANDROID_HOME\cmdline-tools\latest\bin\sdkmanager.bat" `
  'platform-tools' `
  'platforms;android-35' `
  'build-tools;35.0.0' `
  'ndk;27.0.12077973' `
  'cmake;3.22.1'
& "$env:ANDROID_HOME\cmdline-tools\latest\bin\sdkmanager.bat" --licenses
```

## Build the APK

Run this from the repository root:

```powershell
Set-Location android
.\gradlew.bat --no-daemon assembleDebug
Set-Location ..
```

The resulting APK is `android\app\build\outputs\apk\debug\app-debug.apk`.

## Install and copy Gothic II

Enable Developer options and USB debugging on the phone, connect it, approve the debugging prompt, and confirm that ADB sees it:

```powershell
& "$env:ANDROID_HOME\platform-tools\adb.exe" devices -l
```

Install the APK, start it once to create its app-specific external directory, and stop it before copying files:

```powershell
$adb = "$env:ANDROID_HOME\platform-tools\adb.exe"
& $adb install -r 'android\app\build\outputs\apk\debug\app-debug.apk'
& $adb shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
& $adb shell am force-stop 'org.opengothic.app'
& $adb shell mkdir -p '/sdcard/Android/data/org.opengothic.app/files/Gothic2'
```

Copy the complete legally owned installation. This example uses the default Steam location:

```powershell
& $adb push 'C:\Program Files (x86)\Steam\steamapps\common\Gothic II\.' '/sdcard/Android/data/org.opengothic.app/files/Gothic2/'
```

OpenGothic automatically uses `/sdcard/Android/data/org.opengothic.app/files/Gothic2` as its game-data path. That directory should directly contain `Data`, `System`, and `_work`; do not create another `Gothic II` directory below it. Android removes this app-specific directory when the application is uninstalled, so keep the original PC installation.

If direct ADB access to `Android/data` is restricted by a device build, use Android Studio's Device Explorer to copy the files into the same app-specific directory while the debug APK is installed.

## Launch and collect logs

Launch or stop the application from PowerShell:

```powershell
& $adb shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
& $adb shell am force-stop 'org.opengothic.app'
```

Capture NativeActivity, Tempest, loader, and crash messages:

```powershell
& $adb logcat -c
& $adb shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
& $adb logcat -v threadtime 'Tempest:I' 'Vulkan:I' 'AndroidRuntime:E' 'libc:F' '*:S'
```

OpenGothic also writes `log.txt` beside the `Gothic2` directory. Pull it with:

```powershell
& $adb pull '/sdcard/Android/data/org.opengothic.app/files/log.txt' '.\opengothic-android-log.txt'
```

## Controls and current limitations

Android supports an optional text-only FPS counter in the top-left corner using Gothic's normal name-label font and its original warm text color, with screen-scaled padding. It is off by default; set `[GAME] showFps=1` in the writable `Gothic.ini` to enable it. It measures rendered frame intervals and requests a text refresh every 250 ms while rendering, including in menus. Video playback hides the counter.

See [Android configuration](CONFIGURATION.md) for the writable INI location, enabling quicksave/load and potion shortcuts, and which original Gothic settings affect this port.

Fresh Android installations use 75% scene resolution and half-resolution SSAO for a mobile performance/quality balance.
Android also initializes a missing writable `[ENGINE] zMaxFPS` to `60` and uses low-power fractional-frame waits.
Both are configurable: choose full resolution in the Resolution menu and set `[ENGINE] ssaoHalfResolution=0` for full-resolution AO.
Existing explicit choices are preserved, and desktop defaults are unchanged.

Menus, dialogue text, and inventory cells scale automatically to fit Gothic's 640x480 reference interface into 85% of the Android viewport. At 2340x1080 this gives a base scale of 1.9125. The existing `[INTERFACE]` `Scale` setting in `Gothic2/System/SystemPack.ini` multiplies that value: `1` uses the automatic size, `0.85` makes it smaller, and `1.1` makes it larger. Restart the game after editing it; very large values can clip menus. Desktop scaling is unchanged.

On wide Android screens, the inventory description occupies the space between the left and right grids so item rows can extend farther down. At 2340x1080 with the default five columns and 70-unit cells, this fits seven rows instead of five without shrinking cells or text. Narrow screens or wider column settings retain the bottom description panel. The FPS counter displays whole numbers.

To adjust the phone's own configuration from PowerShell without changing the PC installation:

```powershell
& $adb pull '/sdcard/Android/data/org.opengothic.app/files/Gothic2/System/SystemPack.ini' '.\SystemPack-android.ini'
notepad '.\SystemPack-android.ini'
& $adb push '.\SystemPack-android.ini' '/sdcard/Android/data/org.opengothic.app/files/Gothic2/System/SystemPack.ini'
```

OpenGothic uses invisible virtual touchscreen controls when no physical gamepad is active. Connecting a Bluetooth or USB gamepad disables these virtual zones and switches movement to the physical controller, but ordinary touch-as-mouse/UI input can still work.

The left half of the screen is a dynamic movement stick. Touch anywhere on that half to place its center, then drag relative to that point. The middle-right area, from 50% to 84% of the screen width, controls the camera. These areas support simultaneous touches, so the player can move and look at the same time. After 800 ms without manual camera input, moving forward gently recenters the camera behind the player. Gothic's existing focus system supplies interaction targeting and combat auto-rotation.

The rightmost 16% of the screen contains five invisible virtual buttons:

| Vertical area | Action | Menu behavior |
| --- | --- | --- |
| Top 20% | Back | Escape or close |
| 20% to 40% | Inventory | Inventory |
| 40% to 60% | Draw or sheathe weapon | Space |
| 60% to 80% | Jump | Left Alt |
| Bottom 20% | Interact or attack | Confirm |

Dragging the dynamic movement stick also emits arrow-key navigation for menus and dialog choices. This avoids precision tapping on the original desktop-sized UI.
While inventory or another UI is open, touch navigation does not feed movement or camera input into gameplay.
Release or center the movement stick after closing a UI before moving again; a held selection direction does not become character movement.

Choosing an empty save slot opens the Android keyboard. Enter a save name and press the keyboard's Done button to accept it and write the save. The keyboard stays hidden during normal gameplay and menu navigation.

Physical controllers now use camera-relative directional movement, A to accept/interact, B to go back, R3 target lock, and LB+L3 walk. Classic/modern combat, D-pad shortcuts, inventory panels, and the equipment wheel have context-specific mappings. See [controller controls and Gamepad.ini](CONTROLLER.md) for the complete layout, remapping commands, and device-test checklist.

Touch controls are normally invisible. For the temporary layout/debug overlay, enable `[DEBUG] touchControls=1` in the writable `Gothic.ini` and restart.
It draws Gothic-colored zone boundaries, action labels, pressed-button highlights and live finger anchors/trails.
The movement stick's outer square shows full axis travel; the inner square marks the arrow-key activation threshold.
Disable it with `touchControls=0`; it is off by default.
With an active gamepad, the overlay marks the virtual zones inactive and explains that touches pass through to normal mouse/UI handling.

With classic controls (`[GAME] useGothic1Controls=1`), draw a melee weapon, hold the bottom-right ACTION zone, then move the left stick from neutral: up attacks forward, down blocks, and left/right request side attacks.
Return the stick to neutral between directional presses; holding ACTION alone is not a directional strike.
Without a weapon, ACTION is interaction; menus use the same zone to confirm.
With Gothic 2 controls (`useGothic1Controls=0`), the bottom-right zone directly uses/attacks.
This debug view exposes the existing keyboard-style touch combat, not a redesigned mobile combat layout.
Right-side touch camera drag looks around without turning the character, respecting Mouse speed and vertical inversion.
The left movement stick still turns the character in place when dragged sideways.

Controller layouts vary, so Android may report different axes for some third-party devices. Ray queries and mesh shaders are disabled by default, and the build uses conservative desktop-compatible rendering paths for sustained mobile operation.

## Inspect the APK

These commands verify signing, manifest metadata, ABI contents, and 16 KiB ZIP alignment:

```powershell
$apk = 'android\app\build\outputs\apk\debug\app-debug.apk'
& "$env:ANDROID_HOME\build-tools\35.0.0\apksigner.bat" verify --verbose --print-certs $apk
& "$env:ANDROID_HOME\build-tools\35.0.0\aapt2.exe" dump badging $apk
& "$env:JAVA_HOME\bin\jar.exe" tf $apk | Select-String '^lib/'
& "$env:ANDROID_HOME\build-tools\35.0.0\zipalign.exe" -c -P 16 -v 4 $apk
```
