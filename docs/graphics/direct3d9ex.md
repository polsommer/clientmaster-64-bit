# Direct3D 9Ex Runtime Toggle

The legacy Star Wars Galaxies client now exposes a runtime toggle for the Direct3D 9Ex renderer. 9Ex improves windowed-mode presentation and reduces compatibility issues on modern Windows releases while staying compatible with the 32-bit client.

## Configuration Keys

Add the following entries to the `Direct3d9` section of your configuration (for example `clientdata/Direct3d9.cfg`) to control the renderer behaviour:

| Key | Default | Description |
| --- | --- | --- |
| `Direct3d9.preferDirect3d9Ex` | `true` | Attempts to load the Direct3D 9Ex runtime via `Direct3DCreate9Ex`. When unavailable the client falls back to the classic Direct3D 9 path and records the reason in the crash report log. |
| `Direct3d9.maximumFrameLatency` | `1` | Sets `IDirect3DDevice9Ex::SetMaximumFrameLatency` on successful 9Ex device creation. Values outside `1-16` are clamped to preserve stability on lower-end 32-bit hardware. |
| `Direct3d9.gpuThreadPriority` | `0` | Applies `IDirect3DDevice9Ex::SetGPUThreadPriority` after device creation. The value is clamped to the documented `-7..7` range and recorded in the crash report metadata. |
| `Direct3d9.waitForVBlankAfterPresent` | `false` | When enabled the renderer calls `IDirect3DDevice9Ex::WaitForVBlank` after a successful present to reduce tearing on windowed-mode swap chains. |
| `Direct3d9.waitForVBlankAdapter` | `-1` | Overrides the adapter index passed to `WaitForVBlank`. The default (`-1`) reuses the active device adapter. |

> ℹ️ The options above are ignored when the system does not provide the Direct3D 9Ex entry points. The client records a `Direct3D9ExFallback` entry in the crash report to aid support triage.

## Behaviour Notes

* The 9Ex path is dynamically loaded, so the 32-bit binary remains compatible with systems that only ship `d3d9.dll`.
* Frame latency is only configured when a 9Ex device is active. The classic Direct3D 9 runtime does not expose the API and remains unchanged.
* Verbose hardware logging (`SharedFoundation` → `verboseHardwareLogging`) emits additional details about the selected runtime and any fallback decisions.
* GPU thread priority and VBlank waits are skipped on systems that do not expose the Direct3D 9Ex entry points.

## Troubleshooting 9Ex fallback

* Enable `SharedFoundation.verboseHardwareLogging` to capture runtime selection and fallback decisions (see the note above about verbose hardware logging).
* Check crash report logs for the `Direct3D9ExFallback` marker to confirm the client dropped back to the classic Direct3D 9 path.
* Fallback is expected on systems without `Direct3DCreate9Ex`, especially older 32-bit Windows installs that only ship the legacy runtime.

## Runtime Queries

Use the `Graphics` facade to detect the 9Ex environment at runtime:

* `Graphics::isDirect3d9ExRuntimeAvailable()` probes the host system for the `Direct3DCreate9Ex` entry point without triggering any loader side-effects.
* `Graphics::isUsingDirect3d9Ex()` reports whether the current renderer instance booted through the Direct3D 9Ex device path.


## Native D3D9 / 9Ex / DXVK precedence

When DXVK toggles are present in `[Direct3d9]`, runtime selection follows this order:

1. `enableDxvk=1` has highest precedence and selects DXVK.
2. If `enableDxvk=0` but `preferDxvkForAmd=1`, DXVK may be auto-selected for AMD GPUs by policy.
3. If DXVK is not selected, the client uses native Direct3D 9 and attempts the 9Ex path first when `preferDirect3d9Ex=true`.
4. If 9Ex entry points are unavailable, it falls back to classic Direct3D 9.

Defaults keep existing behavior unless explicitly changed (`enableDxvk=0`, `preferDxvkForAmd=0`).


## Migration note: deprecated utility libraries

As of the 64-bit renderer dependency cleanup, the Direct3D 9 projects (`Direct3d9`, `Direct3d9_ffp`, `Direct3d9_vsps`) no longer link `dxerr9.lib` or `d3dx9.lib`.

- HRESULT diagnostics now use local `FormatMessage` formatting instead of `DXGetErrorString` helpers.
- Legacy D3DX entry points used by shader assembly/compilation and surface copy helpers are resolved dynamically at runtime from `d3dx9_4x.dll` when present.
- Support guidance: if shader compilation or texture conversion fails on a machine missing D3DX runtime DLLs, install the DirectX End-User Runtime (June 2010) or validate equivalent redistributable coverage.
