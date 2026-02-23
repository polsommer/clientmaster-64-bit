# DXVK 2.4 vendor drop

This directory pins the DXVK runtime used by Win32 client packaging.

- Expected file: `x32/d3d9.dll`
- Expected source release: <https://github.com/doitsujin/dxvk/releases/tag/v2.4>
- Packaging metadata: `DXVK_VERSION.json`

`x32/d3d9.dll` is intentionally not recreated by build scripts. Keep the exact vendor binary in this path so packaging can copy it into client output directories for DXVK-enabled builds.
