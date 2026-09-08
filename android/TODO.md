# Android TODO

## Move Android build boilerplate into Tempest

- Add a reusable CMake helper that generates the Gradle project and default manifest in the build directory.
- Keep native sources and dependencies defined in CMake, with no hand-maintained Gradle project per application.
- Keep Android platform integration in Tempest and allow application-specific resources, setup screens and manifest overrides.
- Leave desktop builds unchanged, with no Java, Android SDK or Gradle requirement. Android builds still require those tools.
- Keep project generation separate from native compilation to avoid recursive CMake/Gradle builds.
- Demonstrate APK generation with a small Tempest example, then adapt OpenGothic and prepare focused PRs without unrelated gameplay or UI changes.
