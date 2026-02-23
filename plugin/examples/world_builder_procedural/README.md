# World Builder Procedural Plugin (Example)

This directory provides a reference implementation of a God Client plugin using the modernised plugin ABI. The plugin does not
ship production features; instead it demonstrates how brush presets, collaborative state, and lifecycle hooks can be wired up.

## Building

1. Configure the project with CMake (see the root `CMakeLists.txt`).
2. Build the `WorldBuilderProceduralPlugin` target for your platform.
3. Drop the produced binary next to `plugin.json` so that the God Client can discover it at runtime.

## Runtime behaviour

* Logs a message when loaded to confirm the host dispatch table works.
* Precomputes a simple cosine falloff curve that future terrain brushes can consume.
* Exposes stub callbacks for per-frame updates and shutdown.

Use this plugin as a starting point for the features outlined in the modernization roadmap.
