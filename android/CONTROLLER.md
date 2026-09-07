# Android controller controls

The native controller path uses semantic game actions, not simulated PC key bindings. Windows keyboard/mouse controls remain unchanged; this does not add a Windows controller backend. Xbox button names are used below. L3 and R3 mean clicking the left and right sticks.

Disconnecting the active gamepad clears held input and opens the pause menu during a game.
An existing menu stays open; reconnecting never resumes automatically.
If disconnection happens while the app is unfocused or loading, the pause menu is deferred until the app is focused and loading finishes.
You can explicitly resume with touch or a reconnected controller.

## Movement and shared shortcuts

| Control | Action |
| --- | --- |
| Left stick | Camera-relative directional movement; light tilt walks, full tilt runs |
| Right stick | Camera; flick left/right to switch targets while locked |
| R3 | Toggle target lock with a drawn weapon |
| L3 | Toggle sneak, if learned |
| LB + L3 | Toggle walk; does not also toggle sneak |
| A | Interact, pick up, accept |
| B | Back, skip a dialogue line, close the active window |
| X | Jump |
| Y | Journal |
| Menu / Start | Pause menu |
| View / Select | Inventory |
| D-pad Up, tap | Draw or sheathe the last weapon/spell |
| D-pad Up, hold | Equipment wheel |
| D-pad Down | Map, through the game's map hotkey script |
| D-pad Left / Right | Health / mana hotkey scripts |
| LB + D-pad Up | Toggle first person |
| LB + D-pad Down, hold | Look behind |
| LB + D-pad Left / Right | Character stats / journal |
| LB + Menu | Quicksave |
| LB + View | Quickload immediately, without confirmation |

Unlocked movement turns toward the chosen direction instead of strafing. Target lock retains an eligible NPC selected by Gothic's existing focus rules; it does not use a new enemy-scoring or distance system. While locked, horizontal movement strafes and the character/camera face the target. The name gains a ` (locked)` suffix. Retention uses the existing cached-focus rules instead of rechecking the acquisition angle every frame. Death, unconsciousness, sheathing, or loss of eligibility releases it. Swimming and diving retain Gothic's native movement constraints. The original `Gothic.ini` `keyLockTarget` now dispatches this same lock action for keyboard input.

While locked, right-stick up/down still adjusts camera elevation, respecting Mouse speed, vertical inversion, and Gothic's pitch limits. Horizontal stick flicks still switch targets. Camera yaw wraps across zero and ±180 degrees before following, so an equivalent angle does not force a long rotation.

Locked ground movement chooses the left stick's dominant axis for Gothic's movement animation. Small vertical noise during a left/right reversal therefore cannot start a forward walk. Both directions use the same radial dead zone and response curve, without an extra sideways threshold. Neutral input stops requesting movement, and intentionally pushing forward/back still advances/retreats. Target-locked touch movement uses these same rules, except while holding the classic ACTION modifier for directional attacks. Swimming, diving, and keyboard movement retain their existing handling.

Automatic low-stick walking applies to unlocked movement and locked forward/back movement, not locked sidesteps. This keeps a reversal from switching between Gothic's run-strafe and walk-strafe animations as the stick passes through the low-speed range. Combat sidesteps use their normal animation speed even at a small sideways deflection. Explicit walk mode (LB+L3), sneak, water movement, and the original animation interruption rules remain intact.

Left-stick responsiveness is separate from Mouse speed. These `Gamepad.ini` options soften movement without reducing full-stick running speed:

```ini
[Axes]
MovementDeadZone=0.28
MovementExponent=1.5
MovementTurnSpeed=180
WalkThreshold=0.65
```

`MovementDeadZone` ignores small deflections. An exponent above `1` makes partial tilts gentler and extends the walking region. `MovementTurnSpeed` limits unlocked character turning in degrees per second (the first prototype used 360). Mouse speed continues to control the camera only. Missing keys inherit these defaults, including in an existing Gamepad.ini; restart after editing.

The existing Mouse speed setting (`[GAME] mouseSensitivity`) also scales the controller camera. `camLookaroundInverse` controls vertical inversion. Camera assistance waits 800 ms after manual input before recentering during movement; disable it with `[Controller] CameraAssist=0` in `Gamepad.ini`.

Android touch and gamepad share an optional unlocked melee facing assist for both Gothic control modes.
At the start of a punch or sword attack, the character faces the currently focused living NPC if Gothic still considers it eligible.
It does not acquire another target, enable persistent lock, move the camera, extend weapon reach, or move the player toward the enemy.
Already-running attack animations, blocking, finishers, ranged attacks, and manual target lock retain their existing behavior.
Desktop keyboard/mouse combat is unchanged.

These `Gamepad.ini` defaults also apply to existing files that omit the section:

```ini
[Combat]
MeleeAssist=1
MeleeAssistMaxAngle=90
MeleeAssistMaxDistance=300
```

`MeleeAssist=0` disables it. The angle is the maximum turn from the character's facing in degrees (0–180).
Distance is in Gothic world units: 300 is approximately three meters, not a new attack reach.
Gothic's focus and visibility checks must still pass; these limits can restrict eligibility, not expand it.
Restart after editing. This assist is separate from camera assistance and mouse sensitivity.

Unlocked camera assistance scales with the left stick's effective movement amount after its dead zone and response curve: small deflections recenter gently, full deflection gives full assistance, and releasing the stick stops assistance. Locked tracking remains active while stationary. Both use frame-rate-independent smoothing controlled by `[TargetLock] CameraSmoothingSeconds`; mouse sensitivity still controls manual camera input, not movement-stick assistance.

Quicksave/load require `[GAME] useQuickSaveKeys=1` in `Gothic.ini`. Potion shortcuts require `usePotionKeys=1`. Disabled shortcuts remain consumed: LB+View never falls through to opening inventory. Potion selection and restrictions come from the installed Gothic scripts, exactly as with the keyboard hotkeys. No separate potion-selection policy is added. See [configuration](CONFIGURATION.md) for safely editing these flags.

## Combat

The original `useGothic1Controls` setting selects the melee context.

| Context | Controls |
| --- | --- |
| Classic melee, weapon drawn | Y: forward attack; X: left attack; B: right attack; hold A: block |
| Modern melee, weapon drawn | RT: attack; hold RB: block |
| Bow, crossbow, or spell drawn | RT: shoot/cast; hold for spells that invest mana |
| Melee finisher | Hold Y in classic melee or RT in modern melee over a finishable NPC |

Holding LT restores exploration face buttons during classic melee. LT+A requests sheathing first, then interacts once with the same still-valid target. With no target it only sheathes; it does not redraw automatically. Ordinary A cannot pick up items while the weapon is drawn. Fists and weapons still obey Gothic's animation, ammunition, skill, and combat restrictions. Moving with a drawn weapon does not implicitly hold the classic action modifier.

Default gamepad blocking uses a dedicated button (A in classic melee, RB in modern melee), not downward stick movement.
The narrow downward block cone described in the touch controls applies to G1 touch ACTION-plus-direction combat; gamepad buttons and keyboard combat are unchanged.

Finishing uses the normal `AttackForward` binding and `[Controller] HoldMs` (400 ms by default).
Begin the hold while focusing a knocked-out NPC with a one-handed or two-handed melee weapon drawn.
Releasing early or losing that target cancels it. Attacks against standing enemies remain immediate and cannot become an automatic finisher when the enemy falls.
`Finish=None` is now the default in both melee sections; existing files with `Finish=LT+RT` keep that optional extra shortcut until changed to `None`.

## Swimming

Touch and gamepad use the same camera-directed swimming behavior; keyboard swimming is unchanged.
The movement stick selects travel direction relative to the camera. Underwater, forward follows camera pitch, backward reverses it, and sideways movement stays level.
Right stick or the touch camera area steers the view. Camera assistance does not recenter it while swimming.

- At the surface, hold Jump to dive and swim downward.
- Release Jump, then hold it again underwater to swim upward, even with movement centered.
- Release Jump to return to camera-directed swimming; center movement to stop.
- Reaching the surface while holding Jump does not start another dive. Release before diving again.

Jump is X by default on gamepad, or the invisible touch Jump zone. Gamepad swimming uses the gameplay bindings even if fists remain drawn.
Gothic's swim animations, collision, oxygen and automatic surfacing rules still apply.

## Menus, inventory, and equipment

Touch also supports hold-and-drag wheels: hold Draw for equipment or Menu/Back for Character Stats and Journal, select with the same finger, and release to apply.
See [touch controls](README.md#controls-and-current-limitations) for cancellation and touch-only page browsing; gamepad wheel bindings remain unchanged.

Menus and dialogue use A to confirm, B to go back/skip, and the D-pad or left stick to navigate. View and Menu also close ordinary menus. Save-name entry still opens the Android keyboard; use its Done button. Unhandled UI input is not forwarded to gameplay.
In the journal, A opens a category or quest and B returns one level at a time: description, quest list, categories, then gameplay.
Up/down selects quests or scrolls a description. Keyboard Enter/Esc and mouse clicks/wheel remain supported on PC.

In the save/load menu, select an occupied slot and press X to request deletion. The confirmation shows the save name: A permanently deletes that one save file; B cancels without leaving the menu. Empty slots do nothing. The slot name and preview clear after deletion. This does not delete game assets or other slots, but there is no undo, so back up important saves first. Keyboard users can use Delete, Enter to confirm, and Escape to cancel.

Touch users can request deletion with a three-finger tap outside the action buttons, then tap the bottom-right Accept/Use area to confirm or top-right Back to cancel.
Ordinary screen clicks outside the virtual Accept area still cannot confirm deletion. Four-finger quickload is disabled in menus.

Remap or disable this independently using `[UI] DeleteSave=X` or `DeleteSave=None` in `Gamepad.ini`. Existing INI files inherit X when the entry is absent. Inventory X retains its separate Drop action, and deletion is ignored outside save/load slots.

In inventory, A uses/equips or transfers one item. Y transfers the full selected stack in trade, chests, and looting. X drops one player-owned item. LB selects the left trade/chest panel and RB selects the right, preserving each panel's selection. These buttons do nothing in a single-panel inventory.

To assign a selected spell/rune in personal inventory:

| Chord | Spell slot |
| --- | --- |
| LT + D-pad Up / Right / Down / Left | 3 / 4 / 5 / 6 |
| RT + D-pad Up / Right / Down / Left | 7 / 8 / 9 / 10 |

Hold D-pad Up for 400 ms to open the equipment wheel. Choose with the right stick and release Up while pointing to equip/draw. Center the stick or press B to cancel. LB/RB change pages; recenter before selecting on a new page. Each page contains up to eight owned weapons or already-assigned spells that pass Gothic's existing attribute and magic-circle requirements. Requirements are checked again before equipping. Touch and gamepad share the centered circular layout, with evenly spaced choices and a title above the wheel. The wheel uses the original inventory item renderer and font with Gothic-colored borders; no original Gothic assets are modified. The world continues running while the wheel is open, but movement/camera input is suspended.

Lockpicking, ladders, and other interactions use their own A/B and directional context. Release held controls and center both sticks after closing UI, returning from another app, or reconnecting the controller before continuing gameplay.

## Remapping

The first launch creates `/sdcard/Android/data/org.opengothic.app/files/Gamepad.ini`. Updates preserve it. Missing entries use defaults; restart after editing. It is separate from `Gothic.ini` and PC key mappings.

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
$padWork = Join-Path $env:TEMP ('OpenGothic-pad-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $padWork | Out-Null
& $adb shell am force-stop org.opengothic.app
& $adb pull '/sdcard/Android/data/org.opengothic.app/files/Gamepad.ini' "$padWork\Gamepad.ini"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read Gamepad.ini; launch this APK once first.' }
Copy-Item -LiteralPath "$padWork\Gamepad.ini" -Destination "$padWork\Gamepad.ini.backup"
notepad "$padWork\Gamepad.ini"
```

Save and close the editor, then upload and restart:

```powershell
& $adb push "$padWork\Gamepad.ini" '/sdcard/Android/data/org.opengothic.app/files/Gamepad.ini'
if ($LASTEXITCODE -ne 0) { throw 'Controller configuration upload failed.' }
& $adb shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
```

The complete defaults and option names live in [gamepadbindings.cpp](../common/utils/gamepadbindings.cpp). For example, edit these entries to move the walk chord and disable automatic camera assistance:

```ini
[Gameplay]
Walk=RB+L3

[Controller]
CameraAssist=0
```

Binding syntax and precedence:

- Names are case-sensitive: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `L3`, `R3`, `Menu`, `View`, `DpadUp`, `DpadDown`, `DpadLeft`, `DpadRight`, and `LeftStickUp`/`RightStickUp` with their other directions. Start/Select alias Menu/View.
- `+` makes a chord. Hold the modifier before pressing the final button. Commas add alternatives; `None` removes that action from that section. `Hold:` delays activation; a paired tap waits until release.
- Melee/ranged sections overlay Gameplay bindings on the same physical input. Inventory overlays UI. Removing an overlay binding exposes any underlying binding; remove that underlying action too when intentionally leaving a button unused.
- UI, wheel, and interaction contexts never inherit gameplay shortcuts. Context changes cancel held actions and require fresh button presses.
- The most specific matching chord wins. Equally specific chords with different simultaneously-held modifiers are suppressed. Duplicate input assignments within a section invalidate that section and retain its previous valid defaults/settings. Errors are logged without rewriting the user's file.
- `[Controller] Enabled=0` disables physical-controller handling and restores the existing touch controls. The copied PC `enableJoystick=0` does not disable Android controllers.
- `[Controller] ExplorationModifier` remaps LT's exploration override. `Finish` remains available as an optional independently remappable shortcut; hold-to-finish follows `AttackForward`.
- `[Axes]` configures dead zone, analog walk threshold, trigger hysteresis, and movement/camera stick assignment. Wheel stick assignment is `[EquipmentWheel] SelectionStick`.
- `[TargetLock]` configures switch threshold/reset, cooldown, and camera smoothing, not Gothic's target eligibility rules.
- `[Combat]` configures unlocked melee facing assistance for Android touch and gamepad.

Only the first connected controller is active. Physical volume buttons remain Android media-volume controls. Touch retains its existing invisible prototype mapping; the new touch layout is a separate milestone.

## Verification

Run the binding regression tests without Android dependencies:

```powershell
cmake -S tests/controller -B build/controller-tests
cmake --build build/controller-tests --config Release
ctest --test-dir build/controller-tests -C Release --output-on-failure
```

Build and lint the ARM64 APK with `.\gradlew.bat --no-daemon assembleDebug lintDebug` from the `android` directory. See the [README](README.md) for SDK setup, installation, APK inspection, and logs.

The tests exercise routing, chords, menu isolation, remapping, malformed settings, reconnect suppression, and tap/hold behavior. They do not simulate Gothic combat or a physical Android controller. Device acceptance testing must still cover movement at several camera headings, both combat settings, LT+A, lock acquisition/switch/loss, trade panels, wheel pages, spell assignment, quicksave/load, and focus/disconnect recovery. Keep a normal save before testing combat or quickload.

Platform reference: Android's [controller input guide](https://developer.android.com/games/sdk/game-controller/controller-input) and [input-device listener API](https://developer.android.com/reference/android/hardware/input/InputManager.InputDeviceListener).
