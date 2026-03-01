# Win64 client build steps

This repository ships the legacy client toolchain now standardized on 64-bit outputs through the Visual Studio 2013 solution at `src/build/win64/swg.sln`.
Use the steps below when you need runtime-ready Direct3D 9 renderer DLLs and optional DXVK-enabled packaging.

> **Note:** Some repository paths and helper script names still use `win32` for legacy layout compatibility (for example `tools/win32/` and portions of `src/build/win32`). Build the Visual Studio solution and project matrix using **x64** platform configurations for the current client outputs.

## Prerequisites

- Visual Studio 2013 with the v120 toolset installed.
- A Win64 build environment (32-bit packaging has been removed).
- For DXVK packaging, place the pinned runtime file at:
  - `src/external/3rd/dxvk/2.4/x64/d3d9.dll`
  - Version metadata is tracked in `src/external/3rd/dxvk/2.4/DXVK_VERSION.json`.

## Visual Studio workflow

1. Open `src/build/win64/swg.sln`.
2. Set **Configuration** to **Release** (or **Optimized**/**Debug** as needed).
3. Set **Platform** to **x64**.
4. Build the following renderer projects:
   - `Direct3d9`
   - `Direct3d9_ffp`
   - `Direct3d9_vsps`

The Direct3D 9Ex helper (`Direct3d9ExSupport.cpp/.h`) is compiled directly into the renderer targets above, so no separate helper library is required.
The resulting renderer DLLs land under `src/compile/win64/Direct3d9*/<Configuration>/`.

## Command-line build (msbuild)

From the repository root:

```bat
msbuild src/build/win64/swg.sln /p:Configuration=Release /p:Platform=x64 /t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps
```

Swap `Release` for `Optimized` or `Debug` as needed.

## Packaging renderers for client output directories

Use the packaging helper to copy built renderer DLLs into client output directories and select runtime mode.

### Native D3D9 distribution (default)

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Configuration Release -Runtime native
```

This copies renderer DLLs from:

- `src/compile/win64/Direct3d9/Release/*.dll`
- `src/compile/win64/Direct3d9_ffp/Release/*.dll`
- `src/compile/win64/Direct3d9_vsps/Release/*.dll`

into default client output directories:

- `src/compile/win64/SwgClient/Release/`
- `src/compile/win64/SwgClientSetup/Release/`

and removes any local `d3d9.dll` override so Windows uses the native system runtime.

### DXVK-enabled distribution (opt-in)

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Configuration Release -Runtime dxvk -FailIfDxvkMissing
```

This performs the same renderer DLL copies as native packaging, then also copies:

- `src/external/3rd/dxvk/2.4/x64/d3d9.dll`

into all packaging targets (renderer output directories plus client output directories), so deployment folders contain a local DXVK `d3d9.dll` beside the renderer outputs.

### Custom client output targets

Use `-ClientOutputDirs` to override deployment destinations:

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Configuration Release -Runtime dxvk -ClientOutputDirs C:\staging\client_r,C:\staging\launcher
```

## Deployment expectations and rollback

- **Native distribution**: do not ship a local `d3d9.dll`; runtime resolves to the OS-provided Direct3D 9 implementation.
- **DXVK distribution**: ship `d3d9.dll` in the same directory as `SwgClient_r.exe` (and any equivalent launcher/client runtime folder).
- **Rollback to native runtime**: remove the local `d3d9.dll` from deployment directories, or rerun packaging with `-Runtime native` to clean output targets.


## Configuration semantics (x64)

- **Debug**: Uses debug macros/CRT for full development-time diagnostics.
- **Optimized**: Uses non-debug CRT and `NDEBUG` like Release, while retaining project-specific diagnostic defines (for example `DEBUG_LEVEL=1` in Direct3D9 renderer projects).
- **Release**: Uses non-debug CRT and `NDEBUG` with production-oriented diagnostic levels/settings.

This keeps Optimized builds diagnostically useful without mixing debug CRT (`/MTd`) into non-debug x64 client renderer outputs.
