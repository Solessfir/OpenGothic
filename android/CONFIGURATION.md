# Android configuration

OpenGothic reads individual `Gothic.ini` settings in this order:

1. `/sdcard/Android/data/org.opengothic.app/files/Gothic.ini`: writable overrides, beside saves and `log.txt`.
2. `Gothic2/System/Gothic.ini` below the same directory: the copied game's settings.
3. OpenGothic's built-in defaults.

Keep the writable file small. Do not replace it with the entire PC INI: some original settings have different meanings in OpenGothic. Preserve existing sections and keys, and edit an existing key instead of adding duplicates. Android paths are case-sensitive; use the actual `System` or `system` spelling in your copied installation.

## Enable shortcuts and the FPS counter

These are supported settings, not edits to Gothic's scripts or assets:

```ini
[GAME]
useQuickSaveKeys=1
usePotionKeys=1
showFps=1
```

- `useQuickSaveKeys`: enables the existing quicksave/quickload shortcut handlers (F5/F9 outside Marvin mode). Setting it to `0` disables those shortcuts, not ordinary menu saves/loads. Existing gameplay restrictions still apply.
- `usePotionKeys`: enables the existing health/mana potion hotkeys. These currently call the original game's script functions; availability and selection depend on the installed scripts. Setting it to `0` disables those hotkeys, not using an item from inventory.
- `showFps`: Android-only, `0` by default. Set `1` for padded Gothic text in the upper-left corner, or `0` to hide it. It uses the normal name-label font's original warm color, without a background rectangle or yellow tint. This does not change the FPS limit or desktop debug displays.

The copied Steam INI inspected during development disables both shortcut options. OpenGothic respects those values unless overridden. Controller chords follow the same gates: LB+Menu saves, LB+View loads, and D-pad Left/Right invoke the original potion scripts. See [controller controls](CONTROLLER.md) for the full layout and the separate remappable `Gamepad.ini`. No separate potion-selection policy is added.

### Edit safely from Windows

Save your game and exit before running this. The commands assume exactly one connected Android device; add `-s SERIAL` after `$adb` if needed.

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
$deviceIni = '/sdcard/Android/data/org.opengothic.app/files/Gothic.ini'
$configWork = Join-Path $env:TEMP ('OpenGothic-config-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $configWork | Out-Null
Write-Output $configWork
& $adb devices -l
& $adb shell am force-stop org.opengothic.app
& $adb pull $deviceIni "$configWork\Gothic.ini"
if ($LASTEXITCODE -ne 0) { throw 'Could not read the current configuration; do not overwrite it.' }
Copy-Item -LiteralPath "$configWork\Gothic.ini" -Destination "$configWork\Gothic.ini.backup"
notepad "$configWork\Gothic.ini"
```

In Notepad, add or edit the three keys in `[GAME]`, save, and close. Preserve other settings. Then:

```powershell
& $adb push "$configWork\Gothic.ini" $deviceIni
if ($LASTEXITCODE -ne 0) { throw 'Configuration upload failed.' }
& $adb shell cat $deviceIni
& $adb shell am start -W -n 'org.opengothic.app/org.tempest.TempestNativeActivity'
```

Restart after external edits; the running game does not watch INI files and may overwrite them on exit. Keep the printed `$configWork` location for recovery; push its `Gothic.ini.backup` to `$deviceIni` with the app stopped to restore it. If the device file is absent, first launch and exit OpenGothic once. These commands do not modify your PC installation. APK upgrades with `adb install -r` preserve app data; uninstalling removes it.

## Opt-in GPU profiling

For Vulkan diagnostics, add this to the writable `Gothic.ini` with the game stopped:

```ini
[DEBUG]
gpuProfile=1
```

Restart and load a game. OpenGothic records up to 600 completed frames to `gpu-profile.csv` beside the writable INI and saves. Capture begins after loading, includes named renderer markers, and automatically stops; unavailable results stop capture after 1,200 attempted frames. The file is flushed every 120 recorded frames and when capture ends. A restart with profiling enabled replaces the previous CSV, so copy it first. Set `gpuProfile=0` and restart to disable profiling. It is off by default, adds measurement overhead when enabled, and does not change graphics quality or grant a higher frame rate. Unsupported graphics backends return no timings.

Results are GPU elapsed intervals between markers, including pipeline stalls and overlap, not CPU recording time or isolated shader execution cost. Readback uses each frame slot's completed submission fence and never requests an additional GPU wait. The existing frame/command-buffer lifecycle still applies. More than 254 markers merges the remainder into `[marker limit]`.

Pull and summarize a completed capture (replace the serial for another phone):

```powershell
New-Item -ItemType Directory -Force build/performance | Out-Null
& C:\Android\Sdk\platform-tools\adb.exe -s RFCX10M60QT pull /sdcard/Android/data/org.opengothic.app/files/gpu-profile.csv build/performance/gpu-profile.csv
if ($LASTEXITCODE -ne 0) { throw 'Could not retrieve GPU timings.' }
./android/tools/Summarize-GpuProfile.ps1 -Path build/performance/gpu-profile.csv
```

The summary combines repeated marker names within each frame and includes zero contribution from frames where a pass was absent. Compare the same save/camera with profiling both off and on. See [PERFORMANCE.md](PERFORMANCE.md) for the baseline and measurement limitations.

## Original settings worth knowing about

This is an audit of native source readers, not a promise of complete original-engine compatibility. Game/menu scripts and mods can also read settings dynamically.

| Setting | Current Android behavior |
| --- | --- |
| `[GAME] useGothic1Controls` | Selects classic (`1`) versus Gothic II (`0`) combat handling, including the controller's melee context. |
| `[GAME] mouseSensitivity`, `camLookaroundInverse` | Shared by mouse, controller camera, and the existing Android touch-camera path. |
| `[GAME] enableMouse`, `enableJoystick` | `enableMouse` gates mouse input. No native reader for `enableJoystick`; the Android gamepad works independently of the copied PC value, which is often `0`. |
| `[GAME] subTitles`, `subTitlesPlayer` | Control dialogue subtitles and the player's subtitles. No separate native readers were found for `subTitlesAmbient` or `subTitlesNoise`. |
| `[GAME] animatedWindows` | Used for dialogue-window animation. Does not imply every original window-animation setting is implemented. |
| `[GAME] invMaxColumns`, `invCatOrder` | Control inventory columns and category order. Positive column counts are supported; `0` falls back to five columns, not the original INI's advertised automatic width. Restart after changing columns. |
| `[GAME] highlightMeleeFocus` | Read by the combat focus display. `highlightInteractFocus` has no native reader yet. |
| `[GAME] scaleVideos` | Used by video rendering. Original extended video-key and video-input-disabling options have no native readers. |
| `[SOUND] soundEnabled`, `soundVolume`, `musicEnabled`, `musicVolume` | Supported mute/volume controls; volume range is `0` to `1`. Android media volume remains a separate system control. |
| `[VIDEO] zVidBrightness`, `zVidContrast`, `zVidGamma` | Used by OpenGothic's renderer. The copied INI uses `0.5` for each. |
| `[ENGINE] zMaxFPS` | Frame-rate cap, unless a positive `SystemPack.ini` `[PARAMETERS] FPS_Limit` overrides it. `0` means uncapped. A cap of `30` is an optional mobile power/heat tradeoff, not a way to raise low FPS. |
| `[INTERNAL] vidResIndex` | OpenGothic render scale: `0` = native, `1` = 75% width/height, `2` = 50%. This is not the original game's display-mode index. The inspected PC value `13` should not be copied into the writable override. |
| `[ENGINE] zEnvMappingEnabled` | Reflection toggle. |
| `[ENGINE] zCloudShadowScale` | Reused as an ambient-occlusion/indirect-lighting toggle, not the original cloud-shadow intensity. |
| `[RENDERER_D3D] zFogRadial` | Reused as the sunshafts toggle, not a Vulkan radial-fog compatibility setting. |
| `[ENGINE] zWindEnabled`, `zWindCycleTime`, `zWindCycleTimeVar` | Read for animated vegetation. Other original wind parameters should not be assumed supported. |

For UI size, use `Gothic2/System/SystemPack.ini`, `[INTERFACE] Scale`, as described in the README. It multiplies Android's automatic UI scale; it is not a `Gothic.ini` setting. Lowering the 3D render scale is separate from making the interface larger.

Do not tune Android using original Windows display modes, refresh-rate overrides, old Direct3D driver workarounds, sound-provider/reverb switches, Windows `keyboardLayout`, or original texture/sound cache sizes. They do not configure equivalent Android backends. The original `sightValue`, `modelDetail`, and `zVobFarClipZScale` also have no native setting readers here; their original comments are not reliable mobile-performance guidance.

### Useful follow-up support, not included yet

- Extend the new controller actions to the next touch layout, including wheel and shortcut gestures.
- Add interaction-focus feedback beyond the existing name label and lock suffix, and audit separate ambient/player/NPC subtitle preferences for mobile readability.
- Fit inventory columns/rows to available UI space, preserving category selection when changing trade panels. Audit `invMaxRows`, `invShowArrows`, `invSplitScreen`, and `invSwitchToFirstCategory` rather than claiming their original semantics already work.
- Audit video skipping and `disallowVideoInput` before adding new controller/touch skip actions. Keep Android system navigation and volume buttons available.

Source entry points: [settings precedence and defaults](../common/gothic.cpp), [shortcut, camera, FPS and focus handling](../common/mainwindow.cpp), [potion script calls](../common/game/gamescript.cpp), [combat mode](../common/game/playercontrol.cpp), [inventory](../common/ui/inventorymenu.cpp), [dialogue](../common/ui/dialogmenu.cpp), and [render settings](../common/graphics/renderer.cpp).
