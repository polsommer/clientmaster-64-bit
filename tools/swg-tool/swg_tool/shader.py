"""Helpers for generating SWG shader template (.sht) files."""

from __future__ import annotations

import json
import re
import struct
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from fnmatch import fnmatch
from pathlib import Path
from typing import Iterable, Optional

from .hlsl_upgrade import (
    DEFAULT_INPUT_DIR,
    DEFAULT_PATTERN,
    DEFAULT_TARGET_PROFILE,
    normalize_target_profile,
    SUPPORTED_DX9_PROFILES,
    SUPPORTED_DX10_PROFILES,
    upgrade_hlsl_in_dir,
)

DEFAULT_EFFECT = "effect\\simplemt1z.eft"
DEFAULT_TEXTURE_EXTENSION = ".dds"
SUPPORTED_TEXTURE_EXTENSIONS = frozenset({".dds", ".png", ".pgn"})
TEXTURE_EXTENSION_PRIORITY = (".dds", ".png", ".pgn")
DEFAULT_PROFILE_MAP = Path(__file__).with_name("shader_profiles.json")
DEFAULT_QUALITY_ORDER = ("low", "medium", "high")
DEFAULT_MATERIAL = (
    1.0,
    1.0,
    1.0,
    1.0,  # ambient
    1.0,
    1.0,
    1.0,
    1.0,  # diffuse
    0.0,
    0.0,
    0.0,
    0.0,  # emissive
    1.0,
    1.0,
    1.0,
    1.0,  # specular
    0.0,  # power
)


@dataclass(frozen=True)
class ShaderTemplateRequest:
    texture_name: str
    output_path: Path
    effect_name: str = DEFAULT_EFFECT
    material: Iterable[float] = DEFAULT_MATERIAL


class ShaderTemplateError(ValueError):
    """Raised when a shader template cannot be generated."""


@dataclass(frozen=True)
class ShaderTemplateInfo:
    texture_name: str
    effect_name: str


@dataclass(frozen=True)
class ShaderProfileTier:
    quality: str
    effect_name: str
    material: tuple[float, ...]


@dataclass(frozen=True)
class ShaderProfile:
    name: str
    patterns: tuple[str, ...]
    tiers: dict[str, ShaderProfileTier]


@dataclass(frozen=True)
class ShaderProfileMap:
    source: Path
    quality_order: tuple[str, ...]
    profiles: tuple[ShaderProfile, ...]


@dataclass(frozen=True)
class ShaderProfileSelection:
    profile: ShaderProfile
    tier: ShaderProfileTier


@dataclass(frozen=True)
class ShaderConversionResult:
    source: Path
    output: Path
    texture_name: str
    effect_name: str
    notes: tuple[str, ...]


@dataclass(frozen=True)
class ShaderBatchConversionItem:
    source: Path
    output: Path
    converted: bool
    notes: tuple[str, ...] = ()
    skipped_reason: Optional[str] = None


@dataclass(frozen=True)
class ShaderBatchConversionSummary:
    total: int
    converted: int
    skipped: int
    warnings: int
    errors: tuple[str, ...]
    items: tuple[ShaderBatchConversionItem, ...]


@dataclass(frozen=True)
class ShaderTemplateBatchItem:
    texture_path: Path
    output: Path
    created: bool
    notes: tuple[str, ...] = ()
    skipped_reason: Optional[str] = None


@dataclass(frozen=True)
class ShaderTemplateBatchSummary:
    total: int
    created: int
    skipped: int
    errors: tuple[str, ...]
    items: tuple[ShaderTemplateBatchItem, ...]


def build_shader_template_bytes(
    texture_name: str,
    effect_name: str = DEFAULT_EFFECT,
    material: Iterable[float] = DEFAULT_MATERIAL,
) -> bytes:
    material_bytes = struct.pack("<17f", *material)
    if len(material_bytes) != 68:
        raise ShaderTemplateError("Material payload must contain 17 floats")

    tag_main = _tag_to_int("MAIN")

    mats_form = _form(
        "MATS",
        [
            _form(
                "0000",
                [
                    _chunk("TAG ", struct.pack("<I", tag_main)),
                    _chunk("MATL", material_bytes),
                ],
            ),
        ],
    )

    txms_form = _form(
        "TXMS",
        [
            _form(
                "TXM ",
                [
                    _form(
                        "0000",
                        [
                            _chunk("DATA", struct.pack("<BI", 0, tag_main)),
                            _chunk("NAME", _string_payload(texture_name)),
                        ],
                    ),
                ],
            ),
        ],
    )

    tcss_form = _form(
        "TCSS",
        [
            _chunk("0000", struct.pack("<IB", tag_main, 0)),
        ],
    )

    inner_form = _form(
        "0000",
        [
            mats_form,
            txms_form,
            tcss_form,
            _chunk("NAME", _string_payload(effect_name)),
        ],
    )

    return _form("SSHT", [inner_form])


def write_shader_template(request: ShaderTemplateRequest) -> Path:
    payload = build_shader_template_bytes(
        request.texture_name,
        effect_name=request.effect_name,
        material=request.material,
    )
    request.output_path.parent.mkdir(parents=True, exist_ok=True)
    request.output_path.write_bytes(payload)
    return request.output_path


def read_shader_template_info(path: Path) -> ShaderTemplateInfo:
    payload = path.read_bytes()
    root = _parse_iff(payload)
    if root.tag != b"FORM" or root.name != b"SSHT":
        raise ShaderTemplateError(f"Unsupported shader format in {path}")

    version_form = _find_form(root, b"0000")
    if version_form is None:
        raise ShaderTemplateError(f"Shader template missing version form in {path}")

    texture_name = _find_texture_name(version_form)
    effect_name = _find_effect_name(version_form) or DEFAULT_EFFECT
    if not texture_name:
        raise ShaderTemplateError(f"Shader template missing texture name in {path}")

    return ShaderTemplateInfo(texture_name=texture_name, effect_name=effect_name)


def convert_shader_template(
    source_path: Path, output_path: Optional[Path] = None
) -> Path:
    result = convert_shader_template_with_report(source_path, output_path)
    return result.output


def convert_shader_template_with_report(
    source_path: Path,
    output_path: Optional[Path] = None,
    *,
    effect_root: Optional[Path] = None,
    strict_effects: bool = False,
) -> ShaderConversionResult:
    notes: list[str] = []
    try:
        info = read_shader_template_info(source_path)
        normalized_texture, notes = _normalize_texture_name(info.texture_name)
        normalized_effect = info.effect_name or DEFAULT_EFFECT
    except ShaderTemplateError as exc:
        if "missing texture name" not in str(exc).lower():
            raise
        inferred_texture, inferred_note = _infer_texture_name_from_context(source_path)
        normalized_texture, normalization_notes = _normalize_texture_name(
            inferred_texture
        )
        notes = [inferred_note, *normalization_notes]
        normalized_effect = (
            _read_shader_template_effect_name(source_path) or DEFAULT_EFFECT
        )
    normalized_effect, effect_notes = validate_effect_name(
        normalized_effect,
        effect_root=effect_root,
        strict_effects=strict_effects,
    )
    notes.extend(effect_notes)
    target = output_path or source_path
    request = ShaderTemplateRequest(
        texture_name=normalized_texture,
        output_path=target,
        effect_name=normalized_effect,
    )
    write_shader_template(request)
    return ShaderConversionResult(
        source=source_path,
        output=target,
        texture_name=normalized_texture,
        effect_name=normalized_effect,
        notes=tuple(notes),
    )


def convert_shader_template_with_profile(
    source_path: Path,
    output_path: Optional[Path] = None,
    *,
    quality: Optional[str] = None,
    profile_map: Optional[ShaderProfileMap] = None,
    effect_root: Optional[Path] = None,
    strict_effects: bool = False,
) -> ShaderConversionResult:
    notes: list[str] = []
    try:
        info = read_shader_template_info(source_path)
        normalized_texture, notes = _normalize_texture_name(info.texture_name)
        normalized_effect = info.effect_name or DEFAULT_EFFECT
        material = _read_shader_template_material(source_path)
    except ShaderTemplateError as exc:
        if "missing texture name" not in str(exc).lower():
            raise
        inferred_texture, inferred_note = _infer_texture_name_from_context(source_path)
        normalized_texture, normalization_notes = _normalize_texture_name(
            inferred_texture
        )
        notes = [inferred_note, *normalization_notes]
        normalized_effect = (
            _read_shader_template_effect_name(source_path) or DEFAULT_EFFECT
        )
        material = _read_shader_template_material(source_path)

    applied_effect = normalized_effect
    applied_material = material
    if quality and profile_map:
        selection = resolve_shader_profile(normalized_texture, quality, profile_map)
        if selection:
            applied_effect = selection.tier.effect_name
            applied_material = selection.tier.material
            notes.append(
                f"Applied profile {selection.profile.name} ({selection.tier.quality})"
            )
    applied_effect, effect_notes = validate_effect_name(
        applied_effect,
        effect_root=effect_root,
        strict_effects=strict_effects,
    )
    notes.extend(effect_notes)

    target = output_path or source_path
    request = ShaderTemplateRequest(
        texture_name=normalized_texture,
        output_path=target,
        effect_name=applied_effect,
        material=applied_material,
    )
    write_shader_template(request)
    return ShaderConversionResult(
        source=source_path,
        output=target,
        texture_name=normalized_texture,
        effect_name=applied_effect,
        notes=tuple(notes),
    )


def convert_shader_templates_in_dir(
    input_dir: Path,
    output_dir: Path,
    pattern: str = "*.sht",
    max_workers: Optional[int] = None,
    quality: Optional[str] = None,
    profile_map: Optional[ShaderProfileMap] = None,
    effect_root: Optional[Path] = None,
    strict_effects: bool = False,
) -> ShaderBatchConversionSummary:
    sources = sorted(path for path in input_dir.rglob(pattern) if path.is_file())
    items: list[ShaderBatchConversionItem] = []
    errors: list[str] = []
    output_dir.mkdir(parents=True, exist_ok=True)

    def _target_path(source: Path) -> Path:
        relative = source.relative_to(input_dir)
        return output_dir / relative

    def _should_skip(source: Path, target: Path) -> Optional[str]:
        if not target.exists():
            return None
        try:
            if target.stat().st_mtime >= source.stat().st_mtime:
                return "output is up to date"
        except OSError:
            return None
        return None

    work_items: list[tuple[Path, Path]] = []
    for source in sources:
        target = _target_path(source)
        reason = _should_skip(source, target)
        if reason:
            items.append(
                ShaderBatchConversionItem(
                    source=source,
                    output=target,
                    converted=False,
                    skipped_reason=reason,
                )
            )
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        work_items.append((source, target))

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(
                convert_shader_template_with_profile,
                source,
                target,
                quality=quality,
                profile_map=profile_map,
                effect_root=effect_root,
                strict_effects=strict_effects,
            ): source
            for source, target in work_items
        }
        for future in as_completed(futures):
            source = futures[future]
            try:
                result = future.result()
            except ShaderTemplateError as exc:  # pragma: no cover - surfaced via CLI
                message = str(exc)
                if "unsupported shader format" in message.lower():
                    items.append(
                        ShaderBatchConversionItem(
                            source=source,
                            output=_target_path(source),
                            converted=False,
                            skipped_reason="unsupported shader format",
                        )
                    )
                    continue
                errors.append(f"{source}: {exc}")
            except Exception as exc:  # pragma: no cover - surfaced via CLI
                errors.append(f"{source}: {exc}")
                continue
            items.append(
                ShaderBatchConversionItem(
                    source=result.source,
                    output=result.output,
                    converted=True,
                    notes=result.notes,
                )
            )

    converted = sum(1 for item in items if item.converted)
    skipped = sum(1 for item in items if not item.converted)
    warnings = sum(1 for item in items if item.notes)

    return ShaderBatchConversionSummary(
        total=len(items),
        converted=converted,
        skipped=skipped,
        warnings=warnings,
        errors=tuple(errors),
        items=tuple(items),
    )


def create_shader_templates_from_dir(
    texture_dir: Path,
    output_dir: Path,
    profile_map: ShaderProfileMap,
    quality: str,
    pattern: str = "*.*",
    force: bool = False,
    allow_non_dds: bool = False,
    default_effect: str = DEFAULT_EFFECT,
    default_material: Iterable[float] = DEFAULT_MATERIAL,
    effect_root: Optional[Path] = None,
    strict_effects: bool = False,
) -> ShaderTemplateBatchSummary:
    sources = sorted(path for path in texture_dir.rglob(pattern) if path.is_file())
    items: list[ShaderTemplateBatchItem] = []
    errors: list[str] = []
    output_dir.mkdir(parents=True, exist_ok=True)

    for source in sources:
        try:
            relative = source.relative_to(texture_dir)
        except ValueError:
            relative = Path(source.name)
        output_path = output_dir / relative.with_suffix(".sht")

        suffix = source.suffix.lower()
        if suffix not in SUPPORTED_TEXTURE_EXTENSIONS and not allow_non_dds:
            items.append(
                ShaderTemplateBatchItem(
                    texture_path=source,
                    output=output_path,
                    created=False,
                    skipped_reason=(
                        "unsupported texture extension "
                        f"{source.suffix or '(none)'}; expected .dds/.png/.pgn"
                    ),
                )
            )
            continue

        if output_path.exists() and not force:
            items.append(
                ShaderTemplateBatchItem(
                    texture_path=source,
                    output=output_path,
                    created=False,
                    skipped_reason="output already exists",
                )
            )
            continue

        texture_name = relative.as_posix()
        selection = resolve_shader_profile(texture_name, quality, profile_map)
        effect_name = default_effect
        material = default_material
        if selection:
            effect_name = selection.tier.effect_name
            material = selection.tier.material
        try:
            effect_name, effect_notes = validate_effect_name(
                effect_name,
                effect_root=effect_root,
                strict_effects=strict_effects,
            )
            request = ShaderTemplateRequest(
                texture_name=texture_name,
                output_path=output_path,
                effect_name=effect_name,
                material=material,
            )
            write_shader_template(request)
        except ShaderTemplateError as exc:  # pragma: no cover - surfaced via CLI
            errors.append(f"{source}: {exc}")
            continue

        items.append(
            ShaderTemplateBatchItem(
                texture_path=source,
                output=output_path,
                created=True,
                notes=tuple(effect_notes),
            )
        )

    created = sum(1 for item in items if item.created)
    skipped = sum(1 for item in items if not item.created)

    return ShaderTemplateBatchSummary(
        total=len(items) + len(errors),
        created=created,
        skipped=skipped,
        errors=tuple(errors),
        items=tuple(items),
    )


def load_shader_profiles(path: Optional[Path] = None) -> ShaderProfileMap:
    source = path or DEFAULT_PROFILE_MAP
    quality_order = DEFAULT_QUALITY_ORDER
    if not source.exists():
        return ShaderProfileMap(source=source, quality_order=quality_order, profiles=())

    try:
        payload = json.loads(source.read_text())
    except json.JSONDecodeError as exc:
        raise ShaderTemplateError(f"Invalid shader profile map: {source}") from exc

    quality_order = tuple(payload.get("quality_order", quality_order))
    profiles: list[ShaderProfile] = []
    for entry in payload.get("profiles", []):
        name = entry.get("name", "Unnamed Profile")
        patterns_raw = entry.get("patterns", [])
        if isinstance(patterns_raw, str):
            patterns = (patterns_raw,)
        else:
            patterns = tuple(patterns_raw)
        tiers: dict[str, ShaderProfileTier] = {}
        for tier_name, tier_data in entry.get("tiers", {}).items():
            effect_name = tier_data.get("effect")
            if not effect_name:
                raise ShaderTemplateError(
                    f"Profile {name} tier {tier_name} missing effect"
                )
            material_data = tier_data.get("material")
            if material_data is None:
                material = tuple(DEFAULT_MATERIAL)
            else:
                material = tuple(float(value) for value in material_data)
                if len(material) != 17:
                    raise ShaderTemplateError(
                        f"Profile {name} tier {tier_name} must define 17 material values"
                    )
            tiers[tier_name.lower()] = ShaderProfileTier(
                quality=tier_name.lower(),
                effect_name=effect_name,
                material=material,
            )
        if patterns:
            profiles.append(
                ShaderProfile(
                    name=name,
                    patterns=tuple(pattern.lower() for pattern in patterns),
                    tiers=tiers,
                )
            )

    return ShaderProfileMap(
        source=source, quality_order=quality_order, profiles=tuple(profiles)
    )


def resolve_shader_profile(
    texture_name: str,
    quality: str,
    profile_map: ShaderProfileMap,
) -> Optional[ShaderProfileSelection]:
    normalized = texture_name.replace("\\", "/").lower()
    requested = quality.lower().strip()
    for profile in profile_map.profiles:
        if any(fnmatch(normalized, pattern) for pattern in profile.patterns):
            tier = _select_profile_tier(profile, requested, profile_map.quality_order)
            if tier is None:
                return None
            return ShaderProfileSelection(profile=profile, tier=tier)
    return None


def validate_effect_name(
    effect_name: str,
    *,
    effect_root: Optional[Path],
    strict_effects: bool,
) -> tuple[str, tuple[str, ...]]:
    if not effect_root:
        return effect_name, ()
    effect_root = Path(effect_root)
    normalized = Path(effect_name.replace("\\", "/"))
    candidates = []
    if normalized.is_absolute():
        candidates.append(normalized)
    else:
        candidates.append(effect_root / normalized)
        candidates.append(normalized)
    if any(candidate.exists() for candidate in candidates):
        return effect_name, ()
    if strict_effects:
        raise ShaderTemplateError(
            f"Effect '{effect_name}' not found under effect root '{effect_root}'."
        )
    note = (
        f"Effect '{effect_name}' not found under '{effect_root}'; "
        f"using default '{DEFAULT_EFFECT}'."
    )
    return DEFAULT_EFFECT, (note,)


def launch_shader_gui() -> None:
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox, simpledialog, ttk
    except ImportError as exc:  # pragma: no cover - depends on environment
        raise ShaderTemplateError(
            "tkinter is required to launch the shader GUI"
        ) from exc

    root = tk.Tk()
    root.title("SWG Shader Template Builder")
    root.resizable(False, False)

    style = ttk.Style(root)
    if "clam" in style.theme_names():
        style.theme_use("clam")

    texture_var = tk.StringVar()
    output_var = tk.StringVar(value=str(Path("shader")))
    effect_var = tk.StringVar(value=DEFAULT_EFFECT)
    profile_var = tk.StringVar()
    quality_var = tk.StringVar(value=DEFAULT_QUALITY_ORDER[-1])
    status_var = tk.StringVar(value="Select a texture and output location.")
    detail_var = tk.StringVar(
        value="Details will appear here once a texture is selected."
    )
    last_output_dir = Path("shader")
    profile_map = load_shader_profiles()

    def _update_output_from_texture(texture_path: str) -> None:
        nonlocal last_output_dir
        if not texture_path:
            return
        normalized = texture_path.replace("\\", "/")
        if Path(normalized).suffix:
            output_var.set(str(last_output_dir / f"{Path(normalized).stem}.sht"))

    def _format_details(
        texture_path: str,
        output_path: str,
        effect_name: str,
        quality: str,
        profile_label: str,
        material: Iterable[float],
    ) -> str:
        details = []
        normalized = texture_path.replace("\\", "/") if texture_path else ""
        output = Path(output_path) if output_path else Path()

        details.append("Shader Template Details")
        details.append("-" * 30)
        details.append(f"Texture: {normalized or 'Not set'}")
        details.append(f"Effect: {effect_name or DEFAULT_EFFECT}")
        details.append(f"Quality: {quality}")
        details.append(f"Profile: {profile_label}")
        details.append(f"Output: {output if output_path else 'Not set'}")

        if normalized:
            if Path(normalized).suffix.lower() in {".png", ".pgn"}:
                details.append(
                    "Note: DDS is the preferred texture format for runtime performance."
                )
            elif Path(normalized).suffix.lower() != ".dds":
                details.append(
                    "Warning: texture extension is not one of .dds/.png/.pgn."
                )

        if normalized:
            try:
                payload = build_shader_template_bytes(
                    normalized,
                    effect_name=effect_name or DEFAULT_EFFECT,
                )
                details.append(f"Estimated size: {len(payload)} bytes")
            except ShaderTemplateError as exc:
                details.append(f"Error: {exc}")

        details.append("")
        details.append("Material (ambient, diffuse, emissive, specular, power):")
        details.append(", ".join(f"{value:.2f}" for value in material))
        return "\n".join(details)

    def _refresh_details(*_args: object) -> None:
        selection = resolve_shader_profile(
            texture_var.get(),
            quality_var.get(),
            profile_map,
        )
        applied_effect = effect_var.get() or DEFAULT_EFFECT
        applied_profile = "None"
        material = DEFAULT_MATERIAL
        if selection:
            applied_effect = selection.tier.effect_name
            applied_profile = f"{selection.profile.name} ({selection.tier.quality})"
            material = selection.tier.material
        profile_var.set(applied_profile)
        detail_var.set(
            _format_details(
                texture_var.get(),
                output_var.get(),
                applied_effect,
                quality_var.get(),
                profile_var.get(),
                material,
            )
        )

    def browse_texture() -> None:
        path = filedialog.askopenfilename(
            title="Select Texture",
            filetypes=[
                ("Supported textures", "*.dds *.png *.pgn"),
                ("DDS textures", "*.dds"),
                ("PNG/PGN textures", "*.png *.pgn"),
                ("All files", "*.*"),
            ],
        )
        if path:
            texture_var.set(path)
            _update_output_from_texture(path)
            _refresh_details()

    def browse_output_dir() -> None:
        nonlocal last_output_dir
        path = filedialog.askdirectory(title="Select Output Directory")
        if path:
            last_output_dir = Path(path)
            output_var.set(path)
            _refresh_details()

    def choose_output_file() -> None:
        nonlocal last_output_dir
        path = filedialog.asksaveasfilename(
            title="Save Shader Template",
            defaultextension=".sht",
            filetypes=[("Shader templates", "*.sht"), ("All files", "*.*")],
        )
        if path:
            output_var.set(path)
            last_output_dir = Path(path).parent
            _refresh_details()

    def generate_shader() -> None:
        texture_path = texture_var.get().strip()
        output_path = output_var.get().strip()
        effect_name = effect_var.get().strip() or DEFAULT_EFFECT

        if not texture_path:
            messagebox.showerror("Missing texture", "Please select a texture file.")
            return

        normalized, normalization_notes = normalize_texture_reference(texture_path)
        suffix = Path(normalized).suffix.lower()
        if suffix not in SUPPORTED_TEXTURE_EXTENSIONS:
            messagebox.showerror(
                "Invalid texture",
                "Texture must have a supported extension (.dds, .png, .pgn).",
            )
            return

        output = Path(output_path)
        if output.suffix.lower() != ".sht":
            output = output / f"{Path(normalized).stem}.sht"

        selection = resolve_shader_profile(normalized, quality_var.get(), profile_map)
        material = DEFAULT_MATERIAL
        if selection:
            effect_name = selection.tier.effect_name
            material = selection.tier.material

        request = ShaderTemplateRequest(
            texture_name=normalized,
            output_path=output,
            effect_name=effect_name,
            material=material,
        )

        try:
            write_shader_template(request)
        except ShaderTemplateError as exc:
            messagebox.showerror("Shader creation failed", str(exc))
            return

        status_var.set(f"Wrote {output}")
        _refresh_details()
        details = ""
        if normalization_notes:
            details = "\n\nNotes:\n" + "\n".join(
                f"- {note}" for note in normalization_notes
            )
        messagebox.showinfo(
            "Shader created",
            f"Shader template written to:\n{output}{details}",
        )

    def convert_shader() -> None:
        source = filedialog.askopenfilename(
            title="Select Shader Template",
            filetypes=[("Shader templates", "*.sht"), ("All files", "*.*")],
        )
        if not source:
            return
        destination_dir = filedialog.askdirectory(title="Select Output Directory")
        if not destination_dir:
            return
        try:
            result = convert_shader_template_with_report(
                Path(source),
                Path(destination_dir) / Path(source).name,
            )
        except ShaderTemplateError as exc:
            messagebox.showerror("Conversion failed", str(exc))
            return

        status_var.set(f"Converted {result.output}")
        note_lines = (
            "\n".join(f"- {note}" for note in result.notes) if result.notes else "None"
        )
        messagebox.showinfo(
            "Conversion complete",
            (
                f"Converted shader written to:\n{result.output}\n\n"
                f"Texture: {result.texture_name}\n"
                f"Effect: {result.effect_name}\n"
                f"Notes:\n{note_lines}"
            ),
        )

    def upgrade_hlsl_headers() -> None:
        source_dir = filedialog.askdirectory(
            title="Select HLSL Source Directory",
            initialdir=str(DEFAULT_INPUT_DIR),
        )
        if not source_dir:
            return

        in_place = messagebox.askyesno(
            "Output Location",
            "Write upgraded headers in place?\n"
            "Choose 'No' to select a separate output directory.",
        )
        output_dir: Optional[Path]
        if in_place:
            output_dir = None
        else:
            destination_dir = filedialog.askdirectory(
                title="Select Output Directory",
            )
            if not destination_dir:
                return
            output_dir = Path(destination_dir)

        target_profile = simpledialog.askstring(
            "Target Profile",
            (
                "Enter the shader model profile to add or replace "
                "(e.g., vs_2_0, ps_3_0, vs_4_0, ps_4_0):"
            ),
            initialvalue=DEFAULT_TARGET_PROFILE,
        )
        if not target_profile:
            return

        mode = simpledialog.askstring(
            "Upgrade Mode",
            "Enter 'add' to append the profile or 'replace' to swap existing profiles:",
            initialvalue="add",
        )
        if not mode:
            return
        normalized_mode = mode.strip().lower()
        if normalized_mode not in {"add", "replace"}:
            messagebox.showerror("Invalid mode", "Mode must be 'add' or 'replace'.")
            return

        allow_unsupported = messagebox.askyesno(
            "DX9 Enforcement",
            "Allow shader profiles outside Direct3D 9 support?\n"
            "(Recommended for vs_4_0 / ps_4_0 targets.)",
        )
        enforce_dx9 = not allow_unsupported
        normalized_target, target_note = normalize_target_profile(
            target_profile, enforce_dx9
        )
        if target_note:
            messagebox.showinfo(
                "Profile adjusted for DX9",
                f"{target_note}\n"
                "Supported profiles: "
                f"{', '.join(sorted(SUPPORTED_DX9_PROFILES))} "
                f"(DX10 options: {', '.join(sorted(SUPPORTED_DX10_PROFILES))}).",
            )

        use_gpu = messagebox.askyesno(
            "GPU Acceleration",
            "Enable GPU-assisted header scanning (requires torch + CUDA/ROCm)?",
        )

        summary = upgrade_hlsl_in_dir(
            Path(source_dir),
            output_dir,
            target_profile=normalized_target,
            mode=normalized_mode,
            pattern=DEFAULT_PATTERN,
            workers=None,
            use_gpu=use_gpu,
            dry_run=False,
            enforce_dx9=enforce_dx9,
        )

        status_var.set(
            f"Upgraded {summary.changed_files} of {summary.total_files} HLSL files."
        )
        messagebox.showinfo(
            "HLSL Upgrade Complete",
            (
                f"Updated {summary.changed_files} files.\n"
                f"Skipped {summary.skipped_files} files.\n"
                f"GPU: {summary.gpu_reason}."
            ),
        )

    def batch_convert_shaders() -> None:
        source_dir = filedialog.askdirectory(
            title="Select Shader Template Directory",
        )
        if not source_dir:
            return

        destination_dir = filedialog.askdirectory(
            title="Select Output Directory",
        )
        if not destination_dir:
            return

        summary = convert_shader_templates_in_dir(
            Path(source_dir),
            Path(destination_dir),
            quality=quality_var.get(),
            profile_map=profile_map,
        )

        error_summary = ""
        if summary.errors:
            error_lines = "\n".join(f"- {error}" for error in summary.errors[:10])
            remaining = len(summary.errors) - 10
            if remaining > 0:
                error_lines = f"{error_lines}\n...and {remaining} more."
            error_summary = f"\nErrors:\n{error_lines}"

        status_var.set(
            f"Converted {summary.converted} of {summary.total} shader templates."
        )
        messagebox.showinfo(
            "Batch Conversion Complete",
            (
                f"Converted {summary.converted} files.\n"
                f"Skipped {summary.skipped} files.\n"
                f"Warnings: {summary.warnings}.\n"
                f"Errors: {len(summary.errors)}.{error_summary}"
            ),
        )

    frame = ttk.Frame(root, padding=12)
    frame.grid(row=0, column=0, sticky="nsew")

    header = ttk.Label(
        frame, text="Shader Template Wizard", font=("Segoe UI", 12, "bold")
    )
    header.grid(row=0, column=0, columnspan=3, pady=(0, 10), sticky="w")

    subtitle = ttk.Label(
        frame,
        text="Generate .sht files that reference DDS textures and effect templates.",
        foreground="#555555",
    )
    subtitle.grid(row=1, column=0, columnspan=3, pady=(0, 10), sticky="w")

    menu = tk.Menu(root)
    file_menu = tk.Menu(menu, tearoff=0)
    file_menu.add_command(label="Open DDS Texture...", command=browse_texture)
    file_menu.add_command(label="Choose Output Folder...", command=browse_output_dir)
    file_menu.add_command(label="Save Shader As...", command=choose_output_file)
    file_menu.add_command(label="Convert Shader...", command=convert_shader)
    file_menu.add_command(
        label="Batch Convert Shaders...", command=batch_convert_shaders
    )
    file_menu.add_command(
        label="Batch Upgrade HLSL Headers...", command=upgrade_hlsl_headers
    )
    file_menu.add_separator()
    file_menu.add_command(label="Exit", command=root.destroy)
    menu.add_cascade(label="File", menu=file_menu)

    help_menu = tk.Menu(menu, tearoff=0)
    help_menu.add_command(
        label="About",
        command=lambda: messagebox.showinfo(
            "About",
            "SWG Shader Template Builder\nCreates .sht files for DDS textures.",
        ),
    )
    menu.add_cascade(label="Help", menu=help_menu)
    root.config(menu=menu)

    ttk.Label(frame, text="DDS Texture").grid(row=2, column=0, sticky="w")
    texture_entry = ttk.Entry(frame, textvariable=texture_var, width=48)
    texture_entry.grid(row=3, column=0, columnspan=2, pady=(4, 8), sticky="we")
    ttk.Button(frame, text="Browse...", command=browse_texture).grid(
        row=3, column=2, padx=8
    )

    ttk.Label(frame, text="Output (folder or .sht file)").grid(
        row=4, column=0, sticky="w"
    )
    output_entry = ttk.Entry(frame, textvariable=output_var, width=48)
    output_entry.grid(row=5, column=0, columnspan=2, pady=(4, 8), sticky="we")
    ttk.Button(frame, text="Folder...", command=browse_output_dir).grid(
        row=5, column=2, padx=8
    )
    ttk.Button(frame, text="File...", command=choose_output_file).grid(
        row=6, column=2, padx=8, pady=(0, 8)
    )

    ttk.Label(frame, text="Effect").grid(row=6, column=0, sticky="w")
    ttk.Entry(frame, textvariable=effect_var, width=48).grid(
        row=7, column=0, columnspan=2, pady=(4, 12), sticky="we"
    )
    ttk.Label(frame, text="Quality Tier").grid(row=6, column=2, sticky="w")
    quality_choices = profile_map.quality_order or DEFAULT_QUALITY_ORDER
    ttk.Combobox(
        frame,
        textvariable=quality_var,
        values=list(quality_choices),
        width=10,
        state="readonly",
    ).grid(row=7, column=2, pady=(4, 12))

    ttk.Button(frame, text="Generate Shader", command=generate_shader).grid(
        row=8, column=0, columnspan=3, pady=(0, 8), sticky="we"
    )

    ttk.Label(frame, textvariable=status_var).grid(
        row=9, column=0, columnspan=3, sticky="w"
    )

    details_frame = ttk.LabelFrame(frame, text="Details", padding=10)
    details_frame.grid(row=10, column=0, columnspan=3, pady=(12, 0), sticky="we")
    detail_text = tk.Text(details_frame, width=64, height=10, wrap="word")
    detail_text.grid(row=0, column=0, sticky="we")
    detail_text.configure(state="disabled")

    def _sync_detail_text(*_args: object) -> None:
        detail_text.configure(state="normal")
        detail_text.delete("1.0", "end")
        detail_text.insert("1.0", detail_var.get())
        detail_text.configure(state="disabled")

    texture_var.trace_add("write", _refresh_details)
    output_var.trace_add("write", _refresh_details)
    effect_var.trace_add("write", _refresh_details)
    quality_var.trace_add("write", _refresh_details)
    detail_var.trace_add("write", _sync_detail_text)
    _refresh_details()

    root.mainloop()


def _tag_to_int(tag: str) -> int:
    if len(tag) != 4:
        raise ShaderTemplateError(f"Tag must be 4 characters: {tag}")
    return (ord(tag[0]) << 24) | (ord(tag[1]) << 16) | (ord(tag[2]) << 8) | ord(tag[3])


def _string_payload(value: str) -> bytes:
    return value.encode("utf-8") + b"\x00"


def _chunk(tag: str, data: bytes) -> bytes:
    if len(tag) != 4:
        raise ShaderTemplateError(f"Chunk tag must be 4 characters: {tag}")
    return tag.encode("ascii") + struct.pack(">I", len(data)) + data


def _form(tag: str, children: list[bytes]) -> bytes:
    if len(tag) != 4:
        raise ShaderTemplateError(f"Form tag must be 4 characters: {tag}")
    content = b"".join(children)
    size = 4 + len(content)
    return b"FORM" + struct.pack(">I", size) + tag.encode("ascii") + content


@dataclass(frozen=True)
class _IffNode:
    tag: bytes
    name: Optional[bytes]
    data: bytes
    children: list["_IffNode"]


def _parse_iff(payload: bytes) -> _IffNode:
    if len(payload) < 8:
        raise ShaderTemplateError("Shader template is too small to be valid IFF data")
    node, offset = _parse_iff_node(payload, 0)
    if offset != len(payload):
        raise ShaderTemplateError("Shader template contains trailing data")
    return node


def _parse_iff_node(payload: bytes, offset: int) -> tuple[_IffNode, int]:
    if offset + 8 > len(payload):
        raise ShaderTemplateError("Unexpected end of IFF data")
    tag = payload[offset : offset + 4]
    size = struct.unpack(">I", payload[offset + 4 : offset + 8])[0]
    start = offset + 8
    end = start + size
    if end > len(payload):
        raise ShaderTemplateError("IFF block extends beyond payload")

    if tag == b"FORM":
        if size < 4:
            raise ShaderTemplateError("FORM block is missing a name")
        name = payload[start : start + 4]
        children_payload = payload[start + 4 : end]
        children = []
        child_offset = 0
        while child_offset < len(children_payload):
            child, consumed = _parse_iff_node(children_payload, child_offset)
            children.append(child)
            child_offset = consumed
        if child_offset != len(children_payload):
            raise ShaderTemplateError("FORM children did not align to block size")
        return _IffNode(tag=tag, name=name, data=b"", children=children), end

    data = payload[start:end]
    return _IffNode(tag=tag, name=None, data=data, children=[]), end


def _find_form(node: _IffNode, name: bytes) -> Optional[_IffNode]:
    if node.tag == b"FORM" and node.name == name:
        return node
    for child in node.children:
        found = _find_form(child, name)
        if found is not None:
            return found
    return None


def _find_chunk(node: _IffNode, tag: bytes) -> Optional[_IffNode]:
    if node.tag == tag and node.name is None:
        return node
    for child in node.children:
        found = _find_chunk(child, tag)
        if found is not None:
            return found
    return None


def _find_all_chunks(node: _IffNode, tag: bytes) -> list[_IffNode]:
    matches = []
    if node.tag == tag and node.name is None:
        matches.append(node)
    for child in node.children:
        matches.extend(_find_all_chunks(child, tag))
    return matches


def _decode_chunk_string(node: _IffNode) -> str:
    if node.data.endswith(b"\x00"):
        return node.data[:-1].decode("utf-8")
    return node.data.decode("utf-8")


def _find_texture_name(node: _IffNode) -> Optional[str]:
    txms = _find_form(node, b"TXMS")
    if txms is None:
        return None
    txm = _find_form(txms, b"TXM ")
    if txm is None:
        return None
    txm_ver = _find_form(txm, b"0000")
    if txm_ver is None:
        return None
    name_chunk = _find_chunk(txm_ver, b"NAME")
    if name_chunk is None:
        return None
    return _decode_chunk_string(name_chunk)


def _find_effect_name(node: _IffNode) -> Optional[str]:
    name_chunks = _find_all_chunks(node, b"NAME")
    for chunk in name_chunks:
        value = _decode_chunk_string(chunk)
        if value.lower().endswith(".eft"):
            return value
    return None




def normalize_texture_reference(texture_name: str) -> tuple[str, tuple[str, ...]]:
    """Normalize a texture reference for shader templates."""
    normalized, notes = _normalize_texture_name(texture_name)
    return normalized, tuple(notes)


def _normalize_texture_name(texture_name: str) -> tuple[str, list[str]]:
    notes = []
    normalized = texture_name.replace("\\", "/")
    normalized_separators = normalized != texture_name
    if not normalized:
        return normalized, notes

    absolute_windows = bool(re.match(r"^[A-Za-z]:/", normalized))
    absolute_posix = normalized.startswith("/")
    if absolute_windows or absolute_posix:
        marker = "/texture/"
        lowered = normalized.lower()
        marker_index = lowered.find(marker)
        if marker_index >= 0:
            normalized = normalized[marker_index + 1 :]
            notes.append(
                "Converted absolute texture path to game-relative texture reference"
            )
        else:
            normalized = Path(normalized).name
            notes.append(
                "Converted absolute texture path to filename-only texture reference"
            )

    suffix = Path(normalized).suffix
    if not suffix:
        normalized = f"{normalized}{DEFAULT_TEXTURE_EXTENSION}"
        notes.append(f"Added missing {DEFAULT_TEXTURE_EXTENSION} extension")
    elif suffix.lower() not in SUPPORTED_TEXTURE_EXTENSIONS:
        notes.append(
            f"Preserved unsupported texture extension {suffix}; no normalization applied"
        )
    if normalized_separators:
        notes.append("Normalized texture path separators")
    return normalized, notes


def _infer_texture_name_from_context(
    source_path: Path, base_prefix: str = "texture"
) -> tuple[str, str]:
    stem = source_path.stem
    extension_priority = tuple(
        ext
        for ext in (
            *TEXTURE_EXTENSION_PRIORITY,
            *sorted(
                ext
                for ext in SUPPORTED_TEXTURE_EXTENSIONS
                if ext not in TEXTURE_EXTENSION_PRIORITY
            ),
        )
        if ext in SUPPORTED_TEXTURE_EXTENSIONS
    )
    extension_order = {ext: index for index, ext in enumerate(extension_priority)}
    matching_siblings = sorted(
        (
            path
            for path in source_path.parent.iterdir()
            if path.is_file()
            and path.stem.lower() == stem.lower()
            and path.suffix.lower() in SUPPORTED_TEXTURE_EXTENSIONS
        ),
        key=lambda path: (
            extension_order.get(path.suffix.lower(), len(extension_order)),
            path.name.lower(),
        ),
    )
    if matching_siblings:
        candidate = matching_siblings[0]
        return (
            candidate.name,
            f"Inferred texture name from sibling texture file: {candidate.name}",
        )
    normalized_prefix = base_prefix.rstrip("/\\")
    fallback_extension = (
        extension_priority[0] if extension_priority else DEFAULT_TEXTURE_EXTENSION
    )
    inferred = (
        f"{normalized_prefix}/{stem}{fallback_extension}"
        if normalized_prefix
        else f"{stem}{fallback_extension}"
    )
    return inferred, f"Inferred texture name from shader template stem: {inferred}"


def _read_shader_template_effect_name(source_path: Path) -> Optional[str]:
    payload = source_path.read_bytes()
    root = _parse_iff(payload)
    if root.tag != b"FORM" or root.name != b"SSHT":
        raise ShaderTemplateError(f"Unsupported shader format in {source_path}")

    version_form = _find_form(root, b"0000")
    if version_form is None:
        raise ShaderTemplateError(
            f"Shader template missing version form in {source_path}"
        )
    return _find_effect_name(version_form)


def _read_shader_template_material(source_path: Path) -> tuple[float, ...]:
    payload = source_path.read_bytes()
    root = _parse_iff(payload)
    if root.tag != b"FORM" or root.name != b"SSHT":
        raise ShaderTemplateError(f"Unsupported shader format in {source_path}")

    version_form = _find_form(root, b"0000")
    if version_form is None:
        raise ShaderTemplateError(
            f"Shader template missing version form in {source_path}"
        )

    mats_form = _find_form(version_form, b"MATS")
    if mats_form is None:
        return tuple(DEFAULT_MATERIAL)

    matl_chunk = _find_chunk(mats_form, b"MATL")
    if matl_chunk is None or not matl_chunk.data:
        return tuple(DEFAULT_MATERIAL)

    if len(matl_chunk.data) != 68:
        raise ShaderTemplateError(
            f"Shader material payload must contain 17 floats in {source_path}"
        )

    return tuple(struct.unpack("<17f", matl_chunk.data))


def _select_profile_tier(
    profile: ShaderProfile,
    requested: str,
    quality_order: tuple[str, ...],
) -> Optional[ShaderProfileTier]:
    if requested in profile.tiers:
        return profile.tiers[requested]
    if quality_order:
        requested_index = (
            quality_order.index(requested) if requested in quality_order else None
        )
        ordered = [quality for quality in quality_order if quality in profile.tiers]
        if ordered:
            if requested_index is not None:
                candidates = [
                    quality
                    for quality in ordered
                    if quality_order.index(quality) <= requested_index
                ]
                if candidates:
                    return profile.tiers[candidates[-1]]
            return profile.tiers[ordered[-1]]
    if profile.tiers:
        return next(iter(profile.tiers.values()))
    return None


__all__ = [
    "DEFAULT_EFFECT",
    "DEFAULT_PROFILE_MAP",
    "DEFAULT_QUALITY_ORDER",
    "DEFAULT_MATERIAL",
    "ShaderProfile",
    "ShaderProfileMap",
    "ShaderProfileSelection",
    "ShaderProfileTier",
    "ShaderTemplateError",
    "ShaderTemplateInfo",
    "ShaderTemplateRequest",
    "ShaderConversionResult",
    "ShaderBatchConversionItem",
    "ShaderBatchConversionSummary",
    "build_shader_template_bytes",
    "convert_shader_template",
    "convert_shader_template_with_profile",
    "convert_shader_template_with_report",
    "convert_shader_templates_in_dir",
    "load_shader_profiles",
    "launch_shader_gui",
    "read_shader_template_info",
    "resolve_shader_profile",
    "validate_effect_name",
    "write_shader_template",
]
