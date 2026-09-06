# Android performance measurements

## Current test preference

The user prefers 75% render scale for acceptable image quality.
The S24's writable `Gothic.ini` was changed to `[INTERNAL] vidResIndex=1` after the cooled worker-wait tests.
Future default comparison runs should use 75%; any explicit native/50% diagnostic runs must remain clearly labeled.
Earlier 50% FPS results below do not describe performance at 75%.
Render scale remains configurable through the existing Resolution choice or INI.
After the half-resolution SSAO test, the user requested 75% scene scale and half-resolution SSAO as Android defaults.
New Android installations now use those defaults; existing explicit choices and desktop defaults are preserved.
The previous local configuration is backed up at `build/performance/s24-preferred75-20260906-232501/Gothic.ini.backup`.

## S24 baseline, 2026-09-06

Measured the installed OpenGothic `38fd92e6` / Tempest `b8c952ef` build on an SM-S921B (Xclipse 940), Android 16. The debug APK uses optimized native code (`RelWithDebInfo`, `-O2 -g -DNDEBUG`), not an unoptimized C++ Debug build.

APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `F01D1F1523303AE1610E5FC9893209617EC9C547F7B096ED025791B39198E67D`.
This APK was installed successfully and launched before this measurement.

The character was outside beside the waterfall/path, at native 2340x1080 (`[INTERNAL] vidResIndex=0`). The phone was USB-connected and already warm. No settings, saves, camera inputs, clocks or thermal controls were changed. ADB stayed connected during collection.

| Observation | Result |
| --- | --- |
| Trace duration | 29.986 seconds |
| Vulkan presentation cadence | 28.36 FPS over 847 intervals |
| Mean / median interval | 35.26 / 35.61 ms |
| 95th / 99th percentile interval | 40.10 / 42.07 ms |
| Worst interval | 45.15 ms |
| GPU utilization | 100 in all 15 vendor utilization samples, two seconds apart |
| GPU current clock and allowed maximum | Both varied together between 600000 and 700000 in vendor sysfs units (600-700 MHz); highest advertised frequency is 1095000 |
| Live thermal sensors after capture | Skin 44.4 C, battery 43.9 C, AP 54.9 C |
| Android thermal status | 2 (moderate throttling) |
| Game/render thread CPU time | 16.482 CPU seconds over the trace, about 55% of one core |
| Game/render thread presentation-call duration | 15.115 ms average, including waits |
| Audio mixer CPU time | 4.302 CPU seconds, about 14% of one core |
| Trace error/loss counters | No nonzero warning/error statistics reported by Trace Processor |

The displayed FPS was 35 in an earlier screenshot and 27 after this capture. These are separate snapshots, not a measured cold-to-hot curve. Android defines thermal status 2 as [moderate throttling](https://developer.android.com/reference/android/os/PowerManager#THERMAL_STATUS_MODERATE).

The evidence strongly points to GPU saturation with thermal/power limits contributing. It does not isolate the most expensive render pass or prove that heat explains every reported 6-18 FPS episode. The game thread still consumes about 19.5 ms of CPU time per presented frame on average; CPU work also needs attention for a 16.67 ms / 60 FPS budget. Do not sum CPU and GPU times: their work overlaps.

`dumpsys SurfaceFlinger --latency` returned only the refresh period, and FrameTimeline contained no app-layer frames on this device. The FPS above therefore uses the driver's `QueuePresentKHR` markers, not verified display scanout. GPU-completion fence waits averaged 35.091 ms on a separate tracing thread; that includes queued work and is not a per-pass GPU timer. Raw traces remain local under ignored `build/performance/` and should not be published without inspection because system traces contain other process names and activity.

## Fresh-process reload follow-up

After the user freshly reloaded, ADB confirmed a new app process. A second 29.960-second trace covered the same waterfall/path viewpoint at unchanged native resolution. This was a fresh process, not a thermally cold start: live skin temperature was already 44.0 C before recording and Android thermal status remained 2.

| Observation | After reload |
| --- | --- |
| Vulkan presentation cadence | 24.76 FPS over 739 intervals |
| Mean / median interval | 40.39 / 40.48 ms |
| 95th / 99th percentile interval | 44.67 / 46.97 ms |
| Worst interval | 50.29 ms |
| GPU utilization | 100 in all 16 samples |
| GPU current clock and allowed maximum | 600 MHz for the first two samples, then 545 MHz for the remaining 14 |
| Live temperatures after recording | Skin 44.3 C, battery 44.6 C, AP 53.3 C |
| Game/render thread CPU time | 15.521 CPU seconds |
| Presentation-call duration | 18.808 ms average, including waits |
| Trace error/loss counters | No nonzero warning/error statistics |

A screenshot after recording showed 24 FPS and a comparable camera position. Reloading did not restore performance during this sample; the lower observed GPU ceiling is consistent with the slower cadence. This does not rule out other time-dependent engine problems or replace a cooled-device comparison. No settings or gameplay inputs were changed, and ADB remained online. Raw data is local in `build/performance/reloaded-20260906/`.

## S23 Ultra installation, 2026-09-06

Installed the same baseline APK on an SM-S918B, Android 16, with `adb -s R5CX520FJDJ install -r`. Package inspection confirmed `arm64-v8a`. The S24 was disconnected and was not modified.

Copied the user's legally owned Steam installation directly to the new phone's app-specific `Gothic2` directory: 518 files, 3,347,791,152 bytes. Device file count matched; SHA-256 checks of `Data/Worlds.vdf` and `_work/Data/Scripts/_compiled/GOTHIC.DAT` matched the PC source. No game assets were added to Git or the APK, and no S24 saves were transferred.

The new writable `Gothic.ini` matches the last measured S24 overrides:

```ini
[INTERNAL]
vidResIndex=0

[GAME]
useQuickSaveKeys=1
usePotionKeys=1
showFps=1
mouseSensitivity=0.500000
```

The copied SystemPack settings retain `Scale=1` and `FPS_Limit=0`. The app generated the current default `Gamepad.ini`, including health/mana on D-pad Left/Right, quicksave/load chords, and the revised movement settings. Original potion-script restrictions still apply; enabling shortcuts does not grant potions or mana.

Cold activity launch returned `Status: ok`; the main menu rendered in landscape at 2316x1080 with the padded FPS counter visible. A menu screenshot showed 62 FPS, which is not an in-game or sustained-performance result. No gameplay inputs were injected; gameplay and audible audio testing remain with the user. Startup logs contained no fatal native or AndroidRuntime error in the inspected process output, but `log.txt` reported `Failed to created DmLoader object. Out of memory?`. The message is not proof of actual memory exhaustion: the loader can also fail during mutex initialization. This warning remains to be diagnosed; reaching the menu does not verify music playback.

Local setup files and the verification screenshot are under ignored `build/device-setup/s23-ultra/`. Launch and inspect this phone explicitly when multiple devices are connected:

```powershell
& C:\Android\Sdk\platform-tools\adb.exe -s R5CX520FJDJ shell am start -W -n org.opengothic.app/org.tempest.TempestNativeActivity
& C:\Android\Sdk\platform-tools\adb.exe -s R5CX520FJDJ shell cat /sdcard/Android/data/org.opengothic.app/files/Gothic.ini
```

## S24 75% scale experiment

On reconnecting the S24 after a cooling break, OpenGothic was closed. Live skin temperature was 42.4 C, battery 39.8 C, and thermal status was still 2. This is not an unthrottled cold baseline.

Backed up the complete writable INI to local `build/performance/s24-scale75-20260906-212213/Gothic.ini.backup`, then changed only `[INTERNAL] vidResIndex` from `0` to `1` and verified the uploaded file. This requests 1755x810 3D rendering instead of 2340x1080, while leaving UI scale, shortcuts, FPS display and sensitivity unchanged. Launched successfully to the main menu. No engine code or APK was changed.

After the user loaded the comparison save, USB disconnected before capture could start. Restarting ADB did not recover it; reconnecting the cable restored the device. The subsequent trace completed successfully with `vidResIndex=1` confirmed. A screenshot after recording showed the same waterfall/path viewpoint and 44 FPS.

| Observation | 75% width/height |
| --- | --- |
| Trace duration | 29.982 seconds |
| Vulkan presentation cadence | 46.51 FPS over 1,388 intervals |
| Mean / median interval | 21.50 / 21.64 ms |
| 95th / 99th percentile interval | 24.14 / 25.26 ms |
| Worst interval | 26.24 ms |
| GPU utilization | 100 in all 16 samples |
| GPU current clock and allowed maximum | Both ranged from 600 to 700 MHz |
| Live skin temperature before / after | 44.1 / 44.4 C |
| Live battery temperature before / after | 42.3 / 43.0 C |
| Thermal status before / after | 2 / 2 |
| Game/render thread CPU time | 18.710 CPU seconds, about 13.5 ms per presented frame |
| Presentation-call duration | 7.861 ms average, including waits |
| Trace error/loss counters | No nonzero warning/error statistics |

Compared with the original 28.36 FPS native sample, cadence improved by about 64% while frame interval fell from 35.26 to 21.50 ms. Both runs sampled the same GPU clock range and similar skin temperature, though conditions were not perfectly controlled. This is strong evidence that resolution-dependent rendering cost is a major bottleneck, not proof of a specific expensive shader. The game is still GPU-saturated and has not reached the 16.67 ms / 60 FPS target. Raw data remains local in `build/performance/s24-scale75-20260906-212657/`.

The subsequent 50% experiment below supersedes this device setting. Per-pass GPU profiling remains pending.

To restore the native-scale configuration, first save and exit the game, then run from this workspace:

```powershell
& C:\Android\Sdk\platform-tools\adb.exe -s RFCX10M60QT push build/performance/s24-scale75-20260906-212213/Gothic.ini.backup /sdcard/Android/data/org.opengothic.app/files/Gothic.ini
```

This restores the entire backed-up INI, so merge later setting changes first if necessary. The S23 Ultra configuration was not changed.

## S24 50% scale experiment

After the user exited, confirmed the S24 app was no longer running and backed up its 75% configuration to local `build/performance/s24-scale50-20260906-213003/Gothic.ini.backup`. Changed only `vidResIndex=1` to `2`, verified the device file and relaunched successfully (`Status: ok`). This requests 1170x540 3D rendering, one quarter of native pixel count; UI scale and all other settings remain unchanged. Before launch, live skin temperature was 42.9 C and thermal status remained 2.

After the user loaded the comparison scene, captured another trace without changing settings or injecting gameplay inputs. ADB remained connected. A screenshot after capture showed the same waterfall/path viewpoint and 57 FPS.

| Observation | 50% width/height |
| --- | --- |
| Trace duration | 29.999 seconds |
| Vulkan presentation cadence | 58.46 FPS over 1,749 intervals |
| Mean / median interval | 17.11 / 16.79 ms |
| 95th / 99th percentile interval | 21.24 / 23.30 ms |
| Worst interval | 136.79 ms |
| Intervals longer than 33.33 ms | Two: 136.79 and 134.52 ms, about 9.3 and 9.7 seconds into the trace |
| GPU utilization | 100 in 15 of 16 samples; one sample reported 17 |
| GPU allowed maximum | 500-545 MHz |
| GPU current clock | 500-545 MHz except one 252 MHz sample with the utilization dip |
| Live skin temperature before / after | 44.4 / 44.5 C |
| Live battery temperature before / after | 43.8 / 44.0 C |
| Thermal status before / after | 2 / 2 |
| Game/render thread CPU time | 18.309 CPU seconds, about 10.5 ms per presented frame |
| Presentation-call duration | 5.423 ms average, including waits |
| Trace error/loss counters | No nonzero warning/error statistics |

This is close to 60 FPS on average despite a lower sampled GPU ceiling than the 75% run, but it is not a stable 60 FPS result. Both long intervals remain included in the averages and percentiles; their cause is undiagnosed. The isolated utilization dip does not prove that the GPU clock change caused the hitches. No app pause/resume or fatal error appeared during the capture in the inspected process lifecycle/crash logs.

The warm scene comparisons now read:

| 3D scale | Internal resolution | Average presentation FPS | 95th percentile interval |
| --- | --- | --- | --- |
| Native | 2340x1080 | 28.36 | 40.10 ms |
| 75% | 1755x810 | 46.51 | 24.14 ms |
| 50% | 1170x540 | 58.46 | 21.24 ms |

These are separate 30-second samples, not clock-controlled benchmarks or proof of sustained performance throughout gameplay. The freshly reloaded native sample was slower still at 24.76 FPS with a lower clock ceiling. The results establish a strong render-resolution tradeoff; they do not yet identify expensive individual passes. Per-pass GPU profiling is the next step toward retaining more image detail while meeting the frame budget.

The S24 remains at 50% render scale. Raw data and the screenshot are local in `build/performance/s24-scale50-20260906-213206/`. No APK or engine code changed.

To return to the preceding 75% configuration, save and exit before restoring this backup:

```powershell
& C:\Android\Sdk\platform-tools\adb.exe -s RFCX10M60QT push build/performance/s24-scale50-20260906-213003/Gothic.ini.backup /sdcard/Android/data/org.opengothic.app/files/Gothic.ini
```

As with the native-scale backup above, this restores the entire INI. Preserve any later setting changes before restoring it.

## Reproduce from PowerShell

Start from the repository root. Load the same save and leave the character/camera still. Keep brightness, power mode, charging state and warm-up duration comparable. Do not take screenshots or stream logcat during the trace. Record both cold and sustained samples separately.

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
$serial = 'RFCX10M60QT' # Replace with the serial from adb devices -l.
$captureTag = 'opengothic-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
$captureDir = Join-Path 'build/performance' $captureTag
$remoteTrace = "/data/misc/perfetto-traces/$captureTag.perfetto-trace"
New-Item -ItemType Directory -Path $captureDir | Out-Null
& $adb devices -l
& $adb -s $serial shell pidof org.opengothic.app
& $adb -s $serial shell dumpsys thermalservice | Out-File "$captureDir/thermal-before.txt"
& $adb -s $serial push android/tools/performance.pbtxt /data/misc/perfetto-configs/opengothic-performance.pbtxt
if ($LASTEXITCODE -ne 0) { throw 'Could not upload the trace configuration.' }
& $adb -s $serial shell perfetto --txt -c /data/misc/perfetto-configs/opengothic-performance.pbtxt -o $remoteTrace
if ($LASTEXITCODE -ne 0) { throw 'Trace recording failed.' }
& $adb -s $serial shell dumpsys thermalservice | Out-File "$captureDir/thermal-after.txt"
& $adb -s $serial pull $remoteTrace "$captureDir/capture.perfetto-trace"
if ($LASTEXITCODE -ne 0) { throw 'Could not retrieve the trace.' }
```

Recording takes 30 seconds and does not stop/restart the game. Use `/data/misc/perfetto-configs`: this phone denied Perfetto access to a config uploaded under `/data/local/tmp`.

Optional Xclipse-specific sampling in a second PowerShell terminal during recording (paths are not portable to Snapdragon/other phones):

```powershell
$adb = 'C:\Android\Sdk\platform-tools\adb.exe'
& $adb -s RFCX10M60QT shell 'for i in $(seq 1 15); do date +%s; cat /sys/class/devfreq/22200000.sgpu/cur_freq /sys/class/devfreq/22200000.sgpu/max_freq /sys/class/devfreq/22200000.sgpu/interface/current_utilization; sleep 2; done'
```

Each sample prints epoch seconds, current frequency, frequency ceiling and utilization. Only read these files; do not override clocks, governors or thermal protection.

Analyze locally with the [official Perfetto Trace Processor](https://perfetto.dev/docs/analysis/trace-processor). The Python wrapper downloads its platform binary on first use; the baseline used v58.2.

```powershell
curl.exe -L --fail https://get.perfetto.dev/trace_processor -o build/performance/trace_processor
if ($LASTEXITCODE -ne 0) { throw 'Could not download Trace Processor.' }
python build/performance/trace_processor query -f android/tools/performance.sql "$captureDir/capture.perfetto-trace"
```

Driver markers may be absent on other devices; an empty cadence result is not zero FPS. The config also captures CPU scheduling/frequency and [FrameTimeline](https://perfetto.dev/docs/data-sources/frametimeline) where available.

## Next optimization experiments

1. The initial warm native/75%/50% comparisons are complete above. Repeat with comparable warm-up durations and genuinely cold conditions, then test longer gameplay runs. This is a diagnostic quality tradeoff, not yet a new packaged default or a promise of 60 FPS. The existing scaler also disables the AA preset when leaving native resolution, so document that confound if AA is enabled.
2. Per-pass GPU timestamps are now implemented; see the capture below. Measure the paired Lanczos upscaler on the phone before changing additional passes. SSAO and shadow rendering are the next measured candidates.
3. Measure the CPU contribution of world/animation updates, command recording and frame pacing. Source inspection found world/animation updates before the nonblocking frame-fence check and a five-millisecond busy-spin tail in Tempest's sleep implementation. Neither is proven to explain this trace; optimize and A/B test rather than assuming.
4. Rebuild Android and Windows, run regression tests and repeat this scene after each change. Keep Android-specific backend changes in Tempest. Target sustained 60 FPS, not a cold menu reading.

The initial cadence measurement pass above did not change rendering or gameplay. The following GPU profiler and upscaler work require a rebuilt APK.

## Per-pass GPU capture and first renderer optimization

The profiling build adds opt-in `[DEBUG] gpuProfile=1`; see [CONFIGURATION.md](CONFIGURATION.md#opt-in-gpu-profiling).
Tempest records Vulkan timestamps at renderer debug markers and reads completed frame slots without adding GPU waits.
Capture stops after 600 completed frames, is disabled by default, and writes `gpu-profile.csv` beside the writable INI.
Unsupported backends return no GPU timings.

The S24 waterfall capture used OpenGothic `41234dfb` and Tempest `46ed0d56` at 50% render scale.
The installed profiling-only APK SHA-256 was `1AAD89517E7ED44BD9A44F5E3C230DD84E2099F3F56BB363834653F4085196C8`.
Local evidence: `build/performance/s24-gpu-markers-20260906/gpu-profile.csv`, 600 frames; `scene.png` records the scene.

| Marker region | Mean GPU interval per frame |
| --- | --- |
| SSAO | 2.94 ms |
| Tonemapping, including upscale | 2.32 ms |
| ShadowMap #1 | 1.58 ms |
| ShadowMap #0 | 1.10 ms |
| Fog-LUTs | 0.86 ms |
| GBuffer | 0.78 ms |
| GWater | 0.52 ms |
| Sum of all marker regions | 12.061 ms |

These are GPU queue intervals, not CPU frame time or isolated shader execution costs.
They can include stalls, overlap and deferred tiled work.
There was no synchronized clock/thermal capture for these 600 frames, so the sum must not be interpreted as an improvement over the earlier 17.11 ms presentation cadence.
The screenshot's instantaneous FPS reading is not a sustained benchmark.

### Paired Lanczos candidate

The first renderer change reduces the existing Lanczos upscaler from 21 nearest texture reads to 12 bilinear reads.
It computes the one-dimensional weights once and pairs the positive central weights, retaining the original footprint and omitted corners.
Native-resolution rendering keeps its existing sampler and shader path.
No resolution defaults, SSAO, shadow quality or gameplay settings were reduced for this change.

The GPU comparison initially found a large mismatch near a zero-distance weight in the original sine/division expression.
Using the analytic limit of one for distances below 0.0001 resolves that numerical instability.
Both GPU reference and optimized filters use this stable weight function.
Hardware interpolation has finite precision, so the paired result is not bit-identical to 21 individual reads.

Verification completed locally:

- Windows Release game build and Android ARM64 `assembleDebug lintDebug` succeeded.
- 114,444 ideal-filter CPU cases passed, maximum absolute difference `2.84217e-14`.
- 40,441,744 Vulkan output-pixel comparisons passed, including RGBA8, HDR R11G11B10UF, borders and both S24 upscale sizes.
  Maximum error was `0.00645709` of the input range; mean error was `0.000841665`, below the fixed `0.01` limit.
- Vulkan validation passed, including device teardown.
  The test exposed a separate Tempest sampler-cache leak and invalid allocation-failure path; Tempest `1dfee2d0` fixes both.
- Both timestamp tests and all three controller/save/camera tests passed.
- APK inspection confirmed only `lib/arm64-v8a/libopengothic.so`, no bundled Gothic assets, NativeActivity, SDK 24/35, Vulkan 1.1 and landscape support.
  APK v2 signature and 16 KB ZIP alignment checks passed.

Candidate APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `09A85FA94D969BC8F0C54EA0E7B98A1108658206267745464C8604554D12D73C`.
The initial install was blocked by an empty ADB device list; the successful S24 installation and measurements below supersede that blocker.
Test commands are in [tests/rendering/README.md](../tests/rendering/README.md).

Game shutdown/relaunch during this testing is authorized; existing saves and assets must still be preserved.

### S24 installation and paired-filter measurements, 2026-09-06

After USB reconnection, the candidate APK above installed with `adb install -r` (`Success`) and cold-launched with `Status: ok`.
The waterfall scene was visible and the process remained alive after both traces.
The writable INI was backed up and confirmed unchanged: 50% scale, FPS display, quicksave/potion keys and GPU profiling enabled.
The previous CSV was copied before restarting; no saves or assets were removed.
Local evidence is in `build/performance/s24-paired-lanczos-20260906-221727/`.

The new 600-frame GPU capture shows a localized improvement:

| Marker | Previous mean | Paired-filter mean |
| --- | --- | --- |
| Tonemapping / upscale | 2.32 ms | 0.78 ms |
| SSAO | 2.94 ms | 2.91 ms |
| ShadowMap #1 | 1.58 ms | 1.62 ms |
| ShadowMap #0 | 1.10 ms | 1.09 ms |
| Sum of marker regions | 12.061 ms | 10.421 ms |

The approximately 66% reduction in the tonemapping region, with largely unchanged neighboring regions, supports a real benefit from the filter change.
It does not prove a 66% whole-game FPS improvement.
Clock/thermal conditions were not synchronized between these two short GPU captures.

Two subsequent 30-second traces recorded presentation cadence after the bounded GPU capture had finished:

| Observation | First sample | Warmer sample |
| --- | --- | --- |
| Presentation FPS | 86.89 | 75.25 |
| Mean / median interval | 11.51 / 11.52 ms | 13.29 / 13.36 ms |
| 95th / 99th percentile interval | 13.29 / 14.43 ms | 15.15 / 16.13 ms |
| Worst interval | 17.48 ms | 20.98 ms |
| Intervals | 2,598 | 2,250 |
| Sampled GPU clocks | 700-800 MHz | 600-650 MHz |
| Live skin temperature before / after | 42.6 / 43.4 C | 44.1 / 44.3 C |
| Android thermal status | 2 | 2 |
| Main game thread CPU time | 23.183 s | 26.367 s |

Neither trace reported nonzero trace-quality errors in the checked Perfetto statistics.
Sampling GPU clocks started shortly after each trace began and partially extends beyond the trace, rather than aligning exactly with its frame intervals.
The earlier 50% baseline was 58.46 FPS at predominantly 500-545 MHz and 44.4-44.5 C skin temperature.
The newer samples are encouraging, but their higher clocks prevent attributing the entire FPS difference to this patch.
These are short stationary scene tests, not a guarantee of sustained 60 FPS throughout gameplay.

Startup logs contain the existing `Failed to created DmLoader object. Out of memory?` warning and ZenKit `1 bytes overflowed in section f590` messages.
No fatal crash was observed during these captures; music correctness and those warnings remain separate follow-up checks.

Next: repeat a longer warm gameplay run and the 75% render scale, with comparable conditions.
SSAO remains the largest measured GPU region, while the warmer trace also shows high main-thread CPU use.
Investigate those separately and retain this APK/capture as the paired-filter baseline.

## SSAO convergence cleanup, 2026-09-06

OpenGothic `d467c1ba` retains the same SSAO sample budget and convergence threshold, removes the redundant workgroup-wide vote counter, and exits the sampling loop when the whole group has converged.
Previously, converged lanes stopped sampling but continued synchronization rounds through the remaining iterations.
Shared state is initialized by one lane and a barrier ensures all lanes read the result before another iteration resets it.
The existing NaN comparison behavior is retained; this is not a change to sky occlusion, reconstruction, sample placement or blur quality.

A separate `SSAO blur` marker now follows `SSAO`.
Compare their sum with earlier captures, which included both operations under `SSAO`.

Verification:

- Windows Release and Android ARM64 `assembleDebug lintDebug` succeeded.
- All eight rendering, timestamp and controller/save/camera regression tests passed.
- The new Vulkan convergence test verified 1,048,576 lane cases against the counter-based decision, including uniform exit and identical evaluated sample signatures.
  Its synthetic synchronization-round reduction is not a measured in-game speedup.
  See [rendering tests](../tests/rendering/README.md#ssao-convergence-regression) for coverage and limitations.
- APK v2 signing and 16 KB ZIP alignment checks passed; manifest inspection confirmed ARM64, NativeActivity, SDK 24/35 and Vulkan 1.1.
- Installation with `adb install -r` returned `Success`; cold launch returned `Status: ok`.
  The user loaded the waterfall save; a screenshot showed the expected scene and the process remained alive after the trace.
  This was a visual sanity check, not a pixel-exact comparison of complete SSAO output.
- The previous GPU CSV and writable INI were backed up before restarting.
  No game assets, saves or graphics settings were removed or changed.

APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `88B31EDB61AAEC8B9572914C20EF68A1493A54C142235318B37DBF900980897F`.
Local evidence: `build/performance/s24-ssao-20260906-223243/`.

The 600-frame GPU capture at 50% scale measured:

| Marker | Mean interval |
| --- | --- |
| SSAO calculation | 2.48 ms |
| SSAO blur | 0.50 ms |
| SSAO combined | approximately 2.98 ms |
| ShadowMap #1 / #0 | 1.80 / 1.26 ms |
| Tonemapping | 0.92 ms |
| Sum of marker regions | 11.528 ms |

The preceding paired-filter capture measured combined SSAO at 2.91 ms and tonemapping at 0.78 ms.
Other regions also grew in this capture, so the raw timings are not a controlled comparison of the SSAO code alone.
No in-game performance improvement from this cleanup is established yet.

The subsequent 30-second trace measured 71.99 presentation FPS across 2,150 intervals.
Mean/median intervals were 13.89/13.85 ms; p95/p99 were 15.73/16.84 ms; the worst was 20.77 ms.
Sampled GPU clocks were 600-650 MHz, with utilization ranging from 91 to 100.
Live skin temperature rose from 42.6 to 43.3 C, with thermal status 2 throughout the trace endpoints.
Main-thread CPU use was 24.736 seconds; no nonzero trace-quality errors were reported by the checked statistics.
The run is not directly comparable to the preceding 75.25 FPS sample: its frequency distribution and warm-up state differ.

Next priorities are a more tightly controlled before/after SSAO comparison and CPU profiling of frame preparation and animation updates.
Do not lower quality or attribute the entire frame-time variation to this cleanup without those measurements.

## CPU profiling and worker completion waits, 2026-09-06

Tempest `23d8764a` adds opt-in scoped Android CPU markers, with balanced scope tests and a no-op backend on other platforms.
OpenGothic `eb1f4020` instruments frame stages and worker execution/waits without changing their ordering.
The first incremental link failed because Tempest's existing source glob did not notice the new implementation file.
Source/header globs now use `CONFIGURE_DEPENDS`; Android and Windows rebuilt successfully.
Enable `[DEBUG] cpuProfile=1` only for diagnostics as described in [CONFIGURATION.md](CONFIGURATION.md#opt-in-cpu-profiling).

The diagnostic APK installed and cold-launched successfully on the S24.
Its SHA-256 was `24D98CEE071CE1F335F6994A1D09344C381E0101F40B3C4D9FA0CCCF31FCA41E`.
The phone's INI was backed up before enabling CPU tracing; render scale remained 50%.
Local evidence: `build/performance/s24-cpu-20260906-224147/`.

The 30-second waterfall trace contained 2,090 complete frame scopes:

| Main-thread region | Mean wall time per call | Total scheduled CPU time |
| --- | --- | --- |
| Simulation | 2.687 ms | 5.610 s |
| Animation | 5.250 ms | 10.961 s |
| Command recording | 4.173 ms | 6.307 s |
| Presentation | 1.766 ms | 1.054 s |
| Worker completion wait, 4,186 calls | 0.632 ms | 2.643 s |
| All complete frame scopes | 14.251 ms | 24.717 s |

Regions are inclusive: worker execution/waits overlap animation, so their totals must not be added to the animation total.
The worker wait used approximately 1.26 CPU ms per frame, almost all of its wall duration.
No skipped-frame or frame-cap sleep markers appeared in this sample.
Consequently, frame-update ordering and Tempest's sleep policy were left unchanged.

### Notification-based worker completion

OpenGothic `cacd3f45` replaces repeated `yield()` polling with C++20 atomic waiting on the observed completion count.
Workers notify after publishing completion, including shutdown.
The caller still executes its normal share of tasks and observes all results before returning.
Task scheduling, simulation, animation sampling and graphics quality are otherwise unchanged.

Windows Release and Android ARM64 `assembleDebug lintDebug` passed.
The 11,000-batch worker regression passed both on Windows and as an ARM64 executable running on the S24.
It checks delayed completion, exactly-once execution, result visibility, empty/small/chunk-boundary batches and clean shutdown.
The CPU trace pairing test and the existing eight rendering/timestamp/controller tests also passed locally.
Commands are in [tests/workers/README.md](../tests/workers/README.md).

Candidate APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `7E42EA921B3C67E8D394F7ED958D5C5E43EA283EA5EED7022C02F7287676FFD1`.
APK signature/alignment checks passed; upgrade installation returned `Success` and cold launch returned `Status: ok`.
The waterfall scene was visible after loading.
Local evidence: `build/performance/s24-worker-wait-20260906-225113/`.

| Observation | Yield-loop baseline | Atomic-wait candidate |
| --- | --- | --- |
| Presentation FPS | 69.94 | 52.45 |
| Mean / p95 / p99 frame interval | 14.30 / 16.16 / 17.39 ms | 19.07 / 23.44 / 27.23 ms |
| Worst frame interval | 21.24 ms | 34.86 ms |
| Main-thread CPU time | 24.816 s | 20.523 s |
| Worker wait CPU time | 2.643 s | 0.786 s |
| Worker wait CPU per complete frame | approximately 1.26 ms | approximately 0.50 ms |
| Worker wait wall time per call | 0.632 ms | 0.818 ms |
| Sampled GPU clock ceiling | 545-600 MHz | 400-450 MHz |
| Live skin temperature before / after | 42.7 / 43.3 C | 43.9 / 44.1 C |
| Thermal status at endpoints | 2 | 2 |

The candidate reduced measured CPU work in completion waits, including when normalized per frame.
However, the two runs have different clock and thermal conditions, and the candidate's measured FPS is worse.
This is not an established frame-rate improvement or proof that the wait change has no latency regression.
Both traces reported no nonzero errors in the checked Perfetto quality statistics.
The game was force-stopped after the candidate trace to let the phone cool; existing saves/assets were preserved.

Next: repeat with comparable temperature/clock conditions, using identical CPU tracing settings, and check animation/wait latency as well as FPS.
Keep or revise the waiting policy based on that comparison; do not count fewer CPU seconds caused by fewer frames as an optimization gain.
Animation remains the largest measured main-thread region.

### Worker-wait retest after cooling, 2026-09-06

The user cooled and reconnected the S24.
The installed APK's on-device SHA-256 matched the atomic-wait candidate above; no rebuild or reinstall was needed.
The app was stopped initially, live skin temperature was 33.8 C and thermal status was 0.
Cold launch returned `Status: ok`, and the user loaded the same waterfall scene.
Render scale remained 50%, with both CPU and bounded GPU profiling enabled.
The previous GPU CSV was preserved before launch.

Local evidence: `build/performance/s24-worker-cooled-20260906-231800/`.
The first trace is `capture.perfetto-trace`; the later, warmer trace is `capture-warm.perfetto-trace`.
Both traces lasted 30 seconds and reported no nonzero errors in the checked Perfetto quality statistics.

| Observation | After cooling | Later warm sample |
| --- | --- | --- |
| Presentation FPS | 97.80 | 85.73 |
| Presentation intervals | 2,923 | 2,564 |
| Mean / median interval | 10.23 / 10.14 ms | 11.66 / 11.64 ms |
| p95 / p99 interval | 12.16 / 13.47 ms | 13.03 / 13.92 ms |
| Worst interval | 16.97 ms | 16.98 ms |
| Sampled GPU clocks / ceiling | 700-800 MHz | 700-800 MHz |
| Live skin temperature before / after | 38.8 / 41.0 C | 43.8 / 44.2 C |
| Thermal status before / after | 0 / 1 | 2 / 2 |
| Main-thread CPU time | 22.059 s | 25.122 s |
| Worker-wait CPU time | 0.971 s | 1.164 s |
| Worker-wait CPU per complete frame | approximately 0.33 ms | approximately 0.45 ms |
| Worker-wait wall time per call | 0.392 ms | 0.586 ms |
| Animation wall time per call | 3.494 ms | 4.532 ms |

The new waits continued to consume less scheduled CPU per frame than the earlier yield-loop baseline's approximately 1.26 ms.
The latest warm wait duration also did not show the earlier candidate's increased mean wall latency, although CPU/GPU clock conditions still differ between runs.
The same candidate previously measured 52.45 FPS with a 400-450 MHz GPU ceiling; its much faster cooled result confirms that the earlier low reading was not a fixed throughput limit of this APK.
It does not isolate the wait change's FPS benefit or prove that thermal state explains every difference.
Similar skin temperatures alone do not imply identical thermal or frequency conditions: this warm run still held higher GPU clocks than the old yield-loop baseline.

The game remained alive through both captures and was stopped afterward to avoid further heating.
No assets, saves, graphics settings or clock controls were changed.
Retain the worker-wait candidate for further testing; long gameplay runs and a clock/temperature-matched baseline comparison remain outstanding.
These stationary, uncapped 50% render-scale samples are not a native-resolution or whole-game sustained-60-FPS guarantee.

### Optional half-resolution SSAO, 2026-09-06

The latest 75% render-scale capture contained 600 frames with a mean total marker span of 17.179 ms.
SSAO calculation averaged 5.90 ms and its blur 1.01 ms, making their combined 6.91 ms the largest measured target.
Local baseline CSV: `build/performance/s24-scale75-latest-20260906-232922/gpu-profile.csv`.
These are GPU marker intervals, not isolated shader execution costs or proof of an overall FPS limit.

The new optional `[ENGINE] ssaoHalfResolution=1` calculates AO at half the scene dimensions and resolves it to scene resolution with a depth-aware filter.
At the user's preferred 75% scene scale, this means 878x405 AO calculation for a 1755x810 scene.
The sampling radius and sample budget per evaluated pixel remain unchanged.
Each 2x2 depth block contributes its nearest surface, with linear representative depth stored beside AO in RG32F.
The nine-tap resolve combines spatial and depth weights; missing background representatives resolve to unoccluded rather than borrowing foreground shadows.
The final lighting input remains an R8 image at scene resolution.
This trades some fine AO detail for fewer evaluated pixels; it is not an identical-image optimization or a forced Android setting.
The initial candidate kept full-resolution SSAO as the default.
The subsequent user-requested defaults change selects half-resolution SSAO on Android, while preserving explicit settings and desktop defaults.
Full resolution remains the fallback when RG32F storage is unsupported.
See [configuration](CONFIGURATION.md#optional-half-resolution-ambient-occlusion) for enabling and reverting it.

Verification completed locally:

- Full Windows Release executable and Android ARM64 native library/APK builds passed; Android lint passed.
- Both new production shader variants passed SPIR-V validation.
- The original full-resolution SSAO shader compiled to byte-identical SPIR-V before and after the change: SHA-256 `5373072267E72ADCCF0935CFA0055FA7582DFE7684838E93E01622A1C0A216E8`.
- The new Vulkan test exercised the production depth-selection/resolve helpers on 8,530,926 pixels, including 1x1, single-row/column, odd dimensions, and 1755x810.
- Constant AO, full occlusion, steep depth edges, thin foreground geometry, alternating near/far surfaces and smooth depth gradients passed; maximum CPU-reference difference was 0.000000298023.
- All eleven rendering, GPU timing, controller, CPU trace and worker tests passed; the GPU tests reported no Vulkan validation errors.
- APK signing, ARM64-only manifest metadata and 16 KiB ZIP alignment checks passed.

Test command after building the existing rendering-test project:

```powershell
ctest --test-dir build/rendering-tests -C Release --output-on-failure
```

The test executables require the built Tempest DLL directory on `PATH` and installed Vulkan validation layers, as in the earlier rendering comparisons.
These helper tests do not establish complete AO image equivalence or real-device performance.

Candidate APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `B11847B1A7B7EBFC895202BF2B8B75E1146EB7F3DF6925D175C215976961D60F`.
Local build logs, a pre-change screenshot and an unchanged device INI backup are in `build/performance/ssao-half/`.
The screenshot taken immediately before stopping the game shows a downward-looking barrel/path scene, not the earlier waterfall camera; it must not be treated as a matched waterfall image comparison.

The S24 disconnected from ADB during the build; the initial installation attempt returned `adb.exe: device 'RFCX10M60QT' not found`.
After the user reconnected it, `adb install -r` succeeded and the installed APK hash matched the candidate above.
The device INI was backed up again and matched the earlier backup before enabling the option; 75% scale, FPS, shortcut and sensitivity preferences were preserved.
Launch returned `Status: ok`; the user loaded the waterfall scene, which was confirmed by screenshot.

### Half-resolution SSAO on the S24

Local evidence: `build/performance/ssao-half/` (`gpu-profile.csv`, `capture.perfetto-trace`, `performance.txt`, thermal snapshots, `gpu-clocks.txt`, `after-launch.png`, `logcat.txt`).
The bounded GPU capture recorded 600 frames; the subsequent Perfetto trace recorded 30 seconds of the waterfall scene.

| Observation | Earlier full-resolution AO | Half-resolution AO |
| --- | --- | --- |
| Scene resolution | 1755x810 | 1755x810 |
| AO calculation | 5.90 ms | 2.27 ms |
| AO blur / depth-aware resolve | 1.01 ms | 0.33 ms |
| Combined AO | 6.91 ms | 2.60 ms |
| Total GPU marker span | 17.179 ms | 15.382 ms |
| Fog-LUTs | 1.65 ms | 2.11 ms |
| ShadowMap #1 | 1.65 ms | 2.01 ms |
| Tonemapping | 0.84 ms | 1.03 ms |

The localized AO reduction is encouraging, but unrelated passes were slower in the candidate capture.
The earlier GPU-only capture lacks matching clock samples, so this is not a controlled clock/thermal-matched A/B test or a measured 4.31 ms whole-frame saving.
The two GPU captures and the later Perfetto cadence measurement also cover different time windows.

The candidate's presentation cadence averaged 67.56 FPS over 2,020 intervals.
Mean/median intervals were 14.80/14.76 ms; p95/p99 were 17.91/19.66 ms, with a worst interval of 23.80 ms.
Sampled GPU clocks were 650-700 MHz at 100% reported utilization.
Live HAL skin temperature rose from 41.7 to 42.9 C, with thermal status changing from 1 to 2.
The checked Perfetto quality statistics contained no nonzero errors.
The app remained alive and its collected logcat contained no fatal signal, Vulkan error or abort matches.
This is a short stationary-scene result, not proof of sustained 60 FPS throughout the game.

The waterfall rendered without obvious gross corruption in the screenshot.
Fine AO quality still needs a matched full/half screenshot comparison and moving-camera review, especially around thin geometry and contact shadows.
The game was stopped after measurement while rebuilding the requested Android defaults; assets and saves were preserved.

### Final Android-defaults build

Commit `2f2398c4` selects 75% scene resolution when the writable Android render-scale key is missing, and half-resolution SSAO when no explicit AO preference exists.
Desktop keeps native scene resolution and full-resolution SSAO by default.
The existing original-game display-mode index is still deliberately not imported as an OpenGothic render scale.
Existing writable render-scale choices and explicit AO preferences are not migrated or overwritten.

Both the Windows Release executable and ARM64 APK rebuilt successfully after this change; Android lint passed again.
The final APK at `android/app/build/outputs/apk/debug/app-debug.apk` has SHA-256 `406943DEBA370ECBCA5C5F279E7251D610D4BD25E88E1EA608507C41C07FF93F`.
Signature, ARM64 metadata and 16 KiB ZIP alignment checks passed again.
Upgrade installation succeeded, the on-device APK hash matched, and cold launch returned `Status: ok`.
The user's explicit 75%/half-AO settings remained intact; the waterfall was visible in `build/performance/ssao-half/defaults-launch.png`.
The game was left running for visual review.
This final rebuild changes only platform defaults, not the measured SSAO algorithm; no second timing result is attributed to its screenshot FPS counter.

### Fractional Android frame pacing, 2026-09-07

OpenGothic commit `2414cef7` uses Tempest `31390de7` for application-side frame pacing on Android.
The old millisecond interval used `1000/60 = 16`, nominally 62.5 FPS, and `Application::sleep` busy-polled its final five milliseconds.
The new schedule distributes fractional nanoseconds across frames, sleeps with a monotonic deadline, and rebases after missed deadlines without catch-up bursts.
Android's other `Application::sleep` callers now use blocking sleeps too; desktop sleep/limiter behavior is unchanged.
Focus, resize and world-loading events reset the pacing schedule.
Android initializes a missing writable `zMaxFPS` to 60 while preserving existing writable values and the positive SystemPack FPS override.
The device's existing 75% scale, half-resolution SSAO and input preferences were preserved; startup added `zMaxFPS=60` automatically.
See [frame-rate configuration](CONFIGURATION.md#frame-rate-limit-and-low-power-waits) for uncapped gameplay and lower limits.

This is not Swappy integration or display-synchronized presentation.
It caps application work without requesting a display refresh-rate change or overriding clocks/thermal controls.
The FPS counter now uses the actual elapsed clock on Android rather than assuming a requested delay completed exactly on time.

Verification:

- Windows Release executable, ARM64 APK and Android lint passed.
- All twelve local tests passed, including the new Tempest pacing test and the existing Vulkan rendering/controller/worker regressions.
- The deterministic pacing test checked 104,649 exact deadlines at nine rates from 1 to 1,000 FPS, plus disabled pacing, changed/unchanged rates, lifecycle reset and a five-second stall.
- On Windows, 120 paced intervals took 2.00127 s with zero thread CPU reported at the OS counter's resolution.
- The same native ARM64 test ran on the S24: 2.00059 s wall time and 0.019968 s thread CPU for 120 intervals.
- APK signature, ARM64-only metadata and 16 KiB ZIP alignment passed.
- Upgrade installation and cold launch succeeded; the installed APK SHA-256 matched `7AD0A5A9094DB895216507D2CC7247D4F23663AFF0B64DAE169180B87B2A500E`.
- Home/background and return to the activity completed successfully with the same process; lifecycle logs recorded pause/focus changes.
- The waterfall rendered before and after the lifecycle test; collected logcat had no fatal signal, Vulkan error or abort matches.

APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
Local evidence: `build/performance/frame-pacing/`.
The `capture.perfetto-trace` and `capture-warm.perfetto-trace` files each cover 30 seconds; matching analysis, thermal snapshots, GPU clock samples and screenshots are alongside them.
The initial device configuration is preserved in `Gothic.ini.backup`.

| Observation | First capped run | Warmer repeat after pause/resume |
| --- | --- | --- |
| Presentation cadence | 59.72 FPS | 35.68 FPS |
| Presentation intervals | 1,783 | 1,065 |
| Mean / median interval | 16.74 / 16.73 ms | 28.03 / 27.96 ms |
| p95 / p99 interval | 18.81 / 19.98 ms | 33.44 / 35.47 ms |
| Worst interval | 23.09 ms | 41.22 ms |
| Sampled GPU clock / ceiling | 600-650 MHz | 252-315 MHz |
| Live HAL skin temperature | 42.5 to 43.2 C | 44.3 to 43.8 C |
| Thermal status | 2 | 2 |
| Pacing wait wall / scheduled CPU | 2,847.815 / 26.143 ms | No pacing-wait scopes |

The first capture had 1,432 pacing waits, averaging 1.989 ms wall time each, without the old multi-millisecond spin.
The warmer repeat had no pacing waits: work already exceeded the frame interval, so the limiter imposed no extra wait.
Both traces reported no nonzero errors in the checked Perfetto quality statistics.
GPU utilization samples were 95-100% in the first run and 100% in the warmer repeat.
The clock ceiling drop is directly observed; these runs do not isolate temperature from other Samsung/device power-policy effects, especially across pause/resume.
They do not establish a battery-power reduction, display-scanout cadence, or sustained 60 FPS.

The early bounded GPU capture averaged a 16.441 ms marker span, including 3.58 ms for both shadow maps and 2.26 ms for fog LUTs.
It precedes the heavily throttled repeat and must not be interpreted as that repeat's GPU timing.
Next rendering candidate: configurable lower-resolution shadow maps with matched quality and timing comparisons.
Proper Vulkan display pacing and a longer continuous thermal test remain outstanding; a 60 FPS cap alone did not meet the hot-device target.
The game was force-stopped after the warmer measurement to cool; existing saves/assets remain intact.

To build and run the standalone pacing test on Windows:

```powershell
cmake -S lib/Tempest/Tests/frame-pacing -B build/frame-pacing-tests
cmake --build build/frame-pacing-tests --config Release
ctest --test-dir build/frame-pacing-tests -C Release --output-on-failure
```

For the actual device wait test:

```powershell
$androidCmake = "$env:ANDROID_HOME/cmake/3.22.1/bin/cmake.exe"
& $androidCmake -S lib/Tempest/Tests/frame-pacing -B build/android-frame-pacing-tests -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$env:ANDROID_HOME/cmake/3.22.1/bin/ninja.exe" `
  "-DCMAKE_TOOLCHAIN_FILE=$env:ANDROID_HOME/ndk/27.0.12077973/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
& $androidCmake --build build/android-frame-pacing-tests
& "$env:ANDROID_HOME/platform-tools/adb.exe" push build/android-frame-pacing-tests/frame-pacing-tests /data/local/tmp/opengothic-frame-pacing-tests
& "$env:ANDROID_HOME/platform-tools/adb.exe" shell chmod 700 /data/local/tmp/opengothic-frame-pacing-tests
& "$env:ANDROID_HOME/platform-tools/adb.exe" shell /data/local/tmp/opengothic-frame-pacing-tests
```

### Configurable conventional shadow maps, 2026-09-07

Commit `6811afc3` replaces the fixed 2048 shadow-map dimension with a validated `[ENGINE] shadowMapResolution` choice.
Supported dimensions are 1024, 1536 and 2048; the default and unsupported-value fallback remain 2048 on Android and desktop.
Both conventional sunlight shadow maps use the selected dimension, including their fog/indirect-lighting consumers.
The original texture-size-aware sampling code and shadow projections are unchanged.
Settings reload still retires old maps through the existing resource recycling path; no new backend or Tempest changes are required.
The effective resolution is reported in `log.txt`.

1024 maps contain 25% of the original depth texels, and 1536 maps contain 56.25%.
These are allocation/raster-target size reductions, not measured pass-time or FPS improvements.
Shadow geometry, draw counts and screen-space sampling remain; lower resolutions trade fine shadow detail for potential memory-bandwidth/raster savings.
See [shadow configuration](CONFIGURATION.md#shadow-map-resolution) for enabling and reverting the setting.

Windows Release and the Android ARM64 native library/APK built successfully; Android lint passed.
All twelve existing rendering, GPU timing, controller, CPU trace, worker and frame-pacing regression tests passed.
No new shadow-quality-specific automated test was added; visual and performance comparisons remain necessary.
APK signing, ARM64-only manifest metadata and 16 KiB ZIP alignment checks passed.

Candidate APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: `0621E2FB2E23053D550767B453E18742F999CE935CBA147C9DA4E72914D27CDB`.
Local build logs and the unchanged device INI backup are in `build/performance/shadow-resolution/`.
Separate local candidate INIs for 1024, 1536 and 2048 preserve 75% scene resolution, half-resolution SSAO, the 60 FPS cap and the user's input/FPS preferences.

The S24 disappeared from ADB just before the first installation attempt: `adb.exe: device 'RFCX10M60QT' not found`.
After reconnection, upgrade installation succeeded and the installed APK hash matched the candidate above.
The 1024 and 1536 INI settings were applied with the game stopped and confirmed in the native log after successful launches.
Existing saves and assets were preserved.

The 1024 waterfall trace recorded 59.99 presentation FPS over 1,791 intervals, with mean/median 16.67/16.65 ms, p95/p99 18.90/20.21 ms and worst 22.28 ms.
Its 600-frame GPU capture reported 0.41 ms for ShadowMap #0 and 0.99 ms for ShadowMap #1, with a 10.807 ms total marker span.
The phone had cooled to 37.9 C skin temperature before launch and reached 41.2 C after tracing; this must not be compared directly with the heavily throttled earlier run.

The first 1536 launch loaded the Xardas room instead of the waterfall.
Its bounded GPU capture had already completed by the time the user corrected the scene.
That CSV is preserved as `1536-wrong-scene-gpu-profile.csv` and is excluded from the shadow-performance comparison.
A fresh `1536-correct.perfetto-trace` and `1536-correct.png` captured the corrected waterfall scene without restarting.
This trace recorded 59.80 presentation FPS over 1,787 intervals, mean/median 16.72/16.57 ms, p95/p99 19.79/21.65 ms and worst 25.58 ms.
Live skin temperature rose from 43.5 to 43.8 C at thermal status 2; the observed GPU ceiling remained high, so this is not equivalent to the earlier 252-315 MHz hot-phone test.
Both checked traces reported no nonzero Perfetto quality errors.

The app was then restarted at the same 1536 setting to reset the bounded GPU profiler, and loading the waterfall first was confirmed by screenshot.
Subsequent 1536 GPU results use this retry, not the invalid Xardas capture.

### Shadow-resolution comparison and warm repeat

All runs below used the same installed APK, 1755x810 scene resolution, half-resolution SSAO and the 60 FPS cap.
The waterfall save/camera was confirmed by screenshots for each valid run.
Each GPU CSV contains 600 completed frames; each presentation-cadence trace covers 30 seconds after the screenshot check.
GPU and cadence captures cover different time windows, and the clock logs include launch/loading as well as gameplay.

| Observation | 1024, first run | 1536, valid retry | 2048 reference | 1024, warm repeat |
| --- | --- | --- | --- | --- |
| Presentation cadence | 59.99 FPS | 59.75 FPS | 46.71 FPS | 35.45 FPS |
| Mean frame interval | 16.67 ms | 16.74 ms | 21.41 ms | 28.21 ms |
| p95 / p99 interval | 18.90 / 20.21 ms | 18.76 / 19.97 ms | 25.80 / 27.32 ms | 31.13 / 32.44 ms |
| Worst interval | 22.28 ms | 22.96 ms | 30.65 ms | 33.96 ms |
| ShadowMap #0 | 0.41 ms | 0.75 ms | 2.25 ms | approximately 0.98 ms |
| ShadowMap #1 | 0.99 ms | 1.30 ms | 3.07 ms | 2.29 ms |
| Fog-LUTs | 1.36 ms | 1.67 ms | 3.40 ms | 3.10 ms |
| SSAO calculation | 1.75 ms | 1.94 ms | 4.42 ms | 4.53 ms |
| Tonemapping | 0.86 ms | 0.91 ms | 1.67 ms | 2.08 ms |
| Total GPU marker span | 10.807 ms | 12.282 ms | 25.827 ms | 25.411 ms |
| Live skin temperature before launch / after trace | 37.9 / 41.2 C | 44.3 / 44.6 C | 44.5 / 44.2 C | 44.4 / 43.6 C |
| Late clock samples | 600-800 MHz | 600 MHz | 400-450 MHz | 252 MHz |

The lower-resolution maps reduce depth allocation/raster target size and the valid lower-resolution runs had smaller shadow-pass intervals.
However, the sequential runs experienced substantially different GPU clocks and power conditions.
The much slower unrelated passes in the 2048 and warm-1024 captures confirm that their FPS differences cannot be attributed solely to shadow resolution.
No fixed millisecond saving, percentage FPS improvement or battery-power reduction is established by this sequence.
Temperature alone is not a reliable substitute for frequency/power-state matching.

All three resolutions rendered without obvious gross corruption in the inspected waterfall screenshots.
The 1024 option sacrifices fine shadow edges; these stationary images do not establish equivalent quality or rule out shimmer, acne or detached shadows in other scenes or while moving.
All four checked traces contained no nonzero Perfetto quality errors, and their collected logcat files contained no fatal-signal, Vulkan-error or abort matches.
The warm 1024 repeat still failed the sustained-60-FPS target at its severely reduced clock ceiling.

The device's writable INI is left at `shadowMapResolution=1024` for the next mobile-performance tests; the packaged default remains 2048 and no other quality/input settings changed.
The game was stopped after the final trace to cool, with saves and assets preserved.
Source and results are committed locally; nothing was pushed.
Next: investigate fog and the remaining GPU/CPU costs, and eventually repeat controlled, longer thermal tests rather than treating a short near-60-FPS sample as completion.

### Cooled 1024-shadow run without restarting, 2026-09-07

The same installed APK and writable settings were retained: 75% scene resolution, half-resolution SSAO, 1024 shadows and the 60 FPS cap.
The waterfall scene was confirmed by screenshots before and after measurement.
The game remained in the foreground without restarting, reloading or changing settings between captures; normal game time and lighting continued to advance.
Nine 30-second Perfetto captures covered 270 seconds of a session spanning approximately six minutes, with gaps for capture transfer and analysis.
These are sampled Vulkan presentation intervals, not continuous display-scanout measurements or a controlled comparison against another build.

| Observation | Early capture | Final capture |
| --- | --- | --- |
| Presentation cadence | 59.93 FPS | 59.83 FPS |
| Intervals | 1,796 | 1,789 |
| Mean / median interval | 16.69 / 16.67 ms | 16.71 / 16.58 ms |
| p95 / p99 interval | 18.82 / 20.77 ms | 20.23 / 21.68 ms |
| Worst interval | 28.18 ms | 24.90 ms |
| Live skin temperature after capture | 40.5 C | 43.7 C |
| Main-thread CPU time in 30 seconds | 18.145 s | 20.564 s |
| Mean pacing sleep, when a wait occurred | 5.410 ms | 3.118 ms |

All nine window averages were between 59.83 and 59.99 FPS.
Live skin temperature was 39.0 C immediately before the early trace and reached 43.7 C, with reported thermal status progressing from 0 to 2.
GPU clock samples were mostly 700-800 MHz, with a brief 545-650 MHz dip and final samples at 700 MHz.
The sustained clock log contains 140 two-second samples; neither it nor the early clock log changes clocks, power policy or thermal controls.
This run did not reproduce the previous 252 MHz condition or its approximately 35 FPS result.
It supports near-60-FPS operation for this stationary scene over this session, not a guarantee for longer play, other scenes or severe throttling.

The bounded, early 600-frame GPU capture averaged an 11.695 ms total marker span.
SSAO calculation/upsampling accounted for 1.91/0.28 ms, fog LUTs for 1.48 ms, and the two shadow maps for approximately 0.45/1.08 ms.
Those GPU markers precede the later cadence captures and must not be used as measurements of the final warm state.
The final CPU trace still contained 1,690 pacing waits across 1,789 complete frame scopes, but animation wall time increased from 3.742 to 5.765 ms per call.
Wall-time regions overlap and include scheduling delays; these observations do not isolate a new optimization or establish power savings.

All nine traces reported no nonzero capture-quality errors.
The collected process logcat contained no fatal-signal, fatal-exception, Vulkan-error or abort-message matches.
Screenshots, traces, clock logs, CPU/GPU reports and the unchanged writable INI are retained locally in `build/performance/cooled-1024-sustained/`.
No source, APK, device setting or asset was changed for this measurement; the game was left running afterward.
Next candidates remain fog rendering and animation/worker costs, followed by longer thermal verification that reproduces the low-clock condition.

### Later slowdown in the same uninterrupted session

After the user reported 49-50 FPS, another 30-second trace was taken without restarting or changing settings.
The process ID remained 25567 and the screenshot confirmed the same waterfall position, with normal time-of-day progression.
This trace began approximately fourteen minutes after the early trace began, not fourteen minutes after the save loaded.
It averaged 46.99 presentation FPS over 1,404 intervals: mean/median 21.28/20.98 ms, p95/p99 26.51/28.65 ms and worst 34.23 ms.
The screenshot taken during tracing showed 43 FPS; its instantaneous counter is not the window average.

Live skin temperature was 44.4 C before and 44.3 C after the trace, with reported thermal status 2.
All fifteen two-second clock samples reported 350 or 400 MHz for both current and maximum GPU frequency, with 100% utilization.
The clock sample window overlaps the trace and extends slightly beyond it.
No frame-pacing waits appeared in the CPU trace.
QueuePresentKHR averaged 5.059 ms, including a 4.541 ms main-thread wait; the driver's GPU-completion wait averaged 21.197 ms on its separate thread.
These nested/overlapping driver waits are not isolated GPU execution timings.
Together with the saturated reduced-clock GPU, they point to GPU throughput as the immediate bottleneck, rather than the 60 FPS limiter.
CPU work also slowed: animation averaged 6.233 ms, recording 5.372 ms and simulation 3.804 ms of inclusive wall time.

The earlier six-minute near-60-FPS observation did not hold for the longer session.
No new per-pass GPU capture was obtained because the bounded profiler had already completed; the early pass timings remain early-only evidence.
This trace reported no nonzero capture-quality errors, and the collected process logcat contained no fatal-signal, fatal-exception, Vulkan-error or abort-message matches.
Artifacts are in `build/performance/cooled-1024-sustained/late/`.
The game was force-stopped after capture so the device could cool; settings, saves and assets were preserved.
Sustained 60 FPS remains unmet and needs reduced rendering workload and further hot-state verification, not repeated short cooled baselines.
