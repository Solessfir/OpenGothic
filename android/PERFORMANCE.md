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
