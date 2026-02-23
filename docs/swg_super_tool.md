# SWG Content Super Tool

The **SWG Content Super Tool** unifies the legacy `SWG_MSHImporter.ms` and `SWG_MSHFullExporter.ms` MaxScript utilities into a single workflow-oriented interface for 3ds Max. It wraps the importer, exporter, and several common scene preparation steps in one launcher so artists can focus on building assets instead of hunting for separate scripts.

## Key features

- **Unified launcher:** load both the importer and exporter without spawning multiple floaters.
- **Automation helpers:** quick buttons for geometry prep, skeleton cleanup, animation baking, and a one-click roundtrip export.
- **Content path management:** track the active asset tag and target output folder, with clipboard shortcuts for `.mgn`, `.msh`, and `.skt` targets.
- **Animation library:** register named clips, apply their frame ranges, and bake them against the current selection.
- **Legacy access:** open the original standalone importer/exporter floaters at any time.

## Installation

1. Copy the following scripts into the same directory (for example, `C:\Program Files\Autodesk\3ds Max <version>\scripts`):
   - `SWG_MSHImporter.ms`
   - `SWG_MSHFullExporter.ms`
   - `SWG_SuperTool.ms`
2. From 3ds Max, run **Scripting ➜ Run Script…** and select `SWG_SuperTool.ms`.
3. The launcher appears as a floater titled **SWG Content Super Tool**.

> The super tool disables the importer/exporter auto-launch when it loads them, so they do not pop up separate windows unless you explicitly request the legacy view.

## Usage overview

1. **Set context**
   - Enter an *Asset Tag* (used for filename hints and log messages).
   - Click **Select Output Folder** to point to your working directory.
2. **Prepare content**
   - Use **Prepare Geometry** to reset transforms, collapse stacks, and convert meshes to editable polys.
   - Use **Prepare Skeleton** to normalise bone settings (scale mode, show links, etc.).
3. **Manage animations**
   - Adjust the **Animation Range** spinners to change the scene range.
   - Switch to the **Utilities** panel, create named clips, and bake them with **Bake Selected Clips**.
4. **Import/Export**
   - Toggle between **Importer**, **Exporter**, and **Utilities** panels with the panel buttons.
   - Trigger importer/exporter actions directly or press **Roundtrip Export** for a combined prep + export pass.
   - Clipboard buttons expose consistent output paths for `.mgn`, `.msh`, and `.skt` assets.
   - Use **Batch Repair MSH** in the Utilities panel to pick an input folder, run the mesh cleanup pass on every `.msh` it contains, and export repaired copies to a chosen output folder.
5. **Legacy UI**
   - If needed, click **Legacy Importer** or **Legacy Exporter** from the Utilities panel to reopen the original floaters.

## Extensibility

The script exposes several helper functions (`swgSuper_PrepareGeometry`, `swgSuper_BakeAnimation`, etc.) that you can call from MaxScript listener/macros to integrate with custom pipelines. Because the importer/exporter scripts now respect the `SWG_Importer_AutoLaunch` and `SWG_Exporter_AutoLaunch` flags, you can safely embed them in other tooling without spawning duplicate windows.
