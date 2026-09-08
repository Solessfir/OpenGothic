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

| File section / key | Android default | Options |
| --- | --- | --- |
| `[INTERNAL] vidResIndex` | `1` | `0`: native, `1`: 75%, `2`: 50% scene resolution; UI stays full-resolution |
| `[ENGINE] ssaoHalfResolution` | `1` | `0`: full-resolution ambient occlusion |
| `[ENGINE] fogHalfResolution` | `1` | `0`: original fog-lighting quality |
| `[ENGINE] shadowMapResolution` | `1024` | `1536` or `2048` increases shadow detail and cost |
| `[ENGINE] zMaxFPS` | `60` | `30` reduces power/heat; `0` uncaps gameplay |
| `[VIDEO] displayMode` | `auto` | Lowercase `sdr` forces SDR; `hdr` requests HDR with SDR fallback |
| `[GAME] showFps` | `0` | `1` shows the text-only FPS counter; also toggle it by holding Back/menu and selecting FPS |

Lower render quality can improve performance, but a cap cannot guarantee sustained FPS.
A positive `[PARAMETERS] FPS_Limit` in `Gothic2/System/SystemPack.ini` overrides `zMaxFPS`.
Menus are capped at at most 60 FPS.

Automatic HDR requires a compatible HDR display and Vulkan surface; unsupported devices fall back to SDR.
HDR expands highlight range without brightening dark shadows. Screenshots may not match the display's HDR appearance.

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
- Extended graphics menu: Cloud Shadows controls SSAO; Radial Fog controls sunshafts; Reflections controls screen-space reflections.

Original Windows driver/audio-provider options and cache-size tweaks do not configure Android equivalents.
Original `sightValue`, `modelDetail` and `zVobFarClipZScale` are not supported distance/detail controls here.

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
