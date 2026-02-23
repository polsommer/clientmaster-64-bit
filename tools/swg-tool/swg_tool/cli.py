"""Command-line entry point for the SWG God Client workflow automation tooling."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Iterable, Optional

from . import __version__
from .autopainter import (
    AutoPainterConfig,
    TerrainDocument,
    load_config_from_args,
    run_headless,
    serve_headless,
)
from .manifest import ManifestValidator
from .navmesh import NavMeshGenerator
from .publisher import Publisher, PublishError
from .preflight import format_preflight_report, run_tree_preflight
from .response import ResponseFileBuilder, ResponseFileBuilderError
from .rspconfig import (
    TreeFileRspConfigBuilder,
    TreeFileRspConfigBuilderError,
)
from .iff import IffBuilder, IffDefinitionError
from .hlsl_upgrade import (
    DEFAULT_INPUT_DIR,
    DEFAULT_PATTERN,
    DEFAULT_TARGET_PROFILE,
    normalize_target_profile,
    SUPPORTED_DX9_PROFILES,
    iter_changed_results,
    upgrade_hlsl_in_dir,
)
from .loadtest import LoadTestScenario, run_loadtest
from .shader import (
    DEFAULT_EFFECT,
    DEFAULT_MATERIAL,
    DEFAULT_PROFILE_MAP,
    DEFAULT_QUALITY_ORDER,
    ShaderProfileMap,
    ShaderTemplateError,
    ShaderTemplateRequest,
    ShaderTemplateBatchItem,
    ShaderTemplateBatchSummary,
    create_shader_templates_from_dir,
    convert_shader_templates_in_dir,
    convert_shader_template_with_report,
    launch_shader_gui,
    load_shader_profiles,
    normalize_texture_reference,
    resolve_shader_profile,
    validate_effect_name,
    write_shader_template,
)
from .treebuilder import TreeFileBuilder, TreeFileBuilderError
from .treefile_gui import main as launch_treefile_gui
from .treefile_ops import (
    TreeFileExtractorError,
    extract_treefile,
    list_treefile,
)


def _path(value: str) -> Path:
    path = Path(value).expanduser().resolve()
    return path


def _existing_file(value: str) -> Path:
    path = _path(value)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"File not found: {path}")
    return path


def _existing_directory(value: str) -> Path:
    path = _path(value)
    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"Directory not found: {path}")
    return path


def _existing_path(value: str) -> Path:
    path = _path(value)
    if not path.exists():
        raise argparse.ArgumentTypeError(f"Path not found: {path}")
    return path


def _ensure_directory(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def _normalize_encrypted_output(output: Path, *, allow_non_tres: bool) -> Path:
    if output.suffix.lower() in {".tres", ".tresx"}:
        return output
    if allow_non_tres:
        return output
    adjusted = output.with_suffix(".tres")
    print(
        "[INFO] Adjusted output path to "
        f"{adjusted} to enforce .tres or .tresx extension."
    )
    return adjusted


def _should_encrypt_tree(args: argparse.Namespace) -> bool:
    if args.no_encrypt:
        return False
    if args.encrypt:
        return True
    if args.passphrase is not None:
        return True
    return args.output.suffix.lower() in {".tres", ".tresx"}


def _builder_debug_lines(builder: TreeFileBuilder) -> list[str]:
    capabilities = builder.capabilities
    feature_map = {
        "passphrase": capabilities.supports_passphrase,
        "encrypt": capabilities.supports_encrypt,
        "no-encrypt": capabilities.supports_no_encrypt,
        "quiet": capabilities.supports_quiet,
        "dry-run": capabilities.supports_dry_run,
        "gpu": capabilities.supports_gpu,
    }
    supported = [name for name, enabled in feature_map.items() if enabled]
    unsupported = [name for name, enabled in feature_map.items() if not enabled]
    lines = []
    if builder.using_internal:
        lines.append("[debug] TreeFileBuilder: internal implementation")
    else:
        lines.append(f"[debug] TreeFileBuilder: {builder.executable}")
    if supported:
        lines.append(f"[debug] Supports: {', '.join(supported)}")
    if unsupported:
        lines.append(f"[debug] Unsupported: {', '.join(unsupported)}")
    return lines


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Tools for modern SWG God Client workflows")
    parser.add_argument("--version", action="version", version=f"swg-tool {__version__}")

    subparsers = parser.add_subparsers(dest="command", required=True)

    validate_parser = subparsers.add_parser(
        "validate", help="Validate plugin or content pack manifests"
    )
    validate_parser.add_argument(
        "manifests",
        nargs="+",
        type=_existing_file,
        help="Path(s) to manifest JSON files",
    )
    validate_parser.set_defaults(func=_run_validate)

    navmesh_parser = subparsers.add_parser(
        "generate-navmesh", help="Generate a prototype navmesh JSON from a heightmap"
    )
    navmesh_parser.add_argument(
        "--terrain",
        required=True,
        type=_existing_file,
        help="Input heightmap file (plain text grid of elevations)",
    )
    navmesh_parser.add_argument(
        "--output",
        required=True,
        type=_path,
        help="Destination JSON file for the generated navmesh",
    )
    navmesh_parser.add_argument(
        "--walkable-threshold",
        type=float,
        default=15.0,
        help="Maximum slope (degrees) considered walkable",
    )
    navmesh_parser.set_defaults(func=_run_generate_navmesh)

    loadtest_parser = subparsers.add_parser(
        "loadtest", help="Launch multiple client instances for shard load testing"
    )
    loadtest_parser.add_argument(
        "--manifest",
        required=True,
        type=_existing_file,
        help="Scenario manifest describing clients, rate, and shard target",
    )
    loadtest_parser.add_argument(
        "--logs",
        type=_path,
        default=Path("loadtest-logs"),
        help="Directory where client stdout/stderr logs should be written",
    )
    loadtest_parser.add_argument(
        "--report",
        required=True,
        type=_path,
        help="Path to write the consolidated JSON report",
    )
    loadtest_parser.add_argument(
        "--stream-report",
        action="store_true",
        help="Also print the generated report JSON to stdout",
    )
    loadtest_parser.set_defaults(func=_run_loadtest)

    autopaint_parser = subparsers.add_parser(
        "terrain-plan",
        help="Run the TerrainAutoPainter headlessly and emit JSON guidance",
    )
    autopaint_parser.add_argument(
        "--document",
        required=True,
        type=_existing_file,
        help="Path to a JSON terrain document (map_width_m, tile_width_m, chunk_width_m)",
    )
    autopaint_parser.add_argument(
        "--output",
        type=_path,
        help="Optional file to write the generated plan JSON",
    )
    autopaint_parser.add_argument(
        "--serve",
        type=int,
        help="If provided, start an HTTP service on the given port instead of exiting",
    )
    autopaint_parser.add_argument("--grid-size", type=int, help="Grid resolution for the generator")
    autopaint_parser.add_argument("--roughness", type=float, help="Diamond-square roughness")
    autopaint_parser.add_argument("--seed", type=int, help="Random seed")
    autopaint_parser.add_argument(
        "--erosion-iterations", type=int, help="Thermal erosion passes to apply"
    )
    autopaint_parser.add_argument("--plateau-bias", type=float, help="Plateau bias factor")
    autopaint_parser.add_argument("--water-level", type=float, help="Water table threshold")
    autopaint_parser.add_argument(
        "--flora-threshold", type=float, help="Minimum height to count towards flora bands"
    )
    autopaint_parser.add_argument(
        "--settlement-threshold", type=float, help="Minimum elevation for settlements"
    )
    autopaint_parser.add_argument(
        "--desired-settlement-count", type=int, help="Number of hubs to recommend"
    )
    autopaint_parser.add_argument("--river-count", type=int, help="Number of rivers to seed")
    autopaint_parser.add_argument(
        "--travel-corridor-threshold",
        type=float,
        help="Score gate for travel corridor candidates",
    )
    autopaint_parser.add_argument(
        "--logistics-hub-count",
        type=int,
        help="Number of logistics hubs to fold into automation messaging",
    )
    autopaint_parser.set_defaults(func=_run_terrain_plan)

    iff_parser = subparsers.add_parser(
        "create-iff", help="Generate a binary IFF file from a JSON definition",
    )
    iff_parser.add_argument(
        "--definition",
        required=True,
        type=_existing_file,
        help="Path to a JSON file describing the FORM/CHUNK layout",
    )
    iff_parser.add_argument(
        "--output",
        required=True,
        type=_path,
        help="Destination path for the generated IFF file",
    )
    iff_parser.add_argument(
        "--preview",
        action="store_true",
        help="Print a human-readable summary of the generated structure",
    )
    iff_parser.set_defaults(func=_run_create_iff)

    shader_parser = subparsers.add_parser(
        "create-shader",
        help="Generate shader template (.sht) files for DDS/PNG/PGN textures",
    )
    shader_parser.add_argument(
        "--texture",
        dest="textures",
        action="append",
        help=(
            "Texture name to embed in the shader "
            "(for example texture/example.dds or texture/example.png)"
        ),
    )
    shader_parser.add_argument(
        "--texture-dir",
        dest="texture_dirs",
        action="append",
        type=_existing_directory,
        help="Directory to scan recursively for textures",
    )
    shader_parser.add_argument(
        "--output",
        type=_path,
        help="Explicit output path for a single shader file",
    )
    shader_parser.add_argument(
        "--output-dir",
        type=_path,
        default=Path("shader"),
        help="Directory where shader files should be written (default: ./shader)",
    )
    shader_parser.add_argument(
        "--pattern",
        default="*.*",
        help=(
            "Glob pattern to find textures when scanning directories "
            "(default: *.*; filters to .dds/.png/.pgn unless --allow-non-dds is set)"
        ),
    )
    shader_parser.add_argument(
        "--effect",
        default=DEFAULT_EFFECT,
        help="Effect template to reference in the shader (default: effect\\simplemt1z.eft)",
    )
    shader_parser.add_argument(
        "--effect-root",
        type=_path,
        help="Optional root directory to validate effect references",
    )
    shader_parser.add_argument(
        "--strict-effects",
        action="store_true",
        help="Treat missing effect references as errors",
    )
    shader_parser.add_argument(
        "--profile-map",
        type=_existing_file,
        default=DEFAULT_PROFILE_MAP,
        help="Path to a shader profile map (default: bundled shader_profiles.json)",
    )
    shader_parser.add_argument(
        "--quality",
        default=DEFAULT_QUALITY_ORDER[-1],
        choices=DEFAULT_QUALITY_ORDER,
        help="Requested quality tier for profile selection (default: high)",
    )
    shader_parser.add_argument(
        "--allow-non-dds",
        action="store_true",
        help="Allow texture names with extensions beyond .dds/.png/.pgn",
    )
    shader_parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing shader templates when scanning directories",
    )
    shader_parser.set_defaults(func=_run_create_shader)

    gui_parser = subparsers.add_parser(
        "shader-gui",
        help="Launch a GUI for creating shader template (.sht) files",
    )
    gui_parser.set_defaults(func=_run_shader_gui)

    convert_parser = subparsers.add_parser(
        "convert-shader",
        help="Convert legacy shader templates to the current standard",
    )
    convert_parser.add_argument(
        "--input",
        dest="inputs",
        action="append",
        required=True,
        type=_existing_file,
        help="Legacy shader template (.sht) file to convert",
    )
    convert_parser.add_argument(
        "--output-dir",
        type=_path,
        default=Path("shader"),
        help="Directory where converted shader files should be written",
    )
    convert_parser.add_argument(
        "--effect-root",
        type=_path,
        help="Optional root directory to validate effect references",
    )
    convert_parser.add_argument(
        "--strict-effects",
        action="store_true",
        help="Treat missing effect references as errors",
    )
    convert_parser.set_defaults(func=_run_convert_shader)

    auto_convert_parser = subparsers.add_parser(
        "auto-update-shaders",
        help="Convert shader templates in a directory tree in parallel",
    )
    auto_convert_parser.add_argument(
        "--input-dir",
        required=True,
        type=_existing_directory,
        help="Root directory to scan for shader templates",
    )
    auto_convert_parser.add_argument(
        "--output-dir",
        required=True,
        type=_path,
        help="Destination root for converted shader templates",
    )
    auto_convert_parser.add_argument(
        "--pattern",
        default="*.sht",
        help="Glob pattern for shader templates (default: *.sht)",
    )
    auto_convert_parser.add_argument(
        "--max-workers",
        type=int,
        help="Maximum number of worker threads to use",
    )
    auto_convert_parser.add_argument(
        "--quality",
        help=(
            "Optional quality tier to resolve shader profiles "
            f"(choices in profile map, default order: {', '.join(DEFAULT_QUALITY_ORDER)})"
        ),
    )
    auto_convert_parser.add_argument(
        "--profile-map",
        type=_existing_file,
        help=f"Optional shader profile map (default: {DEFAULT_PROFILE_MAP})",
    )
    auto_convert_parser.add_argument(
        "--effect-root",
        type=_path,
        help="Optional root directory to validate effect references",
    )
    auto_convert_parser.add_argument(
        "--strict-effects",
        action="store_true",
        help="Treat missing effect references as errors",
    )
    auto_convert_parser.set_defaults(func=_run_auto_update_shaders)

    upgrade_hlsl_parser = subparsers.add_parser(
        "upgrade-hlsl",
        help="Upgrade HLSL vertex shader headers in clientHLSL files",
    )
    upgrade_hlsl_parser.add_argument(
        "--input-dir",
        type=_existing_directory,
        default=DEFAULT_INPUT_DIR,
        help="Directory containing source .vsh files (default: clientHLSL files)",
    )
    upgrade_hlsl_parser.add_argument(
        "--output-dir",
        type=_path,
        help="Optional output directory for upgraded shaders (defaults to in-place)",
    )
    upgrade_hlsl_parser.add_argument(
        "--target",
        default=DEFAULT_TARGET_PROFILE,
        help="Shader model profile to add or replace (default: vs_4_0)",
    )
    upgrade_hlsl_parser.add_argument(
        "--mode",
        choices=("add", "replace", "smart"),
        default="add",
        help=(
            "Whether to add the target profile, replace existing profiles, or"
            " smart-upgrade legacy profiles (default: add)"
        ),
    )
    upgrade_hlsl_parser.add_argument(
        "--pattern",
        default=DEFAULT_PATTERN,
        help="Glob pattern for shader files (default: *.vsh)",
    )
    upgrade_hlsl_parser.add_argument(
        "--workers",
        type=int,
        default=None,
        help="Number of parallel workers to use (default: CPU count)",
    )
    upgrade_hlsl_parser.add_argument(
        "--gpu",
        action="store_true",
        help="Enable GPU-assisted header scanning when torch + CUDA/ROCm is available",
    )
    upgrade_hlsl_parser.add_argument(
        "--allow-unsupported",
        action="store_true",
        help="Allow targets above DX9-supported profiles",
    )
    upgrade_hlsl_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Scan and report without writing changes",
    )
    upgrade_hlsl_parser.set_defaults(func=_run_upgrade_hlsl)

    publish_parser = subparsers.add_parser(
        "publish", help="Create a versioned publish bundle"
    )
    publish_parser.add_argument(
        "--content",
        required=True,
        type=_existing_directory,
        help="Directory containing validated content to publish",
    )
    publish_parser.add_argument(
        "--destination",
        required=True,
        type=_path,
        help="Directory where the publish bundle should be written",
    )
    publish_parser.add_argument(
        "--manifest",
        type=_existing_file,
        help="Optional manifest JSON describing the bundle",
    )
    publish_parser.add_argument(
        "--label",
        type=str,
        default="snapshot",
        help="Human-readable label for the release",
    )
    publish_parser.set_defaults(func=_run_publish)

    config_parser = subparsers.add_parser(
        "generate-rsp-config",
        help="Create a configuration file for TreeFileRspBuilder_r",
    )
    config_parser.add_argument(
        "--output",
        type=_path,
        default=Path("TreeFileRspBuilder.cfg"),
        help="Destination for the generated configuration file",
    )
    config_parser.add_argument(
        "--entry",
        dest="entries",
        metavar="COMMENT=PATH",
        action="append",
        required=True,
        help=(
            "searchPath entry to include. Prefix the value with an optional "
            "comment followed by '=' (for example, --entry "
            "'Root data=C:/swg/live/data')."
        ),
    )
    config_parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Allow searchPath entries that do not currently exist",
    )
    config_parser.add_argument(
        "--no-header",
        action="store_true",
        help="Omit the autogenerated header comments",
    )
    config_parser.set_defaults(func=_run_generate_rsp_config)

    response_parser = subparsers.add_parser(
        "generate-response",
        help="Create a TreeFileBuilder response (.rsp) file from source paths",
    )
    response_parser.add_argument(
        "--destination",
        required=True,
        type=_path,
        help="File path where the generated response should be written",
    )
    response_parser.add_argument(
        "--source",
        dest="sources",
        action="append",
        type=_existing_path,
        help=(
            "File or directory to include in the generated response file. "
            "Provide multiple times to include additional paths."
        ),
    )
    response_parser.add_argument(
        "--entry-root",
        type=_existing_directory,
        help=(
            "Optional root directory used to compute tree entry names when generating a "
            "response file. Defaults to the common ancestor of all --source paths."
        ),
    )
    response_parser.add_argument(
        "--allow-overrides",
        action="store_true",
        help=(
            "Allow later --source paths to override earlier entries when duplicate tree "
            "paths are encountered."
        ),
    )
    response_parser.set_defaults(func=_run_generate_response)

    tree_parser = subparsers.add_parser(
        "build-tree", help="Build a .tre/.tres/.tresx archive using TreeFileBuilder"
    )
    tree_parser.add_argument(
        "--response",
        type=_path,
        help=(
            "Response (.rsp) file describing the tree contents. When used with --source, "
            "the response file will be generated at this location."
        ),
    )
    tree_parser.add_argument(
        "--source",
        dest="sources",
        action="append",
        type=_existing_path,
        help=(
            "File or directory to include when generating a response file. "
            "Provide multiple times to include additional paths."
        ),
    )
    tree_parser.add_argument(
        "--entry-root",
        type=_existing_directory,
        help=(
            "Optional root directory used to compute tree entry names when generating a "
            "response file. Defaults to the common ancestor of all --source paths."
        ),
    )
    tree_parser.add_argument(
        "--allow-overrides",
        action="store_true",
        help=(
            "Allow later --source paths to override earlier entries when duplicate tree "
            "paths are encountered."
        ),
    )
    tree_parser.add_argument(
        "--output",
        required=True,
        type=_path,
        help="Destination tree archive (.tre, .tres, or .tresx)",
    )
    tree_parser.add_argument(
        "--builder",
        type=_path,
        help="Optional path to the TreeFileBuilder executable",
    )
    tree_parser.add_argument(
        "--internal-builder",
        action="store_true",
        help="Force the internal TreeFileBuilder implementation",
    )
    tree_parser.add_argument(
        "--no-toc-compression",
        action="store_true",
        help="Disable compression of the tree table of contents",
    )
    tree_parser.add_argument(
        "--no-file-compression",
        action="store_true",
        help="Disable compression of file payloads",
    )
    tree_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Perform a dry run without writing the archive",
    )
    tree_parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress verbose TreeFileBuilder output",
    )
    tree_parser.add_argument(
        "--encrypt",
        action="store_true",
        help="Force encryption to produce a .tres or .tresx archive",
    )
    tree_parser.add_argument(
        "--no-encrypt",
        action="store_true",
        help="Disable encryption even if the response requests it",
    )
    tree_parser.add_argument(
        "--passphrase",
        help="Optional passphrase for .tres or .tresx encryption",
    )
    tree_parser.add_argument(
        "--gpu",
        action="store_true",
        help="Request GPU acceleration from TreeFileBuilder when supported",
    )
    tree_parser.add_argument(
        "--debug",
        action="store_true",
        help="Emit detailed TreeFileBuilder diagnostics",
    )
    tree_parser.set_defaults(func=_run_build_tree)

    create_tree_parser = subparsers.add_parser(
        "create-tree-file",
        help="Create an encrypted .tres/.tresx archive from source paths",
    )
    create_tree_parser.add_argument(
        "--source",
        dest="sources",
        action="append",
        required=True,
        type=_existing_path,
        help=(
            "File or directory to include in the generated .tres/.tresx file. "
            "Provide multiple times to include additional paths."
        ),
    )
    create_tree_parser.add_argument(
        "--output",
        required=True,
        type=_path,
        help=(
            "Destination path for the encrypted tree archive "
            "(default extension: .tres)"
        ),
    )
    create_tree_parser.add_argument(
        "--passphrase",
        help="Optional passphrase used to encrypt the .tres or .tresx archive",
    )
    create_tree_parser.add_argument(
        "--encrypt",
        action="store_true",
        help="Deprecated: .tres or .tresx extension is always enforced for encrypted output",
    )
    create_tree_parser.add_argument(
        "--builder",
        type=_path,
        help="Optional path to the TreeFileBuilder executable",
    )
    create_tree_parser.add_argument(
        "--internal-builder",
        action="store_true",
        help="Force the internal TreeFileBuilder implementation",
    )
    create_tree_parser.add_argument(
        "--smart",
        action="store_true",
        help="Run a preflight scan and emit compression/encryption guidance",
    )
    create_tree_parser.add_argument(
        "--gpu",
        action="store_true",
        help=(
            "Use GPU-assisted hashing for preflight when CUDA/ROCm is available and "
            "request GPU acceleration from TreeFileBuilder when supported"
        ),
    )
    create_tree_parser.add_argument(
        "--debug",
        action="store_true",
        help="Emit detailed TreeFileBuilder diagnostics",
    )
    create_tree_parser.set_defaults(func=_run_create_tree_file)

    tree_gui_parser = subparsers.add_parser(
        "treefile-gui", help="Launch the TreeFileBuilder GUI"
    )
    tree_gui_parser.set_defaults(func=_run_treefile_gui)

    treefile_parser = subparsers.add_parser(
        "treefile", help="List or extract entries from .tre/.tres/.tresx archives"
    )
    treefile_subparsers = treefile_parser.add_subparsers(
        dest="treefile_action", required=True
    )

    treefile_list_parser = treefile_subparsers.add_parser(
        "list", help="List contents of a tree file"
    )
    treefile_list_parser.add_argument(
        "treefile",
        type=_existing_file,
        help="Tree file to inspect (.tre, .tres, .tresx)",
    )
    treefile_list_parser.add_argument(
        "--passphrase",
        help="Passphrase for encrypted .tres/.tresx archives",
    )
    treefile_list_parser.add_argument(
        "--extractor",
        type=_path,
        help="Optional path to TreeFileExtractor.exe",
    )
    treefile_list_parser.set_defaults(func=_run_treefile_list)

    treefile_extract_parser = treefile_subparsers.add_parser(
        "extract", help="Extract entries from a tree file"
    )
    treefile_extract_parser.add_argument(
        "treefile",
        type=_existing_file,
        help="Tree file to extract (.tre, .tres, .tresx)",
    )
    treefile_extract_parser.add_argument(
        "--output",
        type=_path,
        required=True,
        help="Directory to write extracted files",
    )
    treefile_extract_parser.add_argument(
        "--entry",
        action="append",
        default=[],
        help="Specific entry to extract (repeatable). Defaults to all entries.",
    )
    treefile_extract_parser.add_argument(
        "--passphrase",
        help="Passphrase for encrypted .tres/.tresx archives",
    )
    treefile_extract_parser.add_argument(
        "--extractor",
        type=_path,
        help="Optional path to TreeFileExtractor.exe",
    )
    treefile_extract_parser.set_defaults(func=_run_treefile_extract)

    return parser


def _run_validate(args: argparse.Namespace) -> None:
    validator = ManifestValidator()
    errors = []
    for manifest in args.manifests:
        errors.extend(validator.validate_manifest(manifest))

    if errors:
        for error in errors:
            print(f"[ERROR] {error}")
        raise SystemExit(1)

    print(f"Validated {len(args.manifests)} manifest(s) successfully.")


def _run_generate_navmesh(args: argparse.Namespace) -> None:
    generator = NavMeshGenerator(walkable_threshold=args.walkable_threshold)
    navmesh = generator.generate_from_heightmap(args.terrain)
    _ensure_directory(args.output.parent)
    args.output.write_text(json.dumps(navmesh, indent=2))
    print(f"Navmesh written to {args.output}")


def _run_loadtest(args: argparse.Namespace) -> None:
    try:
        scenario = LoadTestScenario.from_manifest(args.manifest)
    except ValueError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    log_directory = _ensure_directory(args.logs)
    report = asyncio.run(run_loadtest(scenario, log_directory))

    _ensure_directory(args.report.parent)
    args.report.write_text(json.dumps(report, indent=2))
    print(f"Load test report written to {args.report}")

    if args.stream_report:
        print(json.dumps(report))


def _run_create_iff(args: argparse.Namespace) -> None:
    try:
        definition = json.loads(args.definition.read_text())
    except json.JSONDecodeError as exc:
        print(f"[ERROR] Failed to parse definition JSON: {exc}")
        raise SystemExit(1) from exc

    try:
        builder = IffBuilder.from_definition(definition)
        payload = builder.build_bytes()
    except IffDefinitionError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    _ensure_directory(args.output.parent)
    args.output.write_bytes(payload)
    print(f"IFF written to {args.output} ({len(payload)} bytes)")

    if args.preview:
        print(builder.describe())


def _run_create_shader(args: argparse.Namespace) -> None:
    textures = args.textures or []
    texture_dirs = args.texture_dirs or []

    if not textures and not texture_dirs:
        print("[ERROR] At least one --texture or --texture-dir is required")
        raise SystemExit(1)

    if args.output and texture_dirs:
        print("[ERROR] --output cannot be used with --texture-dir")
        raise SystemExit(1)

    if args.output and len(textures) > 1:
        print("[ERROR] --output can only be used with a single --texture entry")
        raise SystemExit(1)

    output_dir = _ensure_directory(args.output_dir)
    effect_root = args.effect_root or (Path("effect") if args.strict_effects else None)
    try:
        profile_map = load_shader_profiles(args.profile_map)
    except ShaderTemplateError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    for texture in textures:
        texture_name = texture.strip()
        if not texture_name:
            print("[ERROR] Texture name cannot be empty")
            raise SystemExit(1)

        normalized, normalization_notes = normalize_texture_reference(texture_name)
        suffix = Path(normalized).suffix.lower()
        if suffix not in {".dds", ".png", ".pgn"} and not args.allow_non_dds:
            print(
                "[ERROR] Texture must use a supported extension "
                f"(.dds, .png, .pgn): {texture_name}"
            )
            raise SystemExit(1)

        if args.output:
            output_path = args.output
        else:
            stem = Path(normalized).stem
            output_path = output_dir / f"{stem}.sht"

        selection = resolve_shader_profile(texture_name, args.quality, profile_map)
        effect_name = args.effect
        material = DEFAULT_MATERIAL
        profile_note = ""
        if selection:
            effect_name = selection.tier.effect_name
            material = selection.tier.material
            profile_note = f" profile={selection.profile.name}({selection.tier.quality})"
        try:
            effect_name, effect_notes = validate_effect_name(
                effect_name,
                effect_root=effect_root,
                strict_effects=args.strict_effects,
            )
        except ShaderTemplateError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc

        request = ShaderTemplateRequest(
            texture_name=normalized,
            output_path=output_path,
            effect_name=effect_name,
            material=material,
        )

        try:
            write_shader_template(request)
        except ShaderTemplateError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc

        print(
            "Shader template written to",
            output_path,
            f"(texture={normalized}, effect={effect_name}{profile_note})",
        )
        for note in normalization_notes:
            print(f"[NOTE] {note}")
        for note in effect_notes:
            print(f"[WARNING] {note}")

    if texture_dirs:
        summary = _run_create_shader_from_dirs(
            texture_dirs,
            output_dir,
            profile_map,
            args.quality,
            args.pattern,
            args.force,
            args.allow_non_dds,
            effect_root,
            args.strict_effects,
        )
        for item in sorted(summary.items, key=lambda entry: str(entry.texture_path)):
            if item.created:
                print(f"Created {item.texture_path} -> {item.output}")
                for note in item.notes:
                    print(f"  [WARNING] {note}")
            else:
                reason = f" ({item.skipped_reason})" if item.skipped_reason else ""
                print(f"Skipped {item.texture_path}{reason}")

        print(
            "Batch creation summary:",
            f"{summary.created} created,",
            f"{summary.skipped} skipped,",
            f"{len(summary.errors)} errors.",
        )

        if summary.errors:
            for error in summary.errors:
                print(f"[ERROR] {error}")
            raise SystemExit(1)


def _run_create_shader_from_dirs(
    texture_dirs: Iterable[Path],
    output_dir: Path,
    profile_map: ShaderProfileMap,
    quality: str,
    pattern: str,
    force: bool,
    allow_non_dds: bool,
    effect_root: Optional[Path],
    strict_effects: bool,
) -> ShaderTemplateBatchSummary:
    all_items: list[ShaderTemplateBatchItem] = []
    all_errors: list[str] = []
    created = 0
    skipped = 0

    for texture_dir in texture_dirs:
        summary = create_shader_templates_from_dir(
            texture_dir,
            output_dir,
            profile_map,
            quality,
            pattern=pattern,
            force=force,
            allow_non_dds=allow_non_dds,
            effect_root=effect_root,
            strict_effects=strict_effects,
        )
        all_items.extend(summary.items)
        all_errors.extend(summary.errors)
        created += summary.created
        skipped += summary.skipped

    return ShaderTemplateBatchSummary(
        total=len(all_items) + len(all_errors),
        created=created,
        skipped=skipped,
        errors=tuple(all_errors),
        items=tuple(all_items),
    )


def _run_shader_gui(_: argparse.Namespace) -> None:
    try:
        launch_shader_gui()
    except ShaderTemplateError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc


def _run_convert_shader(args: argparse.Namespace) -> None:
    output_dir = _ensure_directory(args.output_dir)
    effect_root = args.effect_root or (Path("effect") if args.strict_effects else None)
    for source in args.inputs:
        target = output_dir / source.name
        try:
            result = convert_shader_template_with_report(
                source,
                target,
                effect_root=effect_root,
                strict_effects=args.strict_effects,
            )
        except ShaderTemplateError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc
        print(f"Converted {source} -> {result.output}")
        if result.notes:
            for note in result.notes:
                print(f"  - {note}")


def _run_auto_update_shaders(args: argparse.Namespace) -> None:
    output_dir = _ensure_directory(args.output_dir)
    effect_root = args.effect_root or (Path("effect") if args.strict_effects else None)
    profile_map = None
    if args.quality:
        profile_map = load_shader_profiles(args.profile_map)
    summary = convert_shader_templates_in_dir(
        args.input_dir,
        output_dir,
        pattern=args.pattern,
        max_workers=args.max_workers,
        quality=args.quality,
        profile_map=profile_map,
        effect_root=effect_root,
        strict_effects=args.strict_effects,
    )
    for item in sorted(summary.items, key=lambda entry: str(entry.source)):
        if item.converted:
            print(f"Converted {item.source} -> {item.output}")
            for note in item.notes:
                print(f"  - {note}")
        else:
            reason = f" ({item.skipped_reason})" if item.skipped_reason else ""
            print(f"Skipped {item.source}{reason}")

    print(
        "Batch conversion summary:",
        f"{summary.converted} converted,",
        f"{summary.skipped} skipped,",
        f"{summary.warnings} with warnings.",
    )

    if summary.errors:
        for error in summary.errors:
            print(f"[ERROR] {error}")
        raise SystemExit(1)


def _run_upgrade_hlsl(args: argparse.Namespace) -> None:
    output_dir = args.output_dir
    if output_dir is not None:
        output_dir = _ensure_directory(output_dir)

    workers = args.workers
    if workers is None:
        workers = os.cpu_count() or 1

    enforce_dx9 = not args.allow_unsupported
    if enforce_dx9 and args.target.strip().lower() in SUPPORTED_DX10_PROFILES:
        print(
            "[INFO] Direct3D 10 profile selected; skipping DX9 enforcement. "
            "Use --allow-unsupported to silence this message."
        )
        enforce_dx9 = False
    target_profile, target_note = normalize_target_profile(args.target, enforce_dx9)
    if target_note:
        print(f"[WARN] {target_note}")
        print(f"[WARN] Supported Direct3D 9 profiles: {', '.join(sorted(SUPPORTED_DX9_PROFILES))}.")

    summary = upgrade_hlsl_in_dir(
        args.input_dir,
        output_dir,
        target_profile=target_profile,
        mode=args.mode,
        pattern=args.pattern,
        workers=workers,
        use_gpu=args.gpu,
        dry_run=args.dry_run,
        enforce_dx9=enforce_dx9,
    )

    for result in iter_changed_results(summary):
        action = "Would upgrade" if args.dry_run else "Upgraded"
        print(f"{action} {result.path} -> {result.output_path}")
        if result.old_header and result.new_header:
            print(f"  - {result.old_header} -> {result.new_header}")

    print(
        "Upgrade summary:",
        f"{summary.changed_files} updated,",
        f"{summary.skipped_files} skipped,",
        f"{summary.total_files} scanned,",
        f"elapsed {summary.duration_s:.2f}s,",
        f"GPU: {summary.gpu_reason}.",
    )


def _run_publish(args: argparse.Namespace) -> None:
    publisher = Publisher()
    destination = _ensure_directory(args.destination)
    manifest: Optional[dict] = None
    if args.manifest:
        manifest = json.loads(args.manifest.read_text())

    try:
        result = publisher.publish(args.content, destination, manifest=manifest, label=args.label)
    except PublishError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    print(
        "Published bundle",
        result.version,
        "->",
        result.output_directory,
    )


def _run_terrain_plan(args: argparse.Namespace) -> None:
    document = TerrainDocument.load(args.document)
    config = load_config_from_args(vars(args))

    payload = run_headless(document, config)

    if args.output:
        args.output.write_text(json.dumps(payload, indent=2))
        print(f"Automation plan written to {args.output}")
    else:
        print(json.dumps(payload, indent=2))

    if args.serve:
        print(f"Serving automation plan on http://0.0.0.0:{args.serve}/plan")
        serve_headless(document, config, args.serve)


def _parse_config_entry(value: str) -> tuple[Optional[str], Path]:
    if "=" in value:
        comment, raw_path = value.split("=", 1)
        comment = comment.strip() or None
    else:
        comment = None
        raw_path = value

    path = Path(raw_path.strip()).expanduser()
    return comment, path


def _run_generate_rsp_config(args: argparse.Namespace) -> None:
    builder = TreeFileRspConfigBuilder(
        allow_missing_paths=args.allow_missing,
        include_header=not args.no_header,
    )

    parsed_entries = [_parse_config_entry(value) for value in args.entries]

    try:
        output_path = builder.write(args.output, parsed_entries)
    except TreeFileRspConfigBuilderError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    noun = "entry" if len(parsed_entries) == 1 else "entries"
    print(
        f"Configuration written to {output_path} with {len(parsed_entries)} {noun}."
    )


def _run_generate_response(args: argparse.Namespace) -> None:
    if not args.sources:
        print("[ERROR] At least one --source path must be provided.")
        raise SystemExit(2)

    builder_kwargs = {}
    if args.entry_root:
        builder_kwargs["entry_root"] = args.entry_root
    if args.allow_overrides:
        builder_kwargs["allow_overrides"] = True

    try:
        builder = ResponseFileBuilder(**builder_kwargs)
        result = builder.write(destination=args.destination, sources=args.sources)
    except ResponseFileBuilderError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc

    entry_count = len(result.entries)
    noun = "entry" if entry_count == 1 else "entries"
    print(
        f"Response file written to {result.path} with {entry_count} {noun}."
    )


def _run_build_tree(args: argparse.Namespace) -> None:
    try:
        builder = TreeFileBuilder(
            executable=args.builder,
            force_internal=bool(args.internal_builder),
        )
    except TreeFileBuilderError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc
    response_path = args.response
    temp_dir: Optional[tempfile.TemporaryDirectory] = None
    encrypt_output = _should_encrypt_tree(args)
    output_path = args.output

    passphrase = None
    if encrypt_output:
        passphrase = (args.passphrase or "").strip() or None
        if not builder.capabilities.supports_encrypt:
            print(
                "[ERROR] This TreeFileBuilder executable does not support --encrypt. "
                "Upgrade the toolchain or disable encryption."
            )
            raise SystemExit(1)
        if passphrase and not builder.capabilities.supports_passphrase:
            print(
                "[ERROR] This TreeFileBuilder executable does not support --passphrase. "
                "Upgrade the toolchain or omit the passphrase."
            )
            raise SystemExit(1)
        output_path = _normalize_encrypted_output(output_path, allow_non_tres=False)
    elif args.passphrase is not None:
        print(
            "[ERROR] --passphrase requires encryption. "
            "Use --encrypt or a .tres or .tresx output."
        )
        raise SystemExit(2)

    try:
        if args.sources:
            response_builder_kwargs = {}
            if args.entry_root:
                response_builder_kwargs["entry_root"] = args.entry_root
            if args.allow_overrides:
                response_builder_kwargs["allow_overrides"] = True

            response_builder = ResponseFileBuilder(**response_builder_kwargs)

            if response_path is None:
                temp_dir = tempfile.TemporaryDirectory()
                response_path = Path(temp_dir.name) / "tree_contents.rsp"

            response_result = response_builder.write(
                destination=response_path, sources=args.sources
            )
            response_path = response_result.path

            entry_count = len(response_result.entries)
            noun = "entry" if entry_count == 1 else "entries"
            print(
                f"Generated response file with {entry_count} {noun} at {response_result.path}"
            )
        elif response_path is None:
            print("[ERROR] --response is required when no --source paths are provided.")
            raise SystemExit(2)

        if args.debug:
            for line in _builder_debug_lines(builder):
                print(line)
            print(f"[debug] Response file: {response_path}")
            print(f"[debug] Output file: {output_path}")
            print(f"[debug] Encrypt output: {encrypt_output}")
            print(f"[debug] No TOC compression: {args.no_toc_compression}")
            print(f"[debug] No file compression: {args.no_file_compression}")
            print(f"[debug] Dry run: {args.dry_run}")
            print(f"[debug] Quiet: {args.quiet}")
            print(f"[debug] GPU: {args.gpu}")
            passphrase_preview = "<redacted>" if passphrase else None
            preview_command = builder.build_command(
                response_file=response_path,
                output_file=output_path,
                no_toc_compression=args.no_toc_compression,
                no_file_compression=args.no_file_compression,
                dry_run=args.dry_run,
                quiet=args.quiet,
                force_encrypt=encrypt_output,
                disable_encrypt=args.no_encrypt,
                passphrase=passphrase_preview,
                use_gpu=args.gpu,
            )
            print(f"[debug] Command: {' '.join(preview_command)}")

        stdout_callback = None
        stderr_callback = None
        suppress_output = bool(args.quiet)
        if not suppress_output:
            def _forward_stdout(chunk: str) -> None:
                sys.stdout.write(chunk)
                sys.stdout.flush()

            def _forward_stderr(chunk: str) -> None:
                sys.stderr.write(chunk)
                sys.stderr.flush()

            stdout_callback = _forward_stdout
            stderr_callback = _forward_stderr

        result = builder.build(
            response_file=response_path,
            output_file=output_path,
            no_toc_compression=args.no_toc_compression,
            no_file_compression=args.no_file_compression,
            dry_run=args.dry_run,
            quiet=args.quiet,
            force_encrypt=encrypt_output,
            disable_encrypt=args.no_encrypt,
            passphrase=passphrase,
            use_gpu=args.gpu,
            stdout_callback=stdout_callback,
            stderr_callback=stderr_callback,
        )
    except ResponseFileBuilderError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc
    except TreeFileBuilderError as exc:
        print(f"[ERROR] {exc}")
        if exc.result and not suppress_output:
            if exc.result.stdout and stdout_callback is None:
                print(exc.result.stdout)
            if exc.result.stderr and stderr_callback is None:
                print(exc.result.stderr)
        raise SystemExit(1) from exc
    finally:
        if temp_dir:
            temp_dir.cleanup()

    if not suppress_output:
        if result.stdout and stdout_callback is None:
            print(result.stdout)
        if result.stderr and stderr_callback is None:
            print(result.stderr)

    print(f"Tree file written to {result.output}")


def _run_create_tree_file(args: argparse.Namespace) -> None:
    output_path = _normalize_encrypted_output(args.output, allow_non_tres=False)
    passphrase = args.passphrase.strip() if args.passphrase else None

    run_preflight = bool(args.smart or args.gpu)
    if run_preflight:
        try:
            report = run_tree_preflight(
                args.sources,
                use_gpu=args.gpu,
                encrypting=True,
            )
        except ResponseFileBuilderError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc
        for line in format_preflight_report(report):
            print(line)

    try:
        builder = TreeFileBuilder(
            executable=args.builder,
            force_internal=bool(args.internal_builder),
        )
    except TreeFileBuilderError as exc:
        print(f"[ERROR] {exc}")
        raise SystemExit(1) from exc
    if not builder.capabilities.supports_encrypt:
        print(
            "[ERROR] This TreeFileBuilder executable does not support --encrypt. "
            "Upgrade the toolchain to build .tres or .tresx files."
        )
        raise SystemExit(1)
    if passphrase and not builder.capabilities.supports_passphrase:
        print(
            "[ERROR] This TreeFileBuilder executable does not support --passphrase. "
            "Upgrade the toolchain to supply a passphrase."
        )
        raise SystemExit(1)

    if args.debug:
        for line in _builder_debug_lines(builder):
            print(line)
        print(f"[debug] Output file: {output_path}")
        print("[debug] Encrypt output: True")
        print(f"[debug] Passphrase set: {'yes' if passphrase else 'no'}")
        print(f"[debug] GPU: {args.gpu}")
    with tempfile.TemporaryDirectory() as temp_dir:
        response_path = Path(temp_dir) / "tree_contents.rsp"

        try:
            response_builder = ResponseFileBuilder()
            response_result = response_builder.write(
                destination=response_path, sources=args.sources
            )
            entry_count = len(response_result.entries)
            noun = "entry" if entry_count == 1 else "entries"
            print(
                f"Generated response file with {entry_count} {noun} at {response_result.path}"
            )
            if args.debug:
                passphrase_preview = "<redacted>" if passphrase else None
                preview_command = builder.build_command(
                    response_file=response_result.path,
                    output_file=output_path,
                    force_encrypt=True,
                    passphrase=passphrase_preview,
                    use_gpu=args.gpu,
                )
                print(f"[debug] Response file: {response_result.path}")
                print(f"[debug] Command: {' '.join(preview_command)}")
            result = builder.build(
                response_file=response_result.path,
                output_file=output_path,
                force_encrypt=True,
                passphrase=passphrase,
                use_gpu=args.gpu,
            )
        except ResponseFileBuilderError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc
        except TreeFileBuilderError as exc:
            print(f"[ERROR] {exc}")
            raise SystemExit(1) from exc

    print(f"Encrypted tree file written to {result.output}")


def _run_treefile_gui(_args: argparse.Namespace) -> None:
    launch_treefile_gui()


def _run_treefile_list(args: argparse.Namespace) -> None:
    try:
        result = list_treefile(
            args.treefile,
            extractor=args.extractor,
            passphrase=args.passphrase,
        )
    except TreeFileExtractorError as exc:
        print(f"[ERROR] {exc}")
        if exc.stderr:
            print(exc.stderr.strip())
        raise SystemExit(1) from exc

    for entry in result.entries:
        if entry.offset is None:
            print(entry.path)
        else:
            print(f"{entry.path}\t{entry.offset}")


def _run_treefile_extract(args: argparse.Namespace) -> None:
    try:
        result = extract_treefile(
            args.treefile,
            args.output,
            extractor=args.extractor,
            passphrase=args.passphrase,
            entries=args.entry,
        )
    except TreeFileExtractorError as exc:
        print(f"[ERROR] {exc}")
        if exc.stderr:
            print(exc.stderr.strip())
        raise SystemExit(1) from exc

    extracted_count = len(result.extracted_paths)
    print(f"Extracted {extracted_count} file(s) to {result.output_dir}")


def main(argv: Optional[Iterable[str]] = None) -> None:
    parser = build_parser()
    if argv is None:
        argv = sys.argv[1:]
    argv_list = list(argv)
    if not argv_list:
        launch_shader_gui()
        return
    args = parser.parse_args(argv_list)
    args.func(args)


__all__ = ["build_parser", "main"]
