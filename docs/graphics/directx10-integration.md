# DirectX 10 Integration Blueprint

The legacy God Client is tightly coupled to Direct3D 9.  Moving to Direct3D 10 requires
introducing a graphics backend abstraction, standing up a D3D10 implementation, and
migrating the higher level rendering systems to the new API.  The following blueprint
captures the concrete work that must land before the D3D10 renderer can ship.  The
foundation is now in place: a dedicated `Direct3d10` project participates in the Visual
Studio solution and CMake build, and the runtime bootstrapper actively validates the
hardware feature set before switching to the new renderer.

## 1. Goals

* Preserve feature parity with the existing Direct3D 9 renderer (materials, post
  processing, dynamic buffers, hardware cursor support).
* Continue supporting the legacy D3D9 path as a fallback during the migration window.
* Minimise churn in higher level systems (`RenderWorld`, `ShaderPrimitiveSorter`, UI)
  by hiding the backend differences behind a common interface.

## 2. High-Level Phases

1. **Backend Abstraction Layer**
   * Define a `GraphicsBackend` interface capturing the operations currently provided by
     `Direct3d9` (device creation, render-target management, shader binds, etc.).
   * Refactor the D3D9 implementation to sit behind this interface without changing
     behaviour.
2. **D3D10 Bootstrap**
   * Add a D3D10 bootstrapper that initialises a DXGI swap chain and `ID3D10Device1`.
   * Implement resource managers for textures, vertex/index buffers, shaders, and
     constant buffers using the D3D10 API.
   * Mirror the fixed-function emulation currently provided by the D3D9 shaders.
3. **Feature Parity Pass**
   * Port the shader library to HLSL 4.0, reimplement the render-state macros, and
     audit all render passes (terrain, characters, post-processing, UI).
   * Validate render-target operations (MSAA resolve, MRT usage, shadow-map depth
     formats) under the new API.
4. **Stabilisation**
   * Add regression coverage: automated frame captures, GPU metrics parity checks, and
     smoke tests for editor workflows.
   * Ship both backends behind a runtime toggle until confidence is reached.

## 3. Immediate Action Items

| Area | Tasks |
| --- | --- |
| Abstraction | Extract a backend-neutral command surface from `Graphics` and friends,
| | introduce thin façade classes so existing call sites remain stable. |
| Bootstrap | Stand up device creation, swap chain handling, and basic render loop for
| | D3D10 in a parallel module (`Direct3d10`). |
| Resources | Implement buffer and texture loaders with DXGI formats mirroring the D3D9
| | equivalents, including sRGB handling and mip streaming hooks. |
| Shaders | Convert shader build pipeline to emit both shader-model 3.0 (D3D9) and 4.0
| | (D3D10) binaries. Document validation process. |

## 4. Integration Artifacts

* **Runtime probing:** `Direct3d10Bootstrap` now performs full module loading, device
  creation, and capability validation so the engine can surface actionable diagnostics
  when Direct3D 10 features are unavailable.
* **Swap-chain validation:** The bootstrapper now spins up a DXGI swap chain, render
  target view, and buffer allocations to exercise the D3D10 resource managers instead
  of merely creating a device.  Shader-model 4.0 readiness is captured alongside
  feature-level detection so failures can be reported early.
* **AMD/heterogeneous GPU detection:** the bootstrap enumerates every DXGI adapter,
  records vendor identifiers, and selects the best hardware device instead of always
  probing adapter 0.  AMD Radeon boards—especially in mixed Intel/AMD laptops—are now
  recognised and reported correctly, eliminating the need for WARP fallbacks on those
  systems.
* **Hard-coding surface:** `Direct3d10` mirrors the legacy `Direct3d9` entry point with
  lightweight scaffolding so systems can be temporarily wired against the new backend
  while feature work lands.  The helper exposes the latest probe results and
  human-readable diagnostics for crash reports and editor tooling.
* **Hybrid runtime toggle:** The legacy renderer exposes a new `Direct3d9.preferDirect3d10`
  configuration key that requests a Direct3D 10 compatibility shim.  When enabled the
  engine will favour the Direct3D 9Ex device path and advertise the active runtime via
  the crash reporter, allowing QA to confirm whether the shim is in effect during game
  sessions.
* **Visual Studio project:** `Direct3d10.vcxproj` is part of `swg.sln`, raising the
  solution inventory to 131 projects and allowing engineers to build the backend in the
  classic workflow.
* **CMake target:** the modern toolchain exposes `swg_direct3d10_bootstrap`, enabling
  continuous integration and headless builds to exercise the D3D10 runtime checks.

## 5. Compatibility Notes

* The existing Bink, SpeedTree, and ImGui integrations depend on D3D9-specific hooks.
  Each system will require targeted updates or an interop layer when running under
  D3D10.
* The legacy asset pipeline assumes D3D9 texture formats.  When adding new DXGI
  formats, update the exporters to ensure identical results on both paths.
* Ensure the build system bundles the June 2010 DirectX SDK redistributables so the
  D3D10 runtime is available on developer machines.

## 6. Tracking

Create a tracking epic that captures the tasks above.  During implementation, gate the
new backend behind a config option (`ClientGraphics.UseD3D10`) so QA can opt-in while
regular users stay on the proven D3D9 pipeline.  The flag will attempt to load the
Direct3D 10 graphics backend first and automatically fall back to the legacy path when
the swap-chain/bootstrap validation fails.

## 7. Feature Parity & Known Issues

* **Render passes:** Terrain, character, post-processing, and UI passes share the same
  validation harness across D3D9 and D3D10, but vendor-specific shader macros still
  need to be audited under SM4.0.  The current bootstrap proves the swap chain and
  buffer allocations compile but does not yet guarantee visual parity.
* **Interop gaps:** Bink, SpeedTree, and ImGui are still bound to the D3D9 state
  helpers.  They run on the fallback renderer; integrating them with the D3D10 backend
  will be handled as follow-on work.
* **Runtime toggles:** `ClientGraphics.UseD3D10` can be switched at runtime via
  configuration.  When set on hardware without D3D10 support the engine will log the
  probe errors and continue with D3D9 without terminating the client.

---

This document should be updated as milestones land to reflect the current state of the
migration.  Once the D3D10 backend reaches feature parity the fallback can be removed
in a major release.
