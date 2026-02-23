# swg-tool CLI

`swg-tool` streamlines common workflows for the modernised God Client toolchain: validating plugin manifests, generating
navmeshes, and publishing content bundles. Each subcommand intentionally mirrors the automation goals from the modernization
plan so teams can begin scripting repeatable tasks immediately.

## Usage

```bash
python -m swg_tool --help
```

### Validate plugin manifests

```bash
python -m swg_tool validate plugin/examples/world_builder_procedural/plugin.json
```

### Generate a navmesh prototype

```bash
python -m swg_tool generate-navmesh --terrain data/heightmap.txt --output build/navmesh.json
```

### Run a shard load test

Describe a scenario manifest with the client binary, shard target, and launch
cadence. The runner supervises stdout/stderr logs for each client and emits a
JSON report so CI jobs can gate on success counts.

```bash
python -m swg_tool loadtest \
  --manifest examples/loadtest.json \
  --logs ./loadtest-logs \
  --report ./build/loadtest-report.json \
  --stream-report
```

Manifest schema:

```json
{
  "client_path": "../bin/SwgClientSetup_r.exe",
  "clients": 10,
  "launch_rate_per_minute": 30,
  "working_directory": "../bin",
  "wrapper": ["wine64"],
  "shard": { "host": "login.swgplus.com", "port": 44453 },
  "arguments": ["--nointro"],
  "env": { "SWG_USERNAME": "bot", "SWG_PASSWORD": "bot" },
  "timeout_seconds": 300,
  "max_retries": 1
}
```

* `client_path`: Absolute or manifest-relative path to the client executable
  to launch.
* `working_directory`: Directory used as the client's CWD. Defaults to the
  directory containing `client_path` so the SWG client can find its data
  tree.
* `clients`: Number of concurrent clients to start.
* `launch_rate_per_minute`: Pace new launches so the runner staggers attempts
  instead of spawning everything at once.
* `shard.host` / `shard.port`: Login/shard endpoint propagated to the client
  environment as `SWG_SHARD_HOST` and `SWG_SHARD_PORT`.
* `wrapper`: Optional command prefix for the executable (for example
  `["wine64"]` when running the Windows client on Linux).
* `arguments`: Optional list of CLI arguments passed to every client.
* `env`: Optional environment variable overrides added to each process.
* `timeout_seconds`: Per-client wall clock timeout before a forced kill.
* `max_retries`: Additional attempts per client on failure or timeout.

### Generate an automated terrain plan

Provide a minimal terrain document (map, tile, and chunk sizes in meters) and
ask the headless driver to run the TerrainAutoPainter along with the Smart
Terrain Analyzer audit. The command emits JSON to stdout or the path passed to
`--output`.

```bash
python -m swg_tool terrain-plan \
  --document examples/terrain_document.json \
  --output build/automation-plan.json
```

Pass `--serve <port>` to expose an HTTP endpoint at `/plan` that returns the
same JSON body for on-demand requests from the website.

### Create a lightweight IFF resource

```bash
python -m swg_tool create-iff --definition examples/simple_chunk.json --output build/simple.iff --preview
```

The `--definition` file describes a tree of `FORM` and `CHUNK` nodes similar to
what SIE exposes in its GUI. A minimal chunk-only file looks like:

```json
{ "chunk": "TEST", "data": "hello world" }
```

### Create shader template files

Generate shader templates that point at supported texture formats (`.dds`, `.png`, `.pgn`) and the default
`effect\\simplemt1z.eft` effect. DDS remains the preferred runtime format for best performance.

```bash
python -m swg_tool create-shader --texture texture/example.png --output shader/example.sht
```

Use `--output-dir` to batch-generate shader files from multiple textures:

```bash
python -m swg_tool create-shader --texture texture/a.dds --texture texture/b.png --output-dir shader
```


When using `--texture-dir`, the default `--pattern` is `*.*` and batch generation
processes `.dds`, `.png`, and `.pgn` files automatically (unless you pass
`--allow-non-dds` to include additional extensions).

Launch the GUI helper if you want a simple windowed workflow:

```bash
python -m swg_tool shader-gui
```

If you run the entry point without arguments, it opens the shader GUI by default:

```bash
python tools/swg-tool.py
```

Convert legacy shader templates to the current standard:

```bash
python -m swg_tool convert-shader --input shader/old_shader.sht --output-dir shader
```

Batch-convert shader templates across a directory tree:

```bash
python -m swg_tool auto-update-shaders \
  --input-dir shader/legacy \
  --output-dir shader/converted \
  --pattern "*.sht" \
  --max-workers 8
```

Normalize client HLSL vertex shader headers for Shader Model 4.0 targets (DX10):

```bash
python -m swg_tool upgrade-hlsl --input-dir "clientHLSL files" --target vs_4_0 --mode smart
```

Write normalized shaders to a separate output directory (with CUDA or ROCm acceleration when available):

```bash
python -m swg_tool upgrade-hlsl \
  --input-dir "clientHLSL files" \
  --output-dir build/clientHLSL-upgraded \
  --target vs_4_0 \
  --mode add \
  --gpu \
  --allow-unsupported
```

### Publish a content pack

```bash
python -m swg_tool publish --content ./dist/content --destination ./publish/releases
```

### Generate a response file

```bash
python -m swg_tool generate-response \
  --source ./dist/content \
  --source ./dist/localized/en \
  --destination build/generated_content.rsp
```

### Create an encrypted `.tres` or `.tresx` archive

```bash
python -m swg_tool create-tree-file \
  --source ./dist/content \
  --source ./dist/localized/en \
  --output publish/snapshot.tresx \
  --passphrase "secret"
```

The command enforces a `.tres` or `.tresx` output extension by default. Use `--encrypt` if you need to keep a non-standard file extension.

If the legacy `TreeFileBuilder` executable is unavailable, the tooling now falls back to an internal builder implementation automatically. Provide `--builder` to force the external executable (and to error if it is missing), or pass `--internal-builder` to bypass external detection entirely. Pass `--gpu` to request GPU acceleration in the external builder (when supported) alongside the optional preflight scan. The GPU path is compatible with CUDA and ROCm (for AMD cards such as the RX 7600); the internal builder will fall back to CPU with a warning when `--gpu` is requested but no external GPU-capable builder is available.

### Build a `.tre`, `.tres`, or `.tresx` archive

```bash
python -m swg_tool build-tree --response build/content.rsp --output publish/snapshot.tresx --encrypt --passphrase "secret"
```

You can also have the command generate the response file on the fly from directories or individual files:

```bash
python -m swg_tool build-tree \
  --source ./dist/content \
  --source ./dist/localized/en \
  --response build/generated_content.rsp \
  --output publish/snapshot.tre
```

When `--source` is provided the inputs are walked recursively, a deterministic response file is produced (either at `--response` or a temporary location), and then handed to the builder. Use `--entry-root` to control how tree entry paths are calculated when the defaults are not suitable.

The command uses the legacy `TreeFileBuilder` executable when available, but it will automatically fall back to the internal builder if the executable is missing. Pass `--builder` to require the external toolchain explicitly, or `--internal-builder` to always use the built-in implementation.

Use `--gpu` to request GPU acceleration from the external `TreeFileBuilder` when available (CUDA or ROCm).

### List or extract entries from `.tre`, `.tres`, or `.tresx` archives

For encrypted `.tres` or `.tresx` files, supply the passphrase with `--passphrase` or set `SWG_TREEFILE_PASSPHRASE` to reuse a shared key across commands.

```bash
python -m swg_tool treefile list publish/snapshot.tresx --passphrase "secret"
python -m swg_tool treefile extract publish/snapshot.tresx --output build/extracted --passphrase "secret"
```

See `python -m swg_tool <command> --help` for a full list of arguments.
