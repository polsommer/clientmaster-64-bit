# Star Wars Galaxies 3ds Max Exporter (MaxScript)

This document describes the MaxScript-based Star Wars Galaxies exporter for Autodesk 3ds Max. The script mirrors the legacy Maya exporter so artists can create static meshes, skeletal assets, hardpoints, portal volumes, and shader data directly inside Max and deliver the same game-ready files used by the live SWG server.

## Installation

1. Copy `tools/swg_max_exporter.ms` into your `%LOCALAPPDATA%/Autodesk/3dsMax/<version> - 64bit/Scripts/Startup` directory.
2. Launch 3ds Max. On startup the script registers the **SWG Exporter** macro under the `Star Wars Galaxies` category.
3. Assign the macro to a toolbar button or open it from the **Utilities** panel to display the rollout.

To uninstall, remove the script from the Startup directory and restart Max. You can also run `swg_uninstall()` from the MaxScript listener to remove the macro during the current session.

## Features

The rollout provides the following functionality:

- **Project and path configuration** – persistent settings stored in `SWGExporterSettings.ini` allow you to match the Maya exporter folder layout (appearance, mesh, skeleton, animation, shader, collision, logs, and publish).
- **Scene validation** – checks for SWG naming conventions, uniform scaling, and frozen transforms before export.
- **Batch export buttons** – dedicated actions for static meshes (`.msh`), skeletal meshes (`.skm`), animations (`.ska`), portal/cell layouts (`.cmp`), and collision volumes (`.clm`).
- **FBX hand-off** – uses the native FBX exporter to write an intermediate file for the current selection.
- **swg-tool integration** – optionally runs the Python `swg-tool export` pipeline to convert the FBX into SWG-ready output files.
- **Publishing helper** – writes a changelist seed file containing the exported assets for downstream Perforce automation.

## Usage

1. Open the rollout and configure the project root plus output directories. Paths are remembered between sessions.
2. Load or select the Max scene you want to export. If nothing is selected the exporter will use all visible geometry.
3. Click **Validate Scene** to ensure naming and transforms are correct.
4. Choose the appropriate export button. The exporter writes an FBX into your temp directory and, if enabled, automatically converts it through `swg-tool` into the SWG formats.
5. If publishing is enabled, a changelist seed file is generated under the publish directory.
6. Review the log file recorded in the configured logs directory for detailed warnings or errors.

## swg-tool Integration

For automatic conversion, make sure Python can run the SWG command-line utilities:

- Install Python 3.9 or newer and ensure it is available on the `PATH`, or set the `SWG_PYTHON` environment variable to point at the interpreter you want to use.
- Place the `swg-tool.py` entry point from this repository in `<project root>/tools` so the script can find it.
- The script runs `python swg-tool.py export --type <type> --input <file> --output <file>` and logs stdout/stderr for troubleshooting.

## Troubleshooting

- **FBX exporter errors** – install the Autodesk FBX plugin if export fails. The MaxScript reports the exception that caused the problem.
- **Conversion skipped** – if the script cannot find `swg-tool.py` it leaves the intermediate FBX in the temp directory for manual processing.
- **Publishing** – the changelist file is plain text and can be ingested by your existing Perforce submit scripts.

## Advanced Tips

- The exporter persists settings per-user. Delete `%LOCALAPPDATA%/Autodesk/3dsMax/<version> - 64bit/Scripts/SWGExporterSettings.ini` to reset everything.
- Use Max layers named `hp_*` and `col_*` to quickly author hardpoints and collision geometry that pass validation.
- Because exports are serialized through a mutex flag, you can trigger actions from MaxScript or macros without colliding with an in-progress export.

For additional automation, integrate the rollout buttons into custom toolbars or keyboard shortcuts so artists transitioning from Maya feel right at home.
