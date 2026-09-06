# Worker completion regression

This test executes the production worker pool across 11,000 batches.
It checks empty/single/multiple task batches, parallel loops around chunk-size boundaries, delayed completion, exactly-once execution and visibility of non-atomic results after completion.
Process exit also exercises worker shutdown and joining.

```powershell
cmake -S tests/workers -B build/worker-tests
cmake --build build/worker-tests --config Release
ctest --test-dir build/worker-tests -C Release --output-on-failure
```

For Android, cross-compile with the NDK and run the executable through ADB.
Use your installed SDK/NDK paths and choose a device serial if several devices are attached:

```powershell
$ndk = "$env:ANDROID_HOME/ndk/27.0.12077973"
$cmakeBin = "$env:ANDROID_HOME/cmake/3.22.1/bin"
& "$cmakeBin/cmake.exe" -S tests/workers -B build/android-worker-tests -G Ninja "-DCMAKE_MAKE_PROGRAM=$cmakeBin/ninja.exe" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
& "$cmakeBin/cmake.exe" --build build/android-worker-tests
$adb = "$env:ANDROID_HOME/platform-tools/adb.exe"
& $adb push build/android-worker-tests/worker-tests /data/local/tmp/opengothic-worker-tests
& $adb shell chmod 700 /data/local/tmp/opengothic-worker-tests
& $adb shell /data/local/tmp/opengothic-worker-tests
```

Passing these tests establishes basic synchronization correctness, not frame-time improvements or immunity to every possible concurrency bug.
