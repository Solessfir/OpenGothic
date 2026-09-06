# Android performance measurements

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
