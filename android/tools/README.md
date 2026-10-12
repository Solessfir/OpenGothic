# Android contributor checks

Player instructions are in the [Android guide](../README.md).
Keep captures, device-specific reports and game-data packages under ignored `build/`, not in tracked documentation.
Review logs/traces before sharing: they may contain personal paths, device identifiers and unrelated app activity.

## Build and tests

From the repository root:

```powershell
cmake -S android -B build/android -G Ninja
cmake --build build/android --target OpenGothic-apk
cmake -S tests/controller -B build/controller-tests
cmake --build build/controller-tests --config Release
ctest --test-dir build/controller-tests -C Release --output-on-failure
python -m unittest discover -s android/tools/tests -v
```

Set `JAVA_HOME` to enable the host Java importer tests. On Linux use `python3`.
Add `-DTEMPEST_ANDROID_BUILD_TYPE=Debug` when configuring to check the debug variant.
Separate suites cover [rendering](../../tests/rendering/README.md) and [worker completion](../../tests/workers/README.md).
Compilation/tests do not replace device checks for controls, audio, lifecycle, imports and save/load.

## Inspect the APK

```powershell
$apk = 'build/android/OpenGothic/app/build/outputs/apk/release/app-release.apk'
& "$env:ANDROID_HOME/build-tools/35.0.0/apksigner.bat" verify --verbose $apk
& "$env:ANDROID_HOME/build-tools/35.0.0/aapt2.exe" dump badging $apk
& "$env:JAVA_HOME/bin/jar.exe" tf $apk
& "$env:ANDROID_HOME/build-tools/35.0.0/zipalign.exe" -c -P 16 -v 4 $apk
```

Expect only `arm64-v8a` native libraries. A normal asset-free build must not contain `assets/private-game-*`,
including after a bundled build. Never upload game assets, signing keys or personal test data.

Release must not report `application-debuggable`. Native compile/link commands under
`build/android/OpenGothic/app/.cxx/Release/` should use `-O3`, `-DNDEBUG` and `-flto=thin`.
Keep `build/android/OpenGothic/app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip` and
`build/android/OpenGothic/app/build/outputs/mapping/release/mapping.txt` with each release for crash analysis.
The setup scripts copy them beside the APK. Debug symbols are stripped from the packaged native library.

## GPU timings

With the game closed, set writable `Gothic.ini` `[DEBUG] gpuProfile=1`, restart and load a scene.
The capture records up to 600 completed frames into `gpu-profile.csv`; another run replaces it.

```powershell
$adb = "$env:ANDROID_HOME/platform-tools/adb.exe"
$device = 'YOUR_DEVICE_SERIAL_OR_IP:PORT'
New-Item -ItemType Directory -Force build/performance | Out-Null
& $adb -s $device pull /sdcard/Android/data/org.opengothic.gothic2notr/files/gpu-profile.csv build/performance/gpu-profile.csv
./android/tools/Summarize-GpuProfile.ps1 -Path build/performance/gpu-profile.csv
```

Results include GPU stalls/overlap, not isolated shader cost. Compare the same scene and settings.
Set `gpuProfile=0` afterward; profiling adds overhead.

## CPU / presentation traces

Set `[DEBUG] cpuProfile=1`, restart, load a scene and leave the camera still.
Using the ADB variables above:

```powershell
& $adb -s $device push android/tools/performance.pbtxt /data/misc/perfetto-configs/opengothic-performance.pbtxt
& $adb -s $device shell perfetto --txt -c /data/misc/perfetto-configs/opengothic-performance.pbtxt -o /data/misc/perfetto-traces/opengothic.perfetto-trace
& $adb -s $device pull /data/misc/perfetto-traces/opengothic.perfetto-trace build/performance/capture.perfetto-trace
```

Recording takes 30 seconds. Analyze with the official [Perfetto Trace Processor](https://perfetto.dev/docs/analysis/trace-processor),
using `performance.sql` and `cpu-performance.sql` in this directory.
For an executable named `trace_processor` on PATH:

```sh
trace_processor query -f android/tools/performance.sql build/performance/capture.perfetto-trace
trace_processor query -f android/tools/cpu-performance.sql build/performance/capture.perfetto-trace
```

CPU scopes are nested; do not add their inclusive durations. Missing markers do not mean zero cost.
Record resolution, quality settings, power/thermal conditions and source revision alongside local captures.
Disable `cpuProfile` afterward. Never infer sustained gameplay FPS from configuration or a short cold run.
