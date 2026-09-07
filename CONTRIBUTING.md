# Contributing

Keep changes focused and include a clear description of the problem and relevant verification.
Use issues for bugs and feature proposals; include reproduction steps and reviewed logs.
Do not upload copyrighted game assets, private signing keys or unreviewed system traces.

## Project layout

- `common/`: game logic, UI, input and rendering code.
- `shader/`: GLSL shaders.
- `lib/Tempest/`: platform and graphics engine submodule.
- `lib/ZenKit/`: Gothic asset and script support.
- `lib/bullet3/`: physics.
- `lib/dmusic/` and `lib/TinySoundFont/`: music support.
- `android/`: Android application, build/setup tooling and user guides.
- `tests/`: focused regression suites.

Keep platform backend changes in Tempest where appropriate.
Preserve unrelated work and update submodule pointers only to available commits.

## Verification

Follow the [build instructions](README.md#build-instructions) for the affected platform.
The [Android contributor guide](android/tools/README.md) covers APK checks, input/import tests and profiling.
[Rendering](tests/rendering/README.md) and [worker](tests/workers/README.md) tests have separate setup instructions.

Keep machine-specific measurements and investigation notes in ignored `build/` or `*.local.md` files.
Public documentation should explain current behavior and reproducible commands, not individual testing sessions.

## References

- [Feature list](https://github.com/Try/OpenGothic/wiki/Feature-list)
- [Daedalus script API coverage](https://github.com/Try/OpenGothic/wiki/Daedalus-scripts)

