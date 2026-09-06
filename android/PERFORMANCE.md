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

The 75% setting remains enabled for testing. A 50% comparison and per-pass GPU profiling remain pending; do not infer their results from this sample.

To restore the native-scale configuration, first save and exit the game, then run from this workspace:

```powershell
& C:\Android\Sdk\platform-tools\adb.exe -s RFCX10M60QT push build/performance/s24-scale75-20260906-212213/Gothic.ini.backup /sdcard/Android/data/org.opengothic.app/files/Gothic.ini
```

This restores the entire backed-up INI, so merge later setting changes first if necessary. The S23 Ultra configuration was not changed.

## S24 50% scale experiment prepared

After the user exited, confirmed the S24 app was no longer running and backed up its 75% configuration to local `build/performance/s24-scale50-20260906-213003/Gothic.ini.backup`. Changed only `vidResIndex=1` to `2`, verified the device file and relaunched successfully (`Status: ok`). This requests 1170x540 3D rendering, one quarter of native pixel count; UI scale and all other settings remain unchanged. Before launch, live skin temperature was 42.9 C and thermal status remained 2. An in-game 50% capture is still pending; no performance result is claimed yet.

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

1. Save and exit, back up the writable INI as described in [CONFIGURATION.md](CONFIGURATION.md), then compare the same scene at `vidResIndex=1` (1755x810, 56% of native pixels) and `2` (1170x540, 25%). Preserve UI scale. Compare warm runs as well as cold ones. This is a diagnostic quality tradeoff, not yet a new default or a promise of 60 FPS. The existing scaler also disables the AA preset when leaving native resolution, so document that confound if AA is enabled.
2. Add per-pass GPU timestamps before choosing expensive renderer work to optimize. Current shadow maps are fixed at 2048; shadow resolution, reflection, AO and fog passes are candidates, not established bottlenecks.
3. Measure the CPU contribution of world/animation updates, command recording and frame pacing. Source inspection found world/animation updates before the nonblocking frame-fence check and a five-millisecond busy-spin tail in Tempest's sleep implementation. Neither is proven to explain this trace; optimize and A/B test rather than assuming.
4. Rebuild Android and Windows, run regression tests and repeat this scene after each change. Keep Android-specific backend changes in Tempest. Target sustained 60 FPS, not a cold menu reading.

No renderer or gameplay changes were made in this measurement pass, and no new APK is needed for the capture tools.
