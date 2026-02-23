# SWG God Client Modernization Plan

This document outlines a proposed path for modernizing the SWG God Client tooling so that it is easier to maintain, extend, and use for new Star Wars Galaxies content development. The goal is to provide a pragmatic roadmap that can be executed incrementally while preserving the proven functionality of the existing client.

## 1. Architectural Goals

1. **Cross-Version Build Support**  
   * Introduce a CMake-based build alongside the legacy Visual Studio 2013 solutions.  
   * Enable builds on newer Microsoft toolchains (Visual Studio 2022) and prepare for compatibility with clang/LLVM.
2. **Modular Plugin System**  
   * Define a lightweight plugin interface so gameplay, UI, and data editors can be composed at runtime.  
   * Ship a core runtime (`swggodcore.dll`) that exposes a stable API and allows optional gameplay/editor plugins to be loaded dynamically.  
   * Document the API surface and provide example plugins.
3. **Data-Driven Editing**  
   * Replace many hard-coded tables with JSON5 or YAML descriptors so designers can iterate without recompiling.  
   * Introduce schema validation using JSON Schema to ensure data correctness at authoring time.
4. **Scripting Integration**  
   * Embed Lua 5.4 (or AngelScript, depending on compatibility) for rapid tool automation.  
   * Provide bindings that expose world editing, object spawning, and quest authoring functionality to scripts.
5. **Automated Testing & CI**  
   * Add unit tests for the most critical subsystems (resource loading, object templates, mission scripts).  
   * Integrate with GitHub Actions to build core tools on every pull request and run lint/static analysis jobs.

## 2. Immediate Action Items

| Priority | Task | Description | Owner |
| --- | --- | --- | --- |
| P0 | Inventory Legacy Projects | Audit every project in `src/build/win32/swg.sln` and classify it as runtime, tool, or dependency. | Engineering |
| P0 | Establish Modern Toolchain | Add a `cmake/` folder containing toolchain files and start with a small target (e.g., `ClientGame`). | Engineering |
| P1 | Define Plugin ABI | Create a header (`src/engine/shared/plugin/PluginAPI.h`) that encapsulates lifecycle hooks and versioning. | Engineering |
| P1 | Authoring UX Research | Interview designers/admins to understand current workflows and pain points to prioritize new UI features. | Design |
| P2 | Lua Sandbox Prototype | Integrate Lua with a single command (e.g., mass spawning) and expose it via a new "Scripting" panel. | Tools Team |

## 3. New Feature Concepts

The modernization initiative introduces a refreshed tool suite spanning world building, data authoring, mission design, and live operations support.

### 3.1 World Builder 2.0
* **Layered Editing**: Toggle terrain, pathing, navmesh, and mission objects in separate layers for clarity.  
* **Procedural Brush System**: Define brush presets in JSON to sculpt terrain, paint biomes, and place props with scatter rules.  
* **Collaborative Sessions**: Optional server component that syncs edits in real time for multiple designers.

### 3.2 Object Template Studio
* **Template Graph View**: Visualize inheritance relationships between object templates to quickly diagnose overrides.  
* **Live Preview**: Embed a runtime preview widget that instantiates objects with selected appearance and animation sets.  
* **Validation Rules**: Configurable lint rules ensure template fields match server expectations before publishing.

### 3.3 Mission & Quest Editor
* **Node-Based Flow**: Use a visual scripting interface to define quest states, triggers, and branching outcomes.  
* **Server Sync**: Export quest graphs to a canonical JSON representation that can be consumed by the server pipeline.  
* **Debug Replay**: Record and replay quest progression for bug reproduction.

### 3.4 Live Operations Dashboard
* **Shard Health Overview**: Display latency, population, and error metrics pulled from Prometheus or InfluxDB.  
* **Event Scheduler**: Plan and deploy in-game events with approval workflows and rollback support.  
* **Patch Pipeline Integration**: Track asset diffs and push builds to staging/production from a single interface.

## 4. Modern UI Foundations

1. **Technology Choice**: Adopt Qt 6 (with QML) or Dear ImGui for rapid iteration, depending on whether a native desktop or in-engine overlay is desired.  
2. **Theming & Accessibility**: Provide dark/light themes, high-contrast modes, and keyboard navigation shortcuts.  
3. **Dockable Panels**: Allow every major tool (Object Browser, Terrain Editor, Script Console) to be undocked or rearranged.

## 5. Workflow Automation Enhancements

* **Content Packs**: Define content bundles (`.swgpack`) that capture assets, scripts, and data with manifest metadata for distribution.  
* **Command-Line Tooling**: Provide `swg-tool` CLI with subcommands to validate data, generate navmesh, and publish builds.  
* **Continuous Publishing**: Automate copying validated content into the `publish/` directory and increment version numbers via scripts.

## 6. Quality & Maintenance

* **Static Analysis**: Enable clang-tidy and CppCoreGuidelines checks during CI builds.  
* **Code Style Guide**: Adopt a documented coding standard (e.g., based on LLVM/Google style) and enforce via clang-format.  
* **Dependency Updates**: Track third-party libraries and upgrade to maintained versions (e.g., zlib, SDL, DirectX SDK replacements).  
* **Documentation Portal**: Publish docs to an internal site generated with MkDocs, including tutorials and API references.

## 7. Adoption Strategy

1. Start with the build system modernization to remove dependency on legacy Visual Studio versions.
2. Incrementally refactor subsystems into modular libraries, ensuring the legacy client still compiles during the transition.
3. Deliver new tooling features (World Builder 2.0, Object Template Studio) as separate plugins, validating the plugin architecture.
4. Roll out automated tests and CI to maintain confidence as the codebase evolves.
5. Gather feedback after each milestone and adjust priorities accordingly.

## 8. Implementation Kickstart

To jump-start the modernization process this repository now includes:

* **CMake project scaffolding** (`CMakeLists.txt`, `cmake/README.md` and toolchain presets) that builds the shared plugin API and example plugin.
* **Plugin ABI header** (`src/engine/shared/plugin/PluginAPI.h`) defining the lifecycle, logging, and service dispatch structures required for runtime extensibility.
* **Example plugin implementation** (`plugin/examples/world_builder_procedural/`) that exercises the ABI, logs lifecycle events, and prepares procedural brush falloff data.
* **`swg-tool` automation CLI** (`tools/swg-tool/`) with subcommands for manifest validation, navmesh prototyping, and publishing versioned content bundles.

These artifacts provide reference implementations that teams can extend while developing the full feature roadmap above.

## 9. Appendix: Example Plugin Manifest

```jsonc
{
  "name": "WorldBuilderProcedural",
  "version": "0.1.0",
  "entryPoint": "WorldBuilderProceduralPlugin",
  "compatibleApi": [1],
  "description": "Adds procedural placement brushes and layer-aware editing tools.",
  "capabilities": [
    "terrain-brushes",
    "biome-scatter",
    "collaborative-editing"
  ],
  "dependencies": {
    "core": ">=1.0.0",
    "terrain": ">=2.0.0"
  }
}
```

This manifest illustrates how a plugin could declare its metadata and required capabilities. The runtime would parse this file, validate compatibility, and dynamically load the plugin DLL/shared library to extend the God Client.

---

By following this plan the team can progressively evolve the SWG God Client into a modern, extensible toolkit for building and maintaining Star Wars Galaxies content.
