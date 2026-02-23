# SWG Tool Studio (swg_tool_gui)

`swg_tool_gui` merges the legacy creation tool and TRE/TRES archive GUI into a single desktop workspace.
It still writes game-compatible TREE (0005) archives, but now also includes a live IFF builder so you can assemble payloads and drop them straight into the archive without leaving the app.

## Building

The tools depend on zlib and OpenSSL for archive compression and encryption. The GUI additionally requires Qt Widgets; CMake will try Qt 6 first, then Qt 5. If no compatible Qt is found only the command-line converter target (`swg_tre_tool`) is built, so the archive utilities still appear in generated projects.

```bash
cmake -S . -B build
cmake --build build --target swg_tool_gui
```

Use `-DSWG_ENABLE_TRE_GUI=OFF` to skip building the tool when configuring.

## Using the tool

1. Launch `swg_tool_gui`.
2. Switch between the **Archives** tab (TRE/TRES editing) and **IFF Builder** tab (live FORM/CHUNK authoring).
3. In **Archives**, click **Add Files** to drop in payloads or remove entries; selecting an item shows a hex/text preview.
4. In **IFF Builder**, paste or load a JSON definition, review the described tree and binary preview, then **Save IFF** or **Add to Archive** with the desired entry name and compression choice.

Archive files are stored with standard `TREE` headers (version `0005`), CRC-sorted TOCs, and optional zlib-compressed payloads so they can be loaded directly by the game client.
