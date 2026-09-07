# Android follow-up work

## Separate fog profiling measurements

Status: planned, not implemented.

- Split the combined `Fog-LUTs` GPU marker into lighting-volume generation and sunshaft occlusion measurements.
  Keep the final fog-compositing measurement separate.
- Compare `fogHalfResolution=0` and `1` in the same saved scene and camera position, recording GPU clocks and power/charging state.
  Preserve the combined total for comparisons with older captures.
- Check stationary and moving image quality near the sun, horizon and shadowed vegetation before changing defaults.
  Include both sunshaft settings and the second wooded-path/shrine test location.
- Distinguish short instrumented captures from longer normal unplugged play when assessing sustained FPS.

## One-command private test APK with game files

Status: planned, not implemented.

- Provide a Windows `.bat` entry point and a Linux `.sh` entry point.
  Test the Linux workflow on Arch Linux under WSL as well as documenting native Linux usage.
- Detect compatible installed build prerequisites and install missing dependencies from official sources.
  Pin tool versions, verify downloads, and ask before license acceptance or privileged system changes.
  Prefer a local tool cache where possible and support rerunning an interrupted setup.
- Discover legally owned Gothic II: Night of the Raven installations in Steam libraries, GOG locations, and common game directories such as `C:\Games` and `D:\Games`.
  Include custom Steam library paths and Windows game directories accessible from WSL.
  Allow an explicit game-data path and ask which installation to use if several are found.
  Validate the selected installation without modifying its files.
- Add an explicit opt-in private packaging mode that includes the required game assets in a single signed ARM64 debug APK.
  Keep the normal distributable APK asset-free.
  Exclude saves, personal configuration and unrelated files unless explicitly selected.
- Validate feasibility with the approximately 3.5 GB installation before settling on the packaging layout.
  Check APK/ZIP and signing-tool limits, Android installation behavior, compression, and temporary/free-space requirements on the phone.
  If assets must be extracted into app-accessible storage, show progress, verify integrity, recover from interrupted extraction, and preserve existing saves/settings during updates.
- Produce an APK that can be transferred through a local network or file-sharing service and installed through Android's package installer without USB or ADB.
  Document Android's installation prompts and reuse the local signing key so subsequent test APKs can update the app.
- Keep proprietary assets, bundled APKs and signing keys out of Git and public build artifacts.
  Document that asset bundling is for private testing on the owner's devices, not redistribution.
- Verify the full workflow on the S24 and S23 Ultra: clean installation, first launch, asset availability, and an update that preserves existing data.

This is a future convenience workflow; the current Gradle build and separate asset-copy workflow remain supported.
