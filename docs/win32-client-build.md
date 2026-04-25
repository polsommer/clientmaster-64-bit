# Win32/Win64 client build steps

This repository ships the legacy client toolchain through the Visual Studio 2013 solutions under `src/build/win32/` and `src/build/win64/`.
Use the steps below when you need runtime-ready Direct3D 9 renderer DLLs and optional DXVK-enabled packaging for either architecture.

> **Note:** Some repository paths and helper script names still use `win32` for legacy layout compatibility (for example `tools/win32/`). Always match packaging/build architecture explicitly with `-Architecture win32` or `-Architecture win64`.

## Prerequisites

- Visual Studio 2013 with the v120 toolset installed.
- Windows 10/11 SDK (or equivalent SDK that provides `d3d9.h`, `d3d9types.h`, and core Win32 headers/libs).
- A build environment for the architecture you are packaging (`Win32` and/or `x64`).
- Optional but recommended for runtime shader/surface helper parity: install the DirectX End-User Runtime (June 2010) so `d3dx9_4x.dll` is available at runtime when the legacy helper path is exercised.
- For DXVK packaging, place pinned runtime files at:
  - `src/external/3rd/dxvk/2.4/x86/d3d9.dll` (Win32 packaging)
  - `src/external/3rd/dxvk/2.4/x64/d3d9.dll` (Win64 packaging)
  - Version metadata is tracked in `src/external/3rd/dxvk/2.4/DXVK_VERSION.json`.


### Direct3D dependency cleanup note

The renderer project files no longer link against `d3dx9.lib` or `dxerr9.lib`. Build-time dependencies are now limited to core platform SDK libraries (for example `d3d9.lib`, `dxguid.lib`, `ddraw.lib`, and Win32 system libraries already listed in each project).

## Visual Studio workflow

1. Open the matching solution:
   - `src/build/win32/swg.sln` for Win32 packaging.
   - `src/build/win64/swg.sln` for Win64 packaging.
2. Set **Configuration** to **Release** (or **Optimized**/**Debug** as needed).
3. Set **Platform** to **Win32** (for win32) or **x64** (for win64).
4. Build the following renderer projects:
   - `Direct3d9`
   - `Direct3d9_ffp`
   - `Direct3d9_vsps`

The Direct3D 9Ex helper (`Direct3d9ExSupport.cpp/.h`) is compiled directly into the renderer targets above, so no separate helper library is required.
The resulting renderer DLLs land under `src/compile/<arch>/Direct3d9*/<Configuration>/`.

## Command-line build (msbuild)

From the repository root:

```bat
msbuild src/build/win32/swg.sln /p:Configuration=Release /p:Platform=Win32 /t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps
msbuild src/build/win64/swg.sln /p:Configuration=Release /p:Platform=x64 /t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps
```

Swap `Release` for `Optimized` or `Debug` as needed.

## Packaging renderers for client output directories

Use the packaging helper to copy built renderer DLLs into client output directories and select runtime mode.

### Example A: Win32 native packaging

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Architecture win32 -Configuration Release -Runtime native
```

Expected Win32 source and default destination paths:

- `src/compile/win32/Direct3d9/Release/*.dll`
- `src/compile/win32/Direct3d9_ffp/Release/*.dll`
- `src/compile/win32/Direct3d9_vsps/Release/*.dll`
- `src/compile/win32/SwgClient/Release/`
- `src/compile/win32/SwgClientSetup/Release/`

### Example B: Win64 DXVK packaging

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Architecture win64 -Configuration Release -Runtime dxvk -FailIfDxvkMissing
```

Expected Win64 source and default destination paths:

- `src/compile/win64/Direct3d9/Release/*.dll`
- `src/compile/win64/Direct3d9_ffp/Release/*.dll`
- `src/compile/win64/Direct3d9_vsps/Release/*.dll`
- `src/compile/win64/SwgClient/Release/`
- `src/compile/win64/SwgClientSetup/Release/`
- DXVK overlay source: `src/external/3rd/dxvk/2.4/x64/d3d9.dll`

The script now applies architecture guardrails and refuses cross-architecture packaging (for example Win32 packaging when only Win64 renderer/DXVK DLLs are present).

### Custom client output targets

Use `-ClientOutputDirs` to override deployment destinations:

```powershell
powershell -ExecutionPolicy Bypass -File tools/win32/package-win32-renderers.ps1 -Architecture win64 -Configuration Release -Runtime dxvk -ClientOutputDirs C:\staging\client_r,C:\staging\launcher
```

## Deployment expectations and rollback

- **Native distribution**: do not ship a local `d3d9.dll`; runtime resolves to the OS-provided Direct3D 9 implementation.
- **DXVK distribution**: ship `d3d9.dll` in the same directory as `SwgClient_r.exe` (and any equivalent launcher/client runtime folder).
- **Rollback to native runtime**: remove the local `d3d9.dll` from deployment directories, or rerun packaging with `-Runtime native` to clean output targets.


## Configuration semantics

- **Debug**: Uses debug macros/CRT for full development-time diagnostics.
- **Optimized**: Uses non-debug CRT and `NDEBUG` like Release, while retaining project-specific diagnostic defines (for example `DEBUG_LEVEL=1` in Direct3D9 renderer projects).
- **Release**: Uses non-debug CRT and `NDEBUG` with production-oriented diagnostic levels/settings.

This keeps Optimized builds diagnostically useful without mixing debug CRT (`/MTd`) into non-debug client renderer outputs.
