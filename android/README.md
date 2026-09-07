# OpenGothic for Android

This project builds a sideloadable NativeActivity debug APK for 64-bit ARM Android devices with Vulkan 1.1. The normal Gradle build does not package Gothic II game files.

For guided prerequisite installation, private game-file packaging and phone installation, run **`setup-android.bat`** (Windows) or **`bash setup-android.sh`** (Linux) from the repository root.
See [the private setup guide](PRIVATE-SETUP.md), including the separate APK + ZIP fallback and optional OpenGothic save transfer.
The manual asset-free workflow below remains supported.

The installed app appears as **Gothic II** with a gold “G” on a black icon.
Its package ID remains `org.opengothic.app`, so existing installations, settings and saves are retained when updating.

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
$env:JAVA_HOME = 'C:\Path\To\jdk-17'
$env:ANDROID_HOME = 'C:\Path\To\Android\Sdk'
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

## Install without USB

For an APK and separate archive:

1. Transfer `OpenGothic-arm64.apk` and `private-game.zip` to the phone's **Downloads** folder using Wi-Fi, cloud storage or another private file-transfer method.
2. Open the APK in the phone's file manager (for example, Samsung **My Files**) and install it.
   Allow that source to install unknown apps if Android asks.
3. Launch **Gothic II**, tap **Choose private-game.zip**, and select the archive from Downloads.
4. Keep the setup screen open while the files are verified and extracted. The game starts automatically afterward.

Setup follows the phone's normal orientation. Landscape mode is enabled only when the game starts.

Use the `private-game.zip` produced by the [setup guide](PRIVATE-SETUP.md), not an arbitrary zipped game installation.
You do not need ZArchiver or manual access to `Android/data`.
If the game is already installed and opens directly, quit it and long-press its launcher icon → **Import files** to select an archive.

With `OpenGothic-PRIVATE-arm64.apk`, the game files are already bundled: install the APK and launch it; no separate ZIP is needed.
After successful extraction, you can delete the transferred ZIP and APK from Downloads.
Do not uninstall the app to reclaim that space: uninstalling also deletes its extracted game files, settings and saves.
Keep bundled game files private; do not redistribute them.

## Install with ADB wireless debugging

Android 11 or newer can install and debug the APK over the local Wi-Fi network without a USB cable.
Keep the phone and computer on the same network, then enable **Developer options → Wireless debugging**.

For the first connection, open **Pair device with pairing code** on the phone.
Use the IP address and pairing port shown in that dialog; enter the six-digit code when ADB asks:

```powershell
$adb = "$env:ANDROID_HOME\platform-tools\adb.exe"
& $adb pair '192.168.1.50:37123'
```

The pairing port is temporary and is not normally the port used by `adb connect`.
After pairing, find the `_adb-tls-connect._tcp` entry and connect to that address:

```powershell
& $adb mdns services
& $adb connect '192.168.1.50:38901'
& $adb devices -l
```

Pairing is retained, so later sessions normally need only `adb mdns services` and `adb connect`.
The IP address or connection port can change after Wi-Fi, the phone, or wireless debugging is restarted.
If mDNS discovery is unavailable, use the **IP address & port** shown on the main Wireless debugging screen.

Use the connected `IP:port` as the device selector when more than one device is listed, then install and launch normally:

```powershell
$device = '192.168.1.50:38901'
& $adb -s $device install --no-streaming -r 'android\app\build\outputs\apk\debug\app-debug.apk'
& $adb -s $device shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
```

`-r` updates the existing app and retains its game files, settings, and saves as long as the APK uses the same signing key.
If the phone offers **Disable adb authorization timeout**, enabling it prevents a trusted computer's authorization from expiring.
Use that setting only on a personal phone paired with a trusted computer; paired computers can be removed from the Wireless debugging screen.

## Install and copy Gothic II using USB

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

## Transfer saves from PC

These instructions apply to saves made by **OpenGothic on PC**, not the original Gothic II executable.
Original Gothic saves (such as `Saves\savegame1`) use a different format; this port does not import or convert them.
Use matching OpenGothic revisions and the same Gothic II installation/mod data on both devices where possible.
Very old OpenGothic saves can contain platform-dependent data; load and resave them on PC with a compatible recent build before transferring.
Keep the original files until you have successfully loaded the transferred save.

On Windows, OpenGothic writes `save_slot_<number>.sav` into its working directory, usually the folder it was launched from.
For a shortcut, check its **Start in** directory. Saves are beside OpenGothic's `log.txt`, not necessarily inside the Steam installation.
Each `.sav` contains the complete save, including its preview; no separate screenshot or original Gothic save directory is needed.
Slot `0` is the quicksave; use a numbered manual slot shown in the game's Load Game menu for normal transfers.

On Android, saves go directly into `/sdcard/Android/data/org.opengothic.app/files/`, **not** into its `Gothic2` game-data subdirectory.
Install and launch the APK once as described above before transferring.
Close OpenGothic on PC, then run this in PowerShell after replacing the serial, source path and destination slot:

```powershell
$adb = "$env:ANDROID_HOME\platform-tools\adb.exe"
& $adb devices -l
$device = 'YOUR_DEVICE_SERIAL'
$pcSave = 'C:\Path\To\OpenGothic\save_slot_1.sav'
$slot = 1 # Choose an empty manual slot in the phone's Load Game menu.
$phoneSave = "/sdcard/Android/data/org.opengothic.app/files/save_slot_$slot.sav"

if (!(Test-Path -LiteralPath $pcSave -PathType Leaf)) { throw 'PC save not found' }
& $adb -s $device shell am force-stop 'org.opengothic.app'
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the game; check the ADB connection' }
& $adb -s $device shell test ! -e $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Destination exists or ADB failed; choose an empty slot and check the connection' }
& $adb -s $device push $pcSave $phoneSave
if ($LASTEXITCODE -ne 0) { throw 'Save transfer failed' }
& $adb -s $device shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
```

Open **Load Game** and select the destination slot. The save keeps its original display name even if you change its slot number.
If loading fails, verify the OpenGothic versions and game/mod data, and collect `log.txt` using the next section.
Do not rename original Gothic save files to `.sav`; changing the extension does not convert them.

To back up an Android save or take it back to PC, stop the game first and pull that slot to a new backup folder:

```powershell
& $adb -s $device shell am force-stop 'org.opengothic.app'
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the game' }
$backup = New-Item -ItemType Directory -Path (Join-Path $PWD ('Android-save-backup-' + [guid]::NewGuid().ToString('N')))
& $adb -s $device pull $phoneSave $backup.FullName
if ($LASTEXITCODE -ne 0) { throw 'Save backup failed' }
```

With PC OpenGothic closed, copy the backed-up `.sav` into its working directory, choosing an empty slot and keeping any existing PC saves.
Repeat for each slot you want to preserve. Uninstalling the Android app removes its saves along with its copied game assets; an in-place `adb install -r` update normally preserves both.

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

At startup, the main menu highlights Load Game when a numbered OpenGothic save file exists, including a quicksave.
Without saves it keeps the script's normal default (New Game in Gothic II).
This only changes the initial highlight, not automatic loading; in-game menu defaults remain unchanged.

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

OpenGothic uses invisible virtual touchscreen controls when no physical gamepad is active. Connecting a Bluetooth or USB gamepad disables these virtual zones and switches movement to the physical controller. Gameplay touches are consumed instead of becoming mouse clicks or full-screen camera movement; the Android keyboard remains available for text entry.

The left half of the screen is a dynamic movement stick. Touch anywhere on that half to place its center, then drag relative to that point. The middle-right area, from 50% to 84% of the screen width, controls the camera. These areas support simultaneous touches, so the player can move and look at the same time. After 400 ms without manual camera input at the default follow speed, moving forward gently recenters the camera behind the player. Gothic's existing focus system supplies interaction targeting and combat auto-rotation.

Back, Inventory, Jump and Draw occupy the rightmost 16% of the screen.
The frequently used Attack/Use area is wider and taller: the rightmost 24% across the bottom 30%, including menu confirmation.
This lower-right extension takes priority over camera input; the remaining middle-right area still controls the camera.

| Vertical area | Action | Menu behavior |
| --- | --- | --- |
| Top 20% | Back | Escape or close |
| 20% to 40% | Inventory | Inventory |
| 40% to 55% | Jump | Left Alt |
| 55% to 70% | Draw or sheathe weapon | Space |
| Bottom 30% (rightmost 24%) | Interact or attack | Confirm |

In menus, drag on either the movement or camera area: up/down selects, right accepts and left goes back one level.
Gamepad menu navigation uses the same behavior. Sliders and choice settings retain left/right adjustment; inventory grids retain directional cell selection.
Returning the stick to neutral before another left/right action prevents a held direction from repeatedly entering or closing menu levels.
While inventory or another UI is open, touch navigation does not feed movement or camera input into gameplay.
Release or center the movement stick after closing a UI before moving again; a held selection direction does not become character movement.
In the journal, Accept opens a category or quest, and Back returns one level: description, quest list, categories, then gameplay.
Use up/down on the touch movement area to select quests or scroll their descriptions.

Choosing an empty save slot opens the Android keyboard. Enter a save name and press the keyboard's Done button to accept it and write the save. The keyboard stays hidden during normal gameplay and menu navigation.

Physical controllers now use camera-relative directional movement, A to accept/interact, B to go back, R3 target lock, and LB+L3 walk. Classic/modern combat, D-pad shortcuts, inventory panels, and the equipment wheel have context-specific mappings. See [controller controls and Gamepad.ini](CONTROLLER.md) for the complete layout, remapping commands, and device-test checklist.
Android touch and gamepad also face the focused NPC when starting an unlocked melee attack, within a 90-degree turn and 300-world-unit distance by default.
This does not lunge, extend attack reach, or activate target lock. Disable or tune it with the `[Combat]` settings documented in [Gamepad.ini controls](CONTROLLER.md).
With fists or a melee weapon drawn, NPC name/health display and target-lock range are doubled on Android.
`[Combat] MeleeFocusRangeScale=1` in `Gamepad.ini` restores Gothic's original range; the default is `2`.
This range setting is separate from the shorter melee facing-assist distance and does not affect weapon reach or item pickup.

Touch controls, including the contextual G2 Block hit area, are invisible by default.
For the temporary layout/debug overlay, enable `[DEBUG] touchControls=1` in the writable `Gothic.ini` and restart.
It draws Gothic-colored zone boundaries, action labels, pressed-button highlights and live finger anchors/trails.
The debug status and gesture hints are grouped into aligned columns at the top of the movement and camera areas.
The movement stick's outer square shows full axis travel; the inner square marks the menu/classic attack direction threshold.
With a melee weapon drawn in G1 controls, a narrow lower cone also shows the deliberate block-entry region.
Disable it with `touchControls=0`; it is off by default.
With an active gamepad, the overlay marks the virtual zones inactive and explains that gameplay touches are ignored while Android text input remains available.

With classic controls (`[GAME] useGothic1Controls=1`), draw a melee weapon, hold the bottom-right ACTION zone, then move the left stick from neutral: up attacks forward, down blocks, and left/right request side attacks.
Return the stick to neutral between directional presses; holding ACTION alone is not a directional strike.
Blocking requires at least 65% downward stick travel within roughly 22 degrees of straight down.
Small downward deviations during side attacks no longer block. Forward and side attacks choose the dominant direction.
An active block has a small release margin (55% depth and a slightly wider cone) to avoid flickering at the boundary.
To finish a knocked-out NPC with a one-handed or two-handed melee weapon, focus them and hold ACTION, then push forward from neutral.
With G2 controls, press ACTION instead; fists cannot perform the finishing move.
Without a weapon, ACTION is interaction; menus use the same zone to confirm.
With Gothic 2 controls (`useGothic1Controls=0`), the bottom-right zone directly uses/attacks.
With fists or a melee weapon drawn, a separate Block hit area is enabled to the left of Draw/Sheathe, above Use/Attack.
Hold it to request Gothic's normal block/parry action and release to stop; this is not an unconditional damage shield.
The hit area is disabled in menus, with ranged weapons/magic, in classic controls, and when touch controls are inactive.
Changing context also releases a held block, and a touch started on Block never becomes a camera drag.
Its gold crossed-blade debug icon is original vector geometry drawn by OpenGothic and is shown only with `touchControls=1`.
This debug view exposes the existing keyboard-style touch combat, not a redesigned mobile combat layout.
Right-side touch camera drag looks around without turning the character, respecting Mouse speed and vertical inversion.
The left movement stick still turns the character in place when dragged sideways.
Turning now scales with sideways deflection instead of switching to full speed at an arrow-key threshold.
It uses `[Axes] TouchMovementDeadZone=0.15`, `MovementExponent=1.5` and `TouchTurnSpeed=180` from `Gamepad.ini`.
Forward/back and turning have independent dead zones, so small finger wobble while turning does not start walking.
Gentle forward/back input walks; a short deliberate drag runs.
`[Axes] TouchWalkThreshold=0.35` enters running at 39% drag and returns to walking below 31%, using the shared anti-flicker margin.
Gamepad retains its separate threshold; see [movement settings](CONTROLLER.md) for tuning.
Forward speed still comes from Gothic's walk/run animations; combat and animation interruption timing are unchanged.
Camera dragging remains proportional to finger travel and uses Gothic's Mouse speed setting.
While target-locked, it instead uses the gamepad's target-relative movement: sideways strafes, up approaches, and down retreats.
The touch dead zone, shared response curve and dominant-axis filtering apply, without switching walk animations during sidesteps.
G1's held ACTION plus direction combat still takes priority over movement; release ACTION to resume locked strafing.
Hold the touch Draw/Sheathe area for 400 ms to open the equipment wheel.
Keep the same finger down, drag toward an item's direction, then release to equip and draw it.
It uses the existing eligible equipment list: owned melee/ranged weapons and assigned spells that satisfy Gothic's requirements.
Hold Menu/Back for 400 ms to open the character wheel: Character Stats at the top, Journal at the bottom.
Drag the same finger up or down and release to open the corresponding entry.
Short taps still draw/sheathe or open/close the menu; holding does not also trigger the tap action.

Wheels appear in the center of the screen, away from the finger holding Draw or Menu/Back.
The finger's position when the wheel opens becomes the input origin.
Keep holding and move that finger a little toward a choice's direction, then release to apply it.
For example, a short upward drag selects the top item in the centered wheel without reaching across the screen.
Selection begins beyond 1/40 of the shorter screen dimension (27 pixels at 1080 pixels high, with a minimum of 16).
The HUD and enabled touch debug visuals remain visible while selecting.
Touch and gamepad wheels share a compact circular layout with evenly spaced choices, a highlighted selection and Gothic-colored borders.
The title appears above the wheel; selected item names and page controls, when needed, appear below it.
The two-entry character wheel is smaller than a full equipment wheel.
Release without moving, return the held finger to its starting position, or select an empty sector to cancel.
Longer drags retain their direction even beyond the wheel's drawn edge.
For equipment with more than eight entries, each page has six items plus Previous/Next sectors.
Hold the finger in a page sector's direction for 500 ms, return near its starting position to rearm selection, then choose an item without lifting.
Releasing on a page sector cancels without equipping anything.
Other touch controls stop while a wheel is open; release and touch again to resume movement.
Focus loss, gamepad takeover, resizing, loading, and another modal UI cancel the gesture without applying it.
The world continues running while the wheel is open, as with the gamepad wheel.
These wheels are visible on demand even when the normal touch zones and debug overlay are hidden.
Touch ACTION and Inventory holds are not wheel gestures.

Two-finger swipes work during gameplay in the movement and camera areas:

| Starting area | Swipe | Action |
| --- | --- | --- |
| Left half | Down | Enable sneak, if Gothic allows it |
| Left half | Up | Leave sneak mode |
| Right half | Up | Toggle first-person view |
| Right half | Down, then keep both fingers held | Look behind until either finger lifts |
| Anywhere, including across both halves | Left | Use Gothic's health-potion shortcut |
| Anywhere, including across both halves | Right | Use Gothic's mana-potion shortcut |

Put both fingers down within 350 ms, before moving the first finger significantly.
Move both in the same direction by at least 1/18 of the shorter screen dimension (60 pixels on a 1080-pixel-high viewport) within 700 ms.
Vertical swipes require both fingers to start on the same half. Horizontal potion swipes can start anywhere, including buttons or opposite halves.
Potions use Gothic's existing selection and consumption logic and respect `[GAME] usePotionKeys`; `0` disables them.
Once paired, the fingers stop driving movement/camera input and trigger at most one gesture; lift both before starting another.
Pinches, diagonal swipes without a clear axis, late second fingers and one-finger drags do not trigger these actions.
Menus, radial wheels and gamepad mode do not recognize these swipes.
Focus loss, resizing or leaving gameplay cancels the gesture and releases look-behind.
Sneak uses Gothic's existing skill/state restrictions; up/down explicitly disables/enables it instead of toggling.

Three-finger tap quicksaves; four-finger tap quickloads immediately without a confirmation dialog.
Both respect `[GAME] useQuickSaveKeys=1`; setting it to `0` disables these gestures as well as controller quicksave/load.
Fingers can start anywhere, including action buttons, and may span both halves of the screen.
Place all three or four fingers within 350 ms, keep them nearly stationary, then lift all within 800 ms of the first touch.
Small finger drift is allowed up to 1/30 of the shorter screen dimension (36 pixels at 1080 pixels high, with a minimum of 24).
Nothing fires until every finger is up, so a four-finger tap cannot first overwrite the quicksave.
Extra fingers, appreciable movement, long holds, or fingers added after release starts cancel the tap.
Single button taps fire on release. Held buttons wait through the 350 ms joining window while a tap candidate remains valid,
so fingers placed over Inventory, Jump or Use do not also trigger those buttons before a multi-finger gesture is recognized.
Menus, radial wheels, loading, focus loss, and gamepad takeover cannot apply a pending tap.

In the save/load menu, select a slot and use a three-finger tap anywhere to request deletion.
The confirmation names the selected save: tap the bottom-right Accept/Use area to permanently delete it, or the top-right Back area to cancel.
The three-finger tap itself never deletes a file, and all fingers must lift before the confirmation appears.
Empty slots do nothing; four-finger taps do nothing in this menu, and neither gameplay quicksave nor quickload runs there.
Save-name editing and an already-open deletion confirmation do not accept deletion gestures.
This action is independent of `useQuickSaveKeys`, which controls gameplay quicksave/load only.
Back up important saves: confirmed deletion has no undo.

Swimming uses camera-relative movement on both touch and gamepad.
Underwater, forward swims where the camera looks, backward reverses that direction, and sideways movement stays level.
Hold Jump at the surface to dive downward; release it, then hold again underwater to rise, even with movement centered.
Reaching the surface while still holding Jump does not trigger another dive.
Use the invisible touch Jump zone or the gamepad's Jump binding (X by default).
Camera assistance is disabled in water, and the old ACTION/directional combat keys do not consume swimming movement.
Gothic's collision, oxygen and swim animations remain in use; keyboard swimming is unchanged.

With a weapon drawn, start in Use/Attack and quickly drag at least 1/18 of the short screen dimension (60 pixels on a 1080-pixel-high viewport) to toggle target lock.
The same gesture unlocks an already locked target, and each gesture toggles only once.
This uses Gothic's existing eligible-NPC focus/lock rules and the existing ` (locked)` name suffix; it does not select arbitrary scenery or require a new targeting system.
The lock gesture must cross the threshold within 180 ms; releasing without a drag sends a normal action tap.
An ordinary held action starts after the multi-finger joining window if the finger remains stationary.
Once the hold has committed, further dragging does not toggle lock; lift and start a new gesture to lock or unlock.
This distinguishes an early lock drag from a held action and a multi-finger gesture.
While locked, the camera follows the character toward the target and vertical camera drag still adjusts elevation; horizontal free look resumes after unlocking.
Android's ordinary third-person camera sits higher along its orbit and pitches downward toward the character and ground ahead.
Set `[GAME] cameraElevationOffset` in the writable `Gothic.ini` to tune the extra elevation from `0` to `30` degrees; the default is `10`, and `0` restores the original framing.
Exploration uses this offset; melee, ranged and magic combat use `[GAME] cameraCombatElevationOffset=20` by default for better close-range visibility, also adjustable from `0` to `30` degrees.
Both use the existing camera following and collision handling and work with existing saves without accumulating on reload.
Drawing or sheathing a weapon preserves your manual pitch adjustment while switching between these offsets.
Combat elevation and zoom changes blend smoothly without slowing manual look or movement follow.
Inventory keeps its original camera framing and smoothing, and closing it restores your manual pitch; radial wheels leave the gameplay camera active.
`[GAME] cameraFollowSpeed=2` doubles the camera's following response, approximately halving its smoothing lag for touch and gamepad.
It also halves the automatic recentering delay to 400 ms; use `1` to restore the prior response. Manual look sensitivity is unchanged.
Manual look remains available. First-person, dialogue, swimming, diving, cutscenes and desktop camera defaults are unchanged.
The debug overlay labels the gesture as `DRAG: LOCK` or `DRAG: UNLOCK`.

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
