# CMake Configuration

The modernization effort standardises the build system around CMake while preserving compatibility with the legacy Visual
Studio solutions. Start new development against this configuration so that tooling and CI can share the same build graph.

## Quick start

```bash
cmake -S . -B build -G "Ninja"
cmake --build build
```

For Visual Studio 2022 on Windows, pass the provided toolchain file:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" \
      -T v143 \
      -A x64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Windows-MSVC.cmake
```

## Targets

* `swg_plugin_api` – header-only interface describing the modern plugin ABI.
* `WorldBuilderProceduralPlugin` – example plugin demonstrating lifecycle integration.
* `swg_direct3d10_bootstrap` – runtime probe that verifies Direct3D 10 dependencies and
  feature support before the renderer switches away from the DX9 path.

Enable `SWG_ENABLE_TESTS` to compile forthcoming unit tests that live under `src/tests` (to be added as subsystems are
modernised).
