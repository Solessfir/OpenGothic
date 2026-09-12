# Static world reuse tests

Build the `WorldCacheTests` target in a configured desktop build, then run the executable from its output directory. No game assets or graphics device are needed. The target is excluded from normal builds.

Checks shared terrain ownership, collision and sector names after the previous world is destroyed, cache eviction, repeated water-only worlds, empty worlds and repeated save-reader lifetimes.
