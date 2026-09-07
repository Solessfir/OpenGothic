# Rendering regression tests

## HDR output checks

The optional Vulkan suite also builds `hdr-output-tests` and runs it as `hdr-vulkan-output`.
It executes the production HDR/PQ helpers over 4,097 intensity samples.
Checks cover the unchanged default SDR tone curve, matching darker HDR values, monotonic bounded highlights, Rec.709-to-Rec.2020 conversion, ST 2084 encoding and 200-nit reference white.
GPU values are compared with double-precision CPU references, and Vulkan validation errors fail the test.
This is a shader-math test, not a substitute for checking HDR surface negotiation, UI/video rendering, display calibration or lifecycle behavior on real hardware.

## Lanczos checks

The CPU test compares the old 21-tap footprint with the paired 12-read formulation using ideal double-precision bilinear sampling.
It covers clamped borders, fractional positions, odd image dimensions, and constant, impulse, checkerboard and HDR-like inputs.

```powershell
cmake -S tests/rendering -B build/rendering-tests
cmake --build build/rendering-tests --config Release
ctest --test-dir build/rendering-tests -C Release --output-on-failure
```

The optional Vulkan test executes the actual optimized GLSL and a separate 21-tap reference on the GPU.
Both use the stable scalar Lanczos weight function, including its analytic limit near zero.
It checks RGBA8 and R11G11B10UF HDR inputs, all three color channels, odd/tiny dimensions and the S24's 1170x540 and 1755x810 to 2340x1080 upscale paths.
The four image patterns produce 40,441,744 output-pixel comparisons.
The tolerance is 0.01 of the input pattern's range to account for finite-precision hardware interpolation; this is not bit-identical filtering.
Validation errors, including errors during device destruction, fail the test.

On Windows, first build the shared Tempest library through the normal OpenGothic Windows build, then configure with its import library.
Run from the repository root and adjust the build directory if yours differs.
Use the Vulkan SDK installer's `VULKAN_SDK` environment variable, or set it to your installation path:

```powershell
# Only needed when the Vulkan SDK environment variable is not already set.
$env:VULKAN_SDK = 'C:\path\to\VulkanSDK\<version>'
$repoRoot = (Get-Location).Path
cmake -S tests/rendering -B build/rendering-tests "-DRENDERING_TEST_TEMPEST_LIBRARY=$repoRoot/build/windows-regression/lib/Release/Tempest.lib"
cmake --build build/rendering-tests --config Release
$env:PATH = "$repoRoot\build\windows-regression\opengothic\Release;$repoRoot\build\windows-regression\opengothic;$env:PATH"
$env:VK_LAYER_PATH = "$env:VULKAN_SDK\Bin"
ctest --test-dir build/rendering-tests -C Release --output-on-failure
```

The CPU test is portable; the optional GPU test requires a working Vulkan device and Tempest runtime dependencies.
Neither test establishes phone FPS or replaces an on-device scene comparison.

## SSAO convergence regression

The same optional Vulkan configuration builds `ssao-vulkan-convergence`.
It exercises the production convergence helper against the previous counter-based decision with defined shared-memory ordering.
Across 1,048,576 lane cases it checks evaluated sample counts, sample signatures, the first convergence round and uniform workgroup exit.
Inputs include zero/high differences, a single unconverged lane, exact threshold boundaries, NaN/infinity, signed zero, mixed lane sample limits and inactive lanes.
The synthetic workload reduces synchronization rounds from 8,388,608 to 6,291,456; this is not a game-performance benchmark.

The helper retains a barrier after reading the shared result, preventing the next iteration from resetting it before every lane has read it.
Workgroup exit is uniform, as required by the [GLSL barrier rules](https://docs.vulkan.org/glsl/latest/chapters/builtinfunctions.html#shader-invocation-control-functions).
This test verifies convergence control flow, not the complete SSAO image or blur filter.

```powershell
ctest --test-dir build/rendering-tests -C Release -R ssao --output-on-failure
```
