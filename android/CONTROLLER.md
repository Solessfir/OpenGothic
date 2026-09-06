# Android controller controls

The native controller path uses semantic game actions, not simulated PC key bindings. Windows keyboard/mouse controls remain unchanged; this does not add a Windows controller backend. Xbox button names are used below. L3 and R3 mean clicking the left and right sticks.

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

Quicksave/load require `[GAME] useQuickSaveKeys=1` in `Gothic.ini`. Potion shortcuts require `usePotionKeys=1`. Disabled shortcuts remain consumed: LB+View never falls through to opening inventory. Potion selection and restrictions come from the installed Gothic scripts, exactly as with the keyboard hotkeys. No separate potion-selection policy is added. See [configuration](CONFIGURATION.md) for safely editing these flags.

## Combat

The original `useGothic1Controls` setting selects the melee context.

| Context | Controls |
| --- | --- |
| Classic melee, weapon drawn | Y: forward attack; X: left attack; B: right attack; hold A: block |
| Modern melee, weapon drawn | RT: attack; hold RB: block |
| Bow, crossbow, or spell drawn | RT: shoot/cast; hold for spells that invest mana |
| Melee finisher | LT + RT, only when Gothic permits finishing the current target |

Holding LT restores exploration face buttons during classic melee. LT+A requests sheathing first, then interacts once with the same still-valid target. With no target it only sheathes; it does not redraw automatically. Ordinary A cannot pick up items while the weapon is drawn. Fists and weapons still obey Gothic's animation, ammunition, skill, and combat restrictions. Moving with a drawn weapon does not implicitly hold the classic action modifier.

## Menus, inventory, and equipment

Menus and dialogue use A to confirm, B to go back/skip, and the D-pad or left stick to navigate. View and Menu also close ordinary menus. Save-name entry still opens the Android keyboard; use its Done button. Unhandled UI input is not forwarded to gameplay.

In the save/load menu, select an occupied slot and press X to request deletion. The confirmation shows the save name: A permanently deletes that one save file; B cancels without leaving the menu. Empty slots do nothing. The slot name and preview clear after deletion. This does not delete game assets or other slots, but there is no undo, so back up important saves first. Keyboard users can use Delete, Enter to confirm, and Escape to cancel. Screen taps cannot confirm deletion.

Remap or disable this independently using `[UI] DeleteSave=X` or `DeleteSave=None` in `Gamepad.ini`. Existing INI files inherit X when the entry is absent. Inventory X retains its separate Drop action, and deletion is ignored outside save/load slots.

In inventory, A uses/equips or transfers one item. Y transfers the full selected stack in trade, chests, and looting. X drops one player-owned item. LB selects the left trade/chest panel and RB selects the right, preserving each panel's selection. These buttons do nothing in a single-panel inventory.

To assign a selected spell/rune in personal inventory:

| Chord | Spell slot |
| --- | --- |
| LT + D-pad Up / Right / Down / Left | 3 / 4 / 5 / 6 |
| RT + D-pad Up / Right / Down / Left | 7 / 8 / 9 / 10 |

Hold D-pad Up for 400 ms to open the equipment wheel. Choose with the right stick and release Up while pointing to equip/draw. Center the stick or press B to cancel. LB/RB change pages; recenter before selecting on a new page. Each page contains up to eight owned weapons or already-assigned spells that pass Gothic's existing attribute and magic-circle requirements. Requirements are checked again before equipping. The wheel uses the original inventory item renderer, slot artwork, and font; no original Gothic assets are modified. The world continues running while the wheel is open, but movement/camera input is suspended.

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
- `[Controller] ExplorationModifier` remaps LT's exploration override. Remap the separate `Finish` chord too if desired.
- `[Axes]` configures dead zone, analog walk threshold, trigger hysteresis, and movement/camera stick assignment. Wheel stick assignment is `[EquipmentWheel] SelectionStick`.
- `[TargetLock]` configures switch threshold/reset, cooldown, and camera smoothing, not Gothic's target eligibility rules.

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
