# Android configuration

OpenGothic reads individual `Gothic.ini` settings in this order:

1. `/sdcard/Android/data/org.opengothic.app/files/Gothic.ini`: writable overrides, beside saves and `log.txt`.
2. `Gothic2/System/Gothic.ini` below the same directory: the copied game's settings.
3. OpenGothic's built-in defaults.

Keep the writable file small. Do not replace it with the entire PC INI: some original settings have different meanings in OpenGothic. Preserve existing sections and keys, and edit an existing key instead of adding duplicates. Android paths are case-sensitive; use the actual `System` or `system` spelling in your copied installation.

## Third-person camera framing

Android raises the ordinary third-person camera along its orbit by 10 degrees and looks down toward the character and ground ahead.
Combat uses 20 degrees of added elevation to improve nearby enemy visibility with its closer camera.
At a three-meter follow distance this adds roughly half a meter of height when starting from a level orbit.
This applies to exploration, combat and inventory, including existing saves; manual camera adjustment remains available.
The offset blends through the normal camera system and does not accumulate on save/load.
First-person, dialogue, swimming, diving, cutscenes and desktop defaults are unaffected.

```ini
[GAME]
cameraElevationOffset=10
cameraCombatElevationOffset=20
cameraFollowSpeed=2
```

Both elevation settings support `0` to `30` degrees, subject to the camera's existing pitch limits. Use `0` for the original framing.
`cameraCombatElevationOffset` applies to melee, ranged and magic combat; `cameraElevationOffset` applies to exploration and inventory.
Drawing or sheathing a weapon preserves your manual pitch adjustment while switching between these offsets.

`cameraFollowSpeed` scales ordinary third-person position/rotation following and touch/gamepad camera assistance.
The default `2` doubles the follow response, approximately halving smoothing lag; automatic recentering waits 400 ms instead of 800 ms after manual input.
Values from `0.25` to `4` are supported; `1` restores the previous response.
Manual camera sensitivity, character movement, animation speed and scripted cameras are unaffected.

## Optional render scale

Reduced-resolution rendering is configurable, not an Android requirement or a forced optimization.
The in-game Resolution choice offers `full`, `upscale(75%)`, and `upscale(half)`.
Fresh Android installations default to 75% scene resolution for a better mobile performance/quality balance.
Existing writable render-scale choices are preserved; desktop installations still default to full/native resolution.

The equivalent writable `Gothic.ini` setting is:

```ini
[INTERNAL]
vidResIndex=1
```

Use `0` for native resolution, `1` for 75% of the output width and height, or `2` for 50%.
On a 2340x1080 display these correspond to 2340x1080, 1755x810, and 1170x540 for the 3D scene.
Lower scales improve performance at the cost of image detail; the upscaler cannot recover all of the missing detail.
UI scaling is a separate setting.
Edit the INI with the game stopped; no APK rebuild is required.

The Lanczos optimization applies only when reduced-resolution rendering is selected.
The 50% measurements in the performance report are diagnostic results, not a recommended or packaged default.

## Automatic HDR display output

Android now defaults to automatic HDR detection:

```ini
[VIDEO]
displayMode=auto
```

Use lowercase `sdr` to force the original SDR output, or `hdr` to explicitly request HDR when supported.
Missing or unrecognized values behave as `auto`; `hdr` never forces an unsupported format.
Edit the writable INI with the game stopped and restart after changing modes.
Desktop output remains SDR; this first HDR output implementation is Android/Vulkan-specific.

Automatic mode requires both a display advertising HDR10/HDR10+ and a Vulkan surface exposing a supported 10-bit Rec.2020/ST 2084 (PQ) format.
It also requires the floating-point intermediate render target used by OpenGothic.
HLG-only and scRGB-only output paths are not implemented and fall back to SDR.
Tempest rechecks display support when the surface is recreated or the display's reported HDR capability changes.
If the HDR swapchain cannot be created, it attempts SDR instead; device-loss errors are not hidden as capability failures.
The engine requests static HDR metadata when the driver supports it; it does not generate HDR10+ dynamic metadata.

The display's reported peak luminance is used with a conservative 1000-nit upper limit; missing peak information uses 1000 nits.
The reference white for UI/video is 200 nits, reduced if the reported peak is lower.
These are rendering targets, not measured screen brightness or a guarantee of sustained panel luminance.
The HDR tone curve preserves the existing curve through unit scene intensity and smoothly extends brighter highlights toward the reference peak.
Fog density, ambient/shadow lighting and the user's brightness/contrast/gamma settings are not adjusted to lift shadows.
SDR composition remains the original path, and save thumbnails remain SDR.

HDR uses a full-resolution RGBA16F intermediate for composition, then converts to Rec.2020/PQ with final 10-bit dithering.
Menus, subtitles, inventory models and video are composed before that conversion rather than being drawn as SDR pixels directly into a PQ surface.
This adds memory and GPU work; use `sdr` if HDR's visual benefit is not worth its cost on your device.
`HDR output` measures the final conversion pass, not all additional bandwidth/compositing overhead.

Look for `Display output = HDR10 PQ` or `Display output = SDR` in logcat/native logs.
On the tested S24, Android's compositor confirms `RGBA_1010102` and `BT2020_PQ` for HDR, versus `RGBA_8888` and `V0_SRGB` for SDR.
Screenshots may be tone-mapped by Android and are not a calibrated representation of the physical HDR display.
External-display transitions, unsupported HDR devices and prolonged HDR power/thermal behavior still need hardware coverage.

## Optional half-resolution ambient occlusion

Android defaults to half-resolution SSAO, independently of scene resolution.
The equivalent writable `Gothic.ini` option is:

```ini
[ENGINE]
ssaoHalfResolution=1
```

Use `0` to restore the existing full-resolution SSAO calculation and blur; this remains the desktop default.
With `1`, AO is calculated at half the scene width and height, then blurred and resolved back to scene resolution using depth to avoid blending unrelated surfaces.
At 75% render scale on a 2340x1080 screen, the scene remains 1755x810 and AO calculation uses 878x405.
Textures, geometry, UI resolution and the AO sampling radius are unchanged.
Fine contact shadows can be softer or missing, especially around thin geometry; this is an optional quality/performance tradeoff, not an identical-image optimization.
The existing `zCloudShadowScale` gate still controls whether SSAO runs at all.
If the required RG32F storage format is unavailable, rendering falls back to full-resolution SSAO.

This renderer option also works on desktop; either setting can be overridden on Android.
Existing explicit AO settings are preserved, and missing settings inherit the platform default.
Use the safe INI-edit commands below with the game stopped, and restart after editing.
Set `ssaoHalfResolution=0` to restore full-resolution AO without changing the scene render scale or rebuilding the APK.
GPU profiling labels the new resolve as `SSAO upsample`; compare its combined cost with `SSAO` against the old `SSAO` plus `SSAO blur`.

## Optional lower-resolution fog lighting

Fog lighting can be calculated in a smaller volume, independently of the scene and SSAO resolutions:

```ini
[ENGINE]
fogHalfResolution=1
```

Android defaults to `1`; desktop defaults to `0`, preserving the original fog quality.
Existing explicit settings are preserved, and missing settings inherit the platform default.
Only the parsed integer `1` enables the smaller volume; other parsed values retain original quality.
This halves the lighting volume's width and height while preserving every depth step and the original scattering calculation.
With sunshafts enabled, both lighting volumes change from 128x64x32 to 64x32x32.
Without sunshafts, the single volume changes from 160x90x64 to 80x45x64.
The existing linear sampling reconstructs the lighting; sunshaft shadow sampling and its occlusion texture remain unchanged.
Fog density, visibility distance, geometry, textures, UI and scene resolution do not change.
Path-traced fog does not use this setting.

The smaller volumes contain 25% as many texels, but this does not imply a 75% reduction in total fog or frame time.
Fine angular lighting gradients may become softer, especially near the sun or horizon; this is an optional quality tradeoff, not an identical-image optimization.
Broader scene comparisons and prolonged thermal measurements remain useful for evaluating this mobile default.
Edit the writable INI with the game stopped, then restart; `log.txt` reports `Fog lighting volume = ...`.
Set `fogHalfResolution=0` to restore original quality without changing any other graphics options or rebuilding the APK.
The `Fog-LUTs` profiler marker includes the separate sunshaft occlusion work when enabled, so it does not measure only lighting-volume generation.

## Shadow-map resolution

The conventional sunlight shadow maps support three resolutions through writable `Gothic.ini`:

```ini
[ENGINE]
shadowMapResolution=1024
```

Use `2048` for the original quality, `1536` for an intermediate setting, or `1024` for the lowest-cost option.
The default remains `2048` on both Android and desktop; missing or unsupported parsed values fall back to it.
Values use the existing Gothic INI integer parser.
In particular, `0` does not disable shadows or allocate an empty texture.
The setting applies to both conventional sunlight shadow maps, including their fog consumers, not virtual shadow-map pages or ray-traced shadow quality.
Restart after editing the INI; `log.txt` reports the effective value as `Shadow map resolution = ...`.

Compared with 2048, 1536 stores 56.25% as many depth texels and 1024 stores 25%.
This does not imply an equivalent frame-time saving: geometry processing, draw submission and much of shadow sampling still remain.
Lower resolutions can make shadow edges softer or more visibly stepped, especially on vegetation and thin geometry.
Scene resolution, SSAO, shadow coverage and lighting direction remain unchanged; the existing shadow filter already uses the texture's actual dimensions.
Set `shadowMapResolution=2048` to restore the original quality without changing other graphics settings.

## Frame-rate limit and low-power waits

Android initializes a missing writable `[ENGINE] zMaxFPS` to `60`, including when copied PC settings request uncapped rendering.
An existing writable value is preserved; set `0` to disable the gameplay cap or another positive value to change it.
A positive `[PARAMETERS] FPS_Limit` in `Gothic2/System/SystemPack.ini` still takes precedence.
To use the writable Gothic limit, leave that SystemPack value at `0`.

```ini
[ENGINE]
zMaxFPS=60
```

Android uses monotonic fractional-frame deadlines: 60 FPS is approximately 16.667 ms, not the old integer 16 ms interval.
The render thread sleeps until the next deadline without busy-spinning, and missed deadlines restart the schedule without catch-up bursts.
Focus, resize and world-loading transitions reset the schedule.
The main menu is limited to at most 60 FPS, including when gameplay is uncapped; a lower explicit limit also applies to menus.
CI timedemos bypass the Android cap.
The FPS counter uses elapsed time after actual rendering, not a requested sleep duration added to the clock.
Desktop timing behavior is unchanged.

This is application-side rate limiting, not display-synchronized presentation or a guarantee that every frame reaches the screen within 16.667 ms.
Android Frame Pacing (Swappy) integration remains a separate backend improvement for display synchronization and queue management.
See the [Android frame-pacing overview](https://developer.android.com/games/sdk/frame-pacing) for that distinction.
A cap avoids excess work when there is spare performance; it cannot make an over-budget CPU/GPU frame complete faster.
Measure long, warmed-up gameplay before claiming sustained 60 FPS.

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

## Opt-in CPU profiling

Add `[DEBUG] cpuProfile=1` to the writable `Gothic.ini` with the app stopped, then restart.
This enables scoped Android trace markers for simulation, animation, camera, UI, command recording, presentation, frame fences, worker tasks and completion waits.
Markers are emitted only while a platform trace is recording the app; there is no CPU CSV or continuous log stream.
The default is off, and the Tempest implementation is a no-op on other platforms.
Set `cpuProfile=0` and restart to disable it.

Use `android/tools/performance.pbtxt` as described in [PERFORMANCE.md](PERFORMANCE.md) to capture the app with Perfetto.
Then run both analyses from the repository root:

```powershell
python build/performance/trace_processor query -f android/tools/performance.sql '<capture.perfetto-trace>'
python build/performance/trace_processor query -f android/tools/cpu-performance.sql '<capture.perfetto-trace>'
```

The CPU report separates inclusive wall time from scheduled CPU time within each marker.
Nested regions overlap, so do not add their durations together.
An empty report means no matching markers were captured, not that those operations took zero time.
Tracing adds overhead; compare identical scenes and trace settings when testing a scheduling change.
The backend uses the [Android NDK tracing API](https://developer.android.com/ndk/reference/group/tracing), with paired scopes on the same thread.

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
| `[ENGINE] zMaxFPS` | Android initializes a missing writable value to `60`; explicit values are preserved. A positive `SystemPack.ini` `[PARAMETERS] FPS_Limit` overrides it. `0` means uncapped gameplay; `30` is an optional power/heat tradeoff. |
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
