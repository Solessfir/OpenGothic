# Gamepad controls

Gothic 1 and Gothic II use the same mappings, movement and radial menus on Android.
Storage examples use NotR's app ID; substitute your [edition's ID](SETUP.md#game-editions-and-storage) for Gothic 1 or Classic.

Xbox button names are used below: L3/R3 click the sticks; Menu/View are Start/Select.
Only the first connected controller is active. Disconnecting it pauses gameplay; reconnecting does not resume automatically.
Physical volume buttons remain Android media controls.

## Movement and shortcuts

| Control | Action |
| --- | --- |
| Left stick | Up moves forward, down steps backward; 8:30/3:30 turns in place, upper diagonals move forward while steering; light tilt walks, firm tilt runs |
| Right stick | Camera; while locked, vertical look and horizontal target switching |
| R3 | Toggle target lock with a weapon drawn |
| L3 | Sneak |
| A / B / X / Y | Interact/accept / back/skip dialogue / jump / journal |
| Menu: tap / hold | Pause / System wheel |
| View | Inventory |
| RB | Draw/sheathe the current weapon |
| D-pad direction: tap / hold | Use the assigned quick slot / open its category wheel |
| LB+D-pad Up | First person |
| LB+D-pad Down, hold | Look behind |
| LB+D-pad Left / Right | Character stats / journal |
| LB+Menu / LB+View | Quicksave / quickload without confirmation |

Quicksave/load and potions require the corresponding [Gothic.ini shortcuts](CONFIGURATION.md#shortcuts-audio-and-other-preferences).
Sneak requires the learned skill. While locked, sideways movement strafes; forward/back approaches/retreats.
The target name gains `(locked)`. Sheathing, a downed target or leaving focus range releases the lock.

## Combat

`Gothic.ini` `[GAME] useGothic1Controls` selects classic (`1`) or modern (`0`, default) combat in either game on Android.

| Context | Controls |
| --- | --- |
| Classic melee | Y forward attack; X left; B right; A or LT timed parry |
| Gothic II melee | RT attack; LT timed parry |
| Bow/crossbow | Tap RT for one shot after aiming; hold for repeat fire |
| Spell | RT cast; hold for spells that invest mana |
| Finisher | Hold Y (classic) or RT (Gothic II) over an unconscious NPC in finishing range |

A finisher requires a drawn one- or two-handed melee weapon. Release early to cancel.
Against a standing target, attacks remain immediate. With no melee target, tap to swing on release; a long hold does nothing.
Parry buttons request one attempt per press, not continuous protection.

Hold LB to restore exploration face buttons during classic combat.
LB+A sheathes, then interacts with the same eligible target; without a target it only sheathes.
Ordinary A cannot pick up items with a weapon drawn.

## Menus, inventory and wheels

- A accepts; B backs out one level or skips a dialogue line.
- Left stick/D-pad navigates items; left/right adjusts the selected slider or setting. Right stick left/right also adjusts values, but does nothing on other menu entries. Hold a direction to repeat.
- Inventory: tap A to use/equip/transfer one item on release; hold A and press a D-pad direction to assign the highlighted owned item without using it. X drops; Y transfers a stack; LB/RB selects trade or chest panels.
- Save/load menu: X requests deletion, A confirms, B cancels. Deletion has no undo.
- Save-name entry uses the Android keyboard; press **Done**.
- System wheel: hold Menu, select with the right stick, then release Menu to toggle FPS, the touch debug overlay or the HP/mana layout. B cancels; releasing without a selection also cancels.
- Quick-slot wheel: hold an assigned D-pad direction, select with the right stick, then release the direction to replace that slot's item and use/draw it. Releasing the stick keeps your choice; B cancels. Opening and releasing without choosing also cancels. LB/RB changes pages.
- Up to eight items fit on one page. Larger collections use six items plus two page arrows, matching touch. Hold the stick over an arrow to turn a page; center it before selecting again.
- The wheel uses the slot's category: weapons, magic, potions, food, or maps/documents. Only owned items meeting their use/equip conditions appear. The world keeps running while it is open.
- Equipped items have a gold rim. Selecting a sheathed weapon draws it; selecting the weapon already in hand leaves it drawn.
- After closing menus or reconnecting, release buttons and center sticks before moving.

### Quick slots

All four directions start empty. In inventory, hold A first, then press the direction to assign; a brief direction label confirms it. D-pad alone still navigates and repeats when held.
Weapons draw on use. Magic is readied without casting; an unassigned spell takes a free numbered spell slot, or replaces slot 3 if all eight are occupied. Teleport and other charging spells still require holding Attack to cast.
Maps keep their individual identities, so different directions can open different owned maps.
When an assigned standard health/mana potion runs out, the slot switches to an available standard potion of the same type, starting with the smallest tier. Permanent-stat potions and unknown/modded potion families are never substituted automatically.
Food, potions and documents use their normal scripts; sheathe first. Quick potions respect `usePotionKeys`.
Assignments are stored by item script name in Gothic.ini's `[QuickSlots]` section, independently for each app and shared across that app's saves. Loading an older save does not restore old assignments or create missing items. Clear a direction's value to unassign it.
Touch remains separate: two-finger left/right swipes use health/mana potions, and holding Draw opens the equipment wheel.

Assign a selected spell/rune from personal inventory:

| Chord | Spell slots |
| --- | --- |
| LT+D-pad Up / Right / Down / Left | 3 / 4 / 5 / 6 |
| RT+D-pad Up / Right / Down / Left | 7 / 8 / 9 / 10 |

## Swimming

Movement follows the camera underwater. Hold X at the surface to dive.
Release and hold X again underwater to rise; release it to return to camera-directed swimming.
Center movement to stop. Reaching the surface while holding X does not start another dive.

## Remapping

Edit `/sdcard/Android/data/org.opengothic.gothic2notr/files/Gamepad.ini` with the game closed.
Use the [INI editing commands](CONFIGURATION.md#editing-settings), choosing `Gamepad.ini`.
Updates preserve the file; missing entries inherit defaults. Restart after editing.

```ini
[Gameplay]
DrawSheathe=RB
SystemWheel=Hold:Menu

[UI]
AdjustLeft=RightStickLeft
AdjustRight=RightStickRight

[Controller]
CameraAssist=0
```

- Button names are case-sensitive: A/B/X/Y, LB/RB/LT/RT, L3/R3, Menu/View, DpadUp/Down/Left/Right, and LeftStick/RightStick directional names such as `LeftStickUp`.
- `+` makes a chord; press modifiers first. Commas add alternatives, `Hold:` delays activation, and `None` removes a binding.
- Combat sections override Gameplay; Inventory overrides UI. Removing an override exposes its underlying binding.
- Menus/wheels do not inherit gameplay shortcuts. The most specific chord wins; conflicting assignments are reported in `log.txt`.
- `[Controller] Enabled=0` disables gamepad handling and restores touch controls.
- `[Controller] HoldMs=400` sets the default hold threshold. `ExplorationModifier=LB` selects the exploration modifier.
- `QuickUp/Down/Left/Right`, `WheelUp/Down/Left/Right`, and inventory `AssignUp/Down/Left/Right` can be rebound independently.

See [complete defaults and supported option names](../common/utils/gamepadbindings.cpp).
Desktop keyboard/mouse bindings are separate; this port does not add a Windows gamepad backend.

## Movement and targeting settings

These entries are in `Gamepad.ini`, not `Gothic.ini`:

| Section / key | Default | Purpose |
| --- | --- | --- |
| `[Axes] MovementDeadZone` | `0.28` | Gamepad movement dead zone |
| `[Axes] TouchMovementDeadZone` | `0.15` | Touch movement dead zone |
| `[Axes] MovementExponent` | `1.5` | Soften small movement/turning input |
| `[Axes] WalkThreshold` / `TouchWalkThreshold` | `0.60` / `0.35` | Walk/run transition |
| `[Axes] WalkHysteresis` | `0.04` | Margin preventing walk/run flicker |
| `[Axes] MovementTurnSpeed` / `TouchTurnSpeed` | `180` / `180` | Base turning speed in degrees/second |
| `[Controller] CameraAssist` | `1` | Gamepad movement recentering |
| `[TargetLock] CameraSmoothingSeconds` | `0.20` | Recenter smoothing, divided by Gothic.ini `cameraFollowSpeed` |
| `[Combat] MeleeAssist` | `1` | Face the focused NPC when beginning a melee attack |
| `[Combat] MeleeAssistMaxAngle` / `MeleeAssistMaxDistance` | `90` / `300` | Facing-assist limits: degrees / centimetres |
| `[Combat] MeleeFocusRangeScale` | `0` | Scripted monster warning range; `1`–`4` instead multiplies original focus distance |

Mouse speed controls the camera, not movement. Forward movement still uses Gothic's walk/run animations.
Left-stick turning scales with stick deflection, up to `MovementTurnSpeed` at full input.
Locked sidesteps avoid automatic low-stick walking. Walking uses stick pressure; there is no default walk-toggle shortcut.

Automatic focus range uses `PERC_DIST_MONSTER_ACTIVE_MAX` (15 metres in standard Gothic II), falling back to doubled focus range if absent/invalid.
It affects melee names/health and locking, not attack reach, item pickup or enemy AI.
It is not a safety guarantee: enemies may skip warnings or attack after a warning timer.
Existing explicit range overrides remain in effect; set `MeleeFocusRangeScale=0` for automatic range.
