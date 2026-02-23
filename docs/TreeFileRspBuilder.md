# TreeFileRspBuilder User Guide

TreeFileRspBuilder is a Windows utility that walks the asset directories defined in a configuration file and produces the response (`.rsp`) lists consumed by the classic Star Wars Galaxies build tools. The program categorises files by extension (music, samples, textures, animations, static meshes, skeletal meshes, and miscellaneous compressed data) and writes one response file per bucket in the working directory.

## Build requirements

TreeFileRspBuilder ships as part of the legacy Visual Studio build. Follow the repository instructions in [README.md](../README.md#build-instructions) to install Visual Studio 2013, then load the solution at `src/build/win32/swg.sln`.

1. Set the solution configuration to **Release** and the platform to **Win32**.
2. In **Solution Explorer**, locate **TreeFileRspBuilder** under `engine/shared/application/TreeFileRspBuilder`.
3. Right-click the project and choose **Build**. The executable is emitted to `src/engine/shared/application/TreeFileRspBuilder/build/win32/Release/TreeFileRspBuilder.exe`.

You can also build from a developer command prompt using MSBuild:

```bat
msbuild src/engine/shared/application/TreeFileRspBuilder/build/win32/TreeFileRspBuilder.vcxproj /p:Configuration=Release /p:Platform=Win32
```

## Configuration file

The tool expects an INI-style configuration file that lists one or more search paths. Each entry must contain the string `searchPath` on the left-hand side of a `key = value` pair. Lines can include comments that start with `#` or `;`, and leading/trailing whitespace is ignored.

Example `TreeFileRspBuilder.cfg`:

```ini
# Root game data
searchPath0 = C:/swg/live/data

# Additional asset directories
searchPath1 = C:/swg/live/custom
searchPath2 = D:/audio/uncompressed
```

Place the configuration file next to the executable if you want TreeFileRspBuilder to auto-detect it, or store it anywhere convenient and provide the path on the command line.

## Running the tool

TreeFileRspBuilder can be launched either from a console or by double-clicking it in Explorer:

### Command-line usage

```bat
TreeFileRspBuilder.exe <path-to-config>
```

* If you omit the argument and a file named `TreeFileRspBuilder.cfg` sits beside the executable, the program uses it automatically.
* Otherwise, TreeFileRspBuilder opens a file picker so you can choose the configuration interactively.

During execution the utility prints a line for each resolved search path. Additional warnings describe skipped directories (reparse points) and duplicate asset names. When attached to a console window, the output streams directly to standard output. When launched without a console (for example, by double-clicking in Explorer), the program buffers log messages and shows them in a completion dialog.

### Generated files

The following response files are produced in the current working directory:

| File | Contents |
| --- | --- |
| `data_uncompressed_music.rsp` | `.mp3` assets |
| `data_uncompressed_sample.rsp` | `.wav` assets |
| `data_compressed_texture.rsp` | `.dds` textures |
| `data_compressed_animation.rsp` | `.ans` animations |
| `data_compressed_mesh_static.rsp` | `.msh` static meshes |
| `data_compressed_mesh_skeletal.rsp` | `.mgn` skeletal meshes |
| `data_compressed_other.rsp` | All other files |

Each `.rsp` line has the form `<virtual path> @ <absolute file path>`. Virtual paths correspond to the relative location under the scanned directory, while the absolute portion records where the asset lives on disk.

## Workflow checklist

1. Build the Release configuration of TreeFileRspBuilder.
2. Create or update the configuration file with the desired `searchPath` entries.
3. Copy the configuration file next to the executable or note its location for the command-line argument.
4. Launch TreeFileRspBuilder.
5. Review the log output (console or dialog) for warnings or missing assets.
6. Confirm that the expected `.rsp` files appear alongside the executable.
7. Commit or distribute the response files to downstream build steps.

## Troubleshooting

| Symptom | Resolution |
| --- | --- |
| "Failed to open config file" | Verify the path passed on the command line or that `TreeFileRspBuilder.cfg` is beside the executable. Ensure the file is not locked by another program. |
| "Warning: no searchPath entries" | The configuration did not contain any `searchPath` keys. Add at least one valid directory entry. |
| "Skipping reparse point" | The directory is a symlink or junction. Adjust the configuration to point directly at the target directory if necessary. |
| "Duplicate found" messages | Multiple directories produced the same virtual path. Resolve the collision by removing or renaming one of the assets. |
| "Failed to open output file" | Confirm you have write access to the working directory and that the `.rsp` files are not read-only. |

If the utility exits immediately when launched from Explorer, check the completion dialog for errors. You can also run it from a command prompt to keep the console window open after it finishes.
