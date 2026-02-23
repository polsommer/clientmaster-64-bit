# Terrain Editor Modernization Plan

This document describes a staged roadmap for bringing the legacy **TerrainEditor**
application up to date while keeping it useful for creating worlds for *Star Wars
Galaxies*.

The code that lives under `src/engine/client/application/TerrainEditor/src/win32`
implements a classic MFC desktop application.  The project still compiles
against an ancient tool-chain and depends on custom UI widgets that are not
portable outside of Windows.  Because of the tight coupling with the game data
format we cannot simply replace the editor overnight; instead, we modernise it
incrementally.

## High level goals

1. **Stabilise the existing feature set.**
   - Audit every `Form*` editor dialog and the helper classes for crashes,
     blocking modal loops and invalid pointer access.
   - Add unit tests around the `TerrainGeneratorHelper*` layer that can be
     executed without the UI to catch regressions early.

2. **Modern user experience.**
   - Replace the MFC document/view frame with an immediate-mode docking layout
     (Qt or Dear ImGui) so panes such as *Layers*, *Properties*, *3D View* and
     *Console* can be rearranged.
   - Introduce keyboard shortcuts, search/filter boxes and undo/redo history that
     spans all editors.

3. **Better rendering preview.**
   - Upgrade the 3D viewport to DirectX 11 or Vulkan via the existing
     `clientGraphics` library, allowing PBR materials, real-time shadows and
     toggles for collision, flora and shader masks.
   - Implement GPU-driven terrain tessellation so large height maps can be
     inspected smoothly.

4. **Workflow improvements.**
   - Implement an autosave/auto-restore layer so that in-progress planets are not
     lost when the editor crashes.
   - Surface validation and lint warnings inline (e.g. missing shader families,
     invalid radial falloff curves).

5. **Cross platform support.**
   - Move project files to CMake presets to make the editor compile on Windows
     and Linux using the same build scripts.
   - Migrate resource handling away from `.rc` only assets; use Qt Designer or
     JSON layout descriptions.

## Modernisation phases

### Phase 1 – Code health and build system

* Extract the legacy Visual Studio project settings into a CMake target.
* Add continuous integration (GitHub Actions) that at least performs a release
  build on Windows.
* Introduce clang-format/clang-tidy configurations shared with the rest of the
  engine.

### Phase 2 – Engine abstraction clean-up

* Wrap the terrain generation helper classes (`TerrainGeneratorHelper*`) in
  plain data-model interfaces.
* Replace raw pointers in form classes with smart pointers to avoid ownership
  confusion.
* Remove global singletons (for example the various `::install()` helpers) from
  the UI code; depend on explicit context objects instead.

### Phase 3 – User interface refresh

* Recreate the main frame using Qt Widgets with a `QMainWindow` hosting
  dockable widgets that map to the existing views (map, blend, flora, shader).
* Provide a Qt-based property editor that binds directly to the data model; this
  enables type-safe editing, validation and multi-selection.
* Port modal dialogs such as `DialogMapParameters` or `DialogFloraParameters`
  into non-blocking side panels with live previews.

### Phase 4 – Rendering and tooling

* Integrate the modern renderer (e.g. `clientGraphics` DirectX 11 backend) into
  the new viewport widget and expose render settings via an overlay.
* Add gizmo controls for moving/rotating/scaling boundaries and affectors.
* Embed a scripting console (Lua or Python) for batch terrain edits.

### Phase 5 – Quality of life polish

* Version the terrain layer format so newer editor versions remain compatible
  with the live game server.
* Provide template projects and in-app documentation for common biomes.
* Bundle a telemetry opt-in that captures crashes and anonymised workflow data
  to drive future improvements.

## Immediate next steps

1. Stand up a small prototype that loads a terrain generator file (`.trn`) and
   renders it using Qt + DirectX 11 to validate the tech stack choice.
2. Set up automated tests that parse existing sample terrains and verify that
   the helper layer produces identical output between the legacy and prototype
   editors.
3. Document the data flow between the editor, `TerrainGenerator`, `Layer`,
   `Boundary`, `Filter` and `Affector` classes to ensure future developers have a
   clear reference.

By completing the above phases we gain a modern, extensible terrain authoring
environment that still respects the bespoke data structures required by *Star
Wars Galaxies*.
