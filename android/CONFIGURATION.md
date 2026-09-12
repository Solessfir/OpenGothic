# Android settings

Most play settings are in the game's menu. For additional options, edit the INI files with the game closed, then restart.
Examples use NotR's app ID; substitute `org.opengothic.gothic1` for Gothic 1 or `org.opengothic.gothic2` for Classic.

## Files and precedence

`Gothic.ini` is read in this order, per setting:

1. `/sdcard/Android/data/org.opengothic.gothic2notr/files/Gothic.ini` - writable overrides.
2. `Gothic2/System/Gothic.ini` below that directory - copied game settings.
3. Built-in defaults.

Keep overrides small; do not replace them with the whole PC INI.
Android paths are case-sensitive: your installation may use `system` instead of `System`.
Controller bindings and movement options use a separate [Gamepad.ini](CONTROLLER.md#remapping).

## Graphics and FPS

Options > Video settings offers resolution in 5% steps, a 30 FPS / 60 FPS / Unlocked limit, half-resolution fog/SSAO, shadow resolution, brightness, contrast and gamma. Changes apply immediately. UI and text always use full resolution. Existing resolution and frame-rate choices are preserved when updating.

| File section / key | Android default | Options |
| --- | --- | --- |
| `[ENGINE] renderScale` | `100` | Scene resolution percentage, `50` to `100` |
| `[ENGINE] ssaoHalfResolution` | `1` | `0`: full-resolution ambient occlusion |
| `[ENGINE] fogHalfResolution` | `1` | `0`: original fog-lighting quality |
| `[ENGINE] shadowMapResolution` | `1024` | `512` reduces cost; `1536` or `2048` increases shadow detail and cost |
| `[ENGINE] zMaxFPS` | `60` | `30` reduces power/heat; `0` uncaps gameplay |
| `[ENGINE] frameRateLimit` | `-1` | Menu selection: `30`, `60` or `0` (Unlocked); `-1` uses the legacy INI limits |
| `[VIDEO] displayMode` | `auto` | Lowercase `sdr` forces SDR; `hdr` requests HDR with SDR fallback |
| `[GAME] showFps` | `0` | `1` shows the text-only FPS counter; also toggle it by holding Back/menu and selecting FPS |

Lower render quality can improve performance, but a cap cannot guarantee sustained FPS.
A menu selection overrides both `zMaxFPS` and SystemPack's limit. Until you select one, a positive `[PARAMETERS] FPS_Limit` in `Gothic2/System/SystemPack.ini` overrides `zMaxFPS`; otherwise the default is 60 FPS. Unlocked removes the game's cap, but presentation may still be limited by the display.
Menus are capped at at most 60 FPS.

Automatic HDR requires a compatible HDR display and Vulkan surface; unsupported devices fall back to SDR.
HDR expands highlight range without brightening dark shadows. Screenshots may not match the display's HDR appearance.

## Quicksave history

Options > Game settings > Quicksave slots controls `[GAME] quickSaveSlots`: Default (`0` in the INI) keeps the original single quicksave; `1` to `20` rotates through separate slots without replacing manual saves. Quickload uses the newest quicksave. In Load Game, select the quicksave row and use left/right to browse retained saves. Reducing the count keeps older files until you delete them.

## Dialogue volume

Options > Audio > Dialogue volume adjusts speech separately from effects and music. `[SOUND] voiceVolume` accepts `0.0` to `1.0`; without an override it follows the existing sound volume. Disabling sound still mutes both voices and effects.

## Vibration

`[GAME] vibration=1` enables feedback for menu, inventory and dialogue navigation, confirmation, lockpicking mistakes, bow shots, spell casts, landed hits and damage taken. Teleport charging builds from light pulses to stronger feedback when you teleport. Set it to `0`, or toggle Vibration in the System wheel, to disable both touch and controller feedback. Phone feedback respects Android's touch-vibration setting. Controller rumble requires support from the controller and its Android driver; unsupported controllers stay silent rather than vibrating the phone.

## Camera and interface

| Gothic.ini `[GAME]` key | Default | Meaning |
| --- | --- | --- |
| `cameraElevationOffset` | `10` | Exploration camera elevation, 0–30 degrees |
| `cameraCombatElevationOffset` | `20` | Melee/ranged/magic camera elevation, 0–30 degrees |
| `cameraFollowSpeed` | `2` | Follow response, 0.25–4; `1` restores slower following |
| `centerPlayerBars` | `0` | Classic corner health/mana bars; `1` stacks bars at bottom-center. Also selectable in the System hold wheel |
| `mouseSensitivity` | `0.53` | Also controls touch/gamepad camera sensitivity |
| `camLookaroundInverse` | Game preference | Vertical camera inversion |

Inventory keeps its own framing; radial wheels leave the gameplay camera active.
Manual pitch is preserved across exploration/combat, and elevation/zoom changes blend smoothly.
Movement sensitivity is separate from camera sensitivity; see [controller tuning](CONTROLLER.md#movement-and-targeting-settings).

UI scales automatically. In `Gothic2/System/SystemPack.ini`, set `[INTERFACE] Scale` to multiply that size:
`1` is automatic, `0.85` smaller, `1.1` larger. Excessive values can clip menus.
`Gothic.ini` `[GAME] invMaxColumns` adjusts inventory columns; `0` falls back to five.

## Camera obstruction fading

Foliage, hanging cave roots and spiderwebs fade around the camera and along the camera-to-player path in G1 and G2. The corridor keeps about 20% visibility with soft edges and tapers off below the player's hips to preserve low plants. Only foliage immediately around the camera can disappear completely. Enabled by default on Android; optional on desktop. Walls, terrain, characters, equipment and shadows stay unchanged.

```ini
[ENGINE]
cameraObstructionFade=1
cameraObstructionFadeDistance=180
```

Set `cameraObstructionFade=0` to disable it. Distance controls the nearby-camera fade in Gothic units (100 = one meter), clamped to 50-500. The nearest quarter of that distance is fully transparent, then visibility increases smoothly. The player corridor follows the camera distance independently, with a 90 cm faded radius and a soft edge out to 180 cm. It fades back to normal over the last 60 cm before the player. First-person, free and cutscene cameras use only the nearby fade. Alpha-tested leaves use fixed dithering; transparent webs retain their normal blending. Only recognized foliage/web texture families are eligible, so renamed mod textures may not fade. Path tracing does not use this effect.

## Shortcuts, audio and other preferences

```ini
[GAME]
useQuickSaveKeys=1
usePotionKeys=1
```

Both default to `1` on Android, enabling quicksave/load and potion shortcuts for touch and gamepad.
Explicit INI preferences, including copied values of `0`, are respected. Potion shortcuts use the game's script handlers when available.
Gothic 1 has no standard handlers, so Android uses the smallest owned standard health/mana potion through its normal drinking action; sheathe first.
Permanent-stat potions are never selected by this fallback, and no potion is used when the corresponding stat is full.

Other supported preferences include:

- `[GAME] useGothic1Controls`: `0` modern combat (Android default), `1` classic combat. Both settings work in Gothic 1 and Gothic II on Android.
- `[GAME] skipEmptyLoot`: `1` skips empty bodies; `0` restores interaction with them. Empty chests remain accessible (all platforms).
- `[GAME] subTitles`, `subTitlesPlayer`: dialogue subtitles.
- `[SOUND] soundEnabled`, `musicEnabled`, `soundVolume`, `musicVolume`: enable flags and volumes (0–1).
- `[VIDEO] zVidBrightness`, `zVidContrast`, `zVidGamma`: image adjustments.
- Extended graphics menu: Wind controls object swaying; Cloud Shadows controls SSAO; Radial Fog controls fog quality and sunshafts; Reflections controls screen-space reflections. Inactive water waves, water fading and Ambient FX options are hidden.

Android hides the inactive Visual Settings category, Performance/Quality presets and unused reverb and sample-rate controls. Use Video Settings for supported graphics options. Sound provider selects the OpenGothic or GothicKit music backend, not an Android audio driver.

## Optional diagnostics

All are off by default in `[DEBUG]`: `touchControls=1` shows the touch layout, `gamepadControls=1` shows the current gamepad bindings, `gpuProfile=1` records GPU timings, and `cpuProfile=1` enables trace markers.
The Back/menu hold wheel toggles the controls overlay for the input method used to open it. Touch and gamepad overlay preferences are saved separately.
Use `0` to disable them. Profiling adds overhead; see [contributor diagnostics](tools/README.md).

## Editing settings

Using the ADB connection from the [installation guide](README.md#connect-and-install-with-adb), save and exit first.
For `Gamepad.ini`, change `$file` below; for UI scale, use `Gothic2/System/SystemPack.ini`.

```powershell
$file = 'Gothic.ini'
$remote = "/sdcard/Android/data/org.opengothic.gothic2notr/files/$file"
$configDir = Join-Path $env:TEMP ('OpenGothic-config-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $configDir | Out-Null
& $adb -s $device shell am force-stop org.opengothic.gothic2notr
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the game' }
& $adb -s $device pull $remote "$configDir/settings.ini"
if ($LASTEXITCODE -ne 0) { throw 'Could not read settings; check path and launch the app once first' }
Copy-Item -LiteralPath "$configDir/settings.ini" -Destination "$configDir/settings.ini.backup"
notepad "$configDir/settings.ini"
```

Edit existing keys rather than adding duplicate sections/keys. Save and close the editor, then:

```powershell
& $adb -s $device push "$configDir/settings.ini" $remote
if ($LASTEXITCODE -ne 0) { throw 'Settings upload failed' }
& $adb -s $device shell am start -W -n org.opengothic.gothic2notr/org.tempest.TempestNativeActivity
```

Keep the backup for recovery. A running game may overwrite external edits on exit.
