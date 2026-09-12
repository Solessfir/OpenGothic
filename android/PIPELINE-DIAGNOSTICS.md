# Adreno pipeline capture

This diagnostic branch is not a crash fix or a performance build. It keeps the normal shaders and graphics settings, records actual Vulkan pipeline creation calls after cache lookup, and flushes each record before entering the driver. Graphics and compute creation share one mutex by default. Each process gets a separate capture directory and matching SPIR-V files.

The app installs separately as `org.opengothic.gothic2notr.diagnostics`. Do not uninstall your regular game. Its saves and settings are not changed. The APK contains no Gothic assets.

## Install and prepare

Use the supplied APK, with `adb` available in your terminal:

```powershell
adb install -r OpenGothic-Adreno-Diagnostics.apk
adb shell am start -n org.opengothic.gothic2notr.diagnostics/org.opengothic.app.SetupActivity
```

Import your own game archive in the diagnostic app. Alternatively, close it and copy the already installed game files on the phone. This needs room for a second copy:

```powershell
adb shell am force-stop org.opengothic.gothic2notr.diagnostics
adb shell cp -R /sdcard/Android/data/org.opengothic.gothic2notr/files/Gothic2 /sdcard/Android/data/org.opengothic.gothic2notr.diagnostics/files/
```

For a load-game reproduction, copy the affected save too, replacing `save_slot_1.sav` with its actual filename if needed:

```powershell
adb shell cp /sdcard/Android/data/org.opengothic.gothic2notr/files/save_slot_1.sav /sdcard/Android/data/org.opengothic.gothic2notr.diagnostics/files/save_slot_1.sav
```

## Capture one attempt

Run the helper from PowerShell. Add `-Adb 'path/to/adb.exe'` or `-Device 'IP:port'` when needed. It records a start time, configures only the diagnostic app, and starts it. Reproduce the crash, then collect without reopening the app:

```powershell
.\capture-adreno.ps1 Start
.\capture-adreno.ps1 Collect
```

Send the resulting `adreno-results-*.zip`. It contains capture records, shader dumps and logs, not game files or saves. Inspect logs before sharing. The helper does not clear the phone's existing log buffers or delete previous captures.

If the serialized run succeeds, compare a parallel run:

```powershell
.\capture-adreno.ps1 Start -Parallel
.\capture-adreno.ps1 Collect
```

Only if requested, repeat with `Start -NoValidation` to check whether validation changes the failure. The official Khronos validation layer is bundled and requested by default. Confirm `enabling layer VK_LAYER_KHRONOS_validation` in the log rather than assuming it loaded. Validation can reduce performance and alter timing.

## Reading the result

In `events.txt`, a flushed `BEFORE N` without `AFTER N` identifies an in-flight Vulkan call, not a cache lookup. Match its `SHADER file=` entries to the SPIR-V files in that same directory. A missing return does not alone prove that shader caused the crash; correlate it with the crash stack and thread. Parallel mode can have several in-flight calls.

Records contain attachment formats, fixed/dynamic graphics state and reflected binding information. They are diagnostic evidence, not a complete replay capture of all Vulkan objects. Keep the exact APK's unstripped library for symbolication; another build's symbols are not interchangeable.

## Rebuild

Check out `diagnose-adreno-pipelines`, initialize its submodules, then use the normal Android prerequisites:

```sh
cmake -S android -B build/adreno-diagnostics -G Ninja -DTEMPEST_ANDROID_BUILD_TYPE=Debug -DOPENGOTHIC_ANDROID_GAME=gothic2notr
cmake --build build/adreno-diagnostics --target OpenGothic-apk
```

Output: `build/adreno-diagnostics/OpenGothic/app/build/outputs/apk/debug/app-debug.apk`. Native code uses RelWithDebInfo. Packaging downloads the pinned Khronos Android validation layer 1.4.357.0 with SHA-256 verification; only ARM64 is included. Binaries remain in the build directory, not Git.

Validation packaging follows the [Android NDK guide](https://developer.android.com/ndk/guides/graphics/validation-layer).
