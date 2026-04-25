# Plugin deployment in mixed-architecture environments

This document defines how plugin manifests and binaries are laid out so hosts reject incompatible modules **before** calling `LoadLibrary`.

## Manifest schema requirement

Each `plugin.json` must declare architecture support:

```json
{
  "name": "ExamplePlugin",
  "entryPoint": "SwgRegisterPlugin",
  "library": "ExamplePlugin",
  "supportedArchitectures": ["x86", "x64"]
}
```

Rules:

1. `supportedArchitectures` is required and must be a non-empty array.
2. Allowed values are `x86` and `x64`.
3. The host validates this field against process bitness and rejects mismatches before loading DLLs.

## Filesystem layout

Keep one manifest root per plugin and split binaries by architecture:

- `plugin/<plugin-name>/plugin.json`
- `plugin/<plugin-name>/win32/<plugin-dll>.dll`
- `plugin/<plugin-name>/win64/<plugin-dll>.dll`

For nested plugin names, preserve the existing folder structure, then append the architecture segment for binaries:

- `plugin/examples/world_builder_procedural/plugin.json`
- `plugin/examples/world_builder_procedural/win32/WorldBuilderProceduralPlugin.dll`
- `plugin/examples/world_builder_procedural/win64/WorldBuilderProceduralPlugin.dll`

## Host loading behavior

1. Parse `plugin.json`.
2. Verify required fields (`name`, `entryPoint`, `library`, `supportedArchitectures`).
3. Detect current process architecture.
4. Reject the manifest if architecture is not supported.
5. Resolve binary path under `<manifest-folder>/<win32|win64>/`.
6. Only then call `LoadLibrary`.

## Server/client separation guidance

In mixed deployments (for example 64-bit server process plus 32-bit legacy tools):

- Keep separate plugin roots per host role where possible (for example `plugin/client/*` and `plugin/server/*`).
- Do not share a single DLL directory between server and client processes.
- Mark client-only plugins as client-deployed only in release notes and packaging manifests.
- If a plugin is intentionally cross-host, ship both binaries and include both values in `supportedArchitectures`.
- If a plugin is host-specific, ship only the relevant binary directory and list only its architecture.

These rules prevent accidental server startup failures caused by loading client-only or wrong-architecture modules.
