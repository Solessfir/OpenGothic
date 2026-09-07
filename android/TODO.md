# Android follow-up work

## Touch and gamepad movement responsiveness

Status: input-side responsiveness implemented; device comparison and animation-transition investigation remain pending.

- Implemented: physical walk/run thresholds shared by touch and gamepad, a gait-switch margin, separate touch dead zones,
  proportional touch turning with a configurable full speed, and input-dependent gamepad turn boost for deliberate direction changes.
  Existing locked sidestep filtering, manual walk, keyboard/mouse behavior and combat animation interruption rules are retained.

- Measure input-to-movement response on the phone, separating startup, stopping, direction reversal and locked strafing.
  Record the installed build, actual INI settings and frame rate; source inspection alone does not establish device latency.
- Compare the new defaults in the same scenes, including gentle walking, firm turns, stopping, menu transitions and locked direction reversals.
  Proportional touch turning does not imply continuously proportional forward movement speed.
- If delays remain, inspect animation-driven displacement and interruption rules for locomotion transitions.
  Preserve combat timing and avoid unrestricted animation cancellation, foot sliding or snapping.
- Compare touch and gamepad in the same situations and preserve desktop keyboard/mouse behavior.
  Device installation and hands-on validation are deferred until requested.

## Separate fog profiling measurements

Status: marker split implemented; comparison captures and quality checks remain deferred.

- Implemented: replaced the combined `Fog-LUTs` GPU marker with `Fog-lighting-volume` and `Fog-sunshaft-occlusion`.
  The final fog-compositing marker remains `Fog`; existing experimental VSM/epipolar markers are unchanged.
  For the standard volumetric HQ path, add the two new measurements to compare with older `Fog-LUTs` captures.
- Compare `fogHalfResolution=0` and `1` in the same saved scene and camera position, recording GPU clocks and power/charging state.
- Check stationary and moving image quality near the sun, horizon and shadowed vegetation before changing defaults.
  Include both sunshaft settings and the second wooded-path/shrine test location.
- Distinguish short instrumented captures from longer normal unplugged play when assessing sustained FPS.

## One-command private test APK with game files

Status: guided setup and private packaging implemented; S24 update/startup verified. Clean-install and S23 verification remain pending.
See [PRIVATE-SETUP.md](PRIVATE-SETUP.md) for commands and safety details.

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

Implemented: Windows `.bat`/PowerShell and Linux `.sh` entry points, optional portable dependencies, installation discovery,
safe preference/save imports, signed private APK packaging, separate ZIP import through Android's document picker,
and optional ADB installation/save backup. The normal Gradle build and separate asset-copy workflow remain supported.

Verified locally: Windows native/APK build and lint, single-APK packaging of the real installation using internal 128 MiB chunks,
full host extraction and checksums, an asset-free rebuild after private packaging, and setup/extractor checks on Windows and Arch WSL.
A single >2 GiB asset failed with `Required array size too large`;
chunking avoids that per-asset Gradle limit. Fresh debug APK packaging also avoids incremental offset overflow and removes orphaned private bytes.
Verified on the S24: installed the 2.46 GB private APK using non-streaming ADB installation, completed asset verification,
and observed gameplay. All four existing saves and both writable INIs retained their original SHA-256 hashes.
Full clean Linux native builds, fresh-OS package-manager bootstrap, clean-device extraction and S23 installation remain to be exercised.
