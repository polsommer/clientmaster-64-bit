# swg+creation_tool

`swg+creation_tool` is a small C++ utility for building SOE/SWG style IFF
binary files from a JSON description. It mirrors the lightweight creation flow
from SIE 3.11.6.4 while fitting into the modernised CMake toolchain. The same
core is also embedded inside the `swg_tool_gui` desktop app for live authoring
and archive injection.

## Building

```bash
cmake -S . -B build
cmake --build build --target swg_creation_tool
```

The compiled executable is named `swg+creation_tool` to match the legacy
workflow.

## Usage

Provide a JSON definition that identifies each FORM or CHUNK node. Tags must be
exactly four ASCII characters and data payloads are padded to even byte
boundaries.

```json
{
  "form": "TEST",
  "children": [
    {"chunk": "INFO", "data": "hello"},
    {"chunk": "DATA", "data": [1, 2, 3, 4]},
    {"form": "NEST", "children": [
      {"chunk": "LEAF", "data": "aGVsbG8=", "encoding": "base64"}
    ]}
  ]
}
```

Run the tool to produce an IFF and optionally print the structure:

```bash
./build/src/tools/swg_creation_tool/swg+creation_tool \
  --input example.json \
  --output output.iff \
  --describe
```

Supported encodings for the `data` field when it contains a string:

- `text` (default): stored as UTF-8 bytes
- `hex`: pairs of hexadecimal digits converted directly to bytes
- `base64`: standard base64 decoded to bytes

Non-string payloads are serialized as compact JSON unless they are arrays of
byte-sized integers (0–255), in which case the raw values are written directly.
