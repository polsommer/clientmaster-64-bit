from __future__ import annotations

import struct
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool.shader import (  # noqa: E402
    DEFAULT_EFFECT,
    build_shader_template_bytes,
    normalize_texture_reference,
    read_shader_template_info,
    convert_shader_template,
    convert_shader_template_with_report,
)


def test_build_shader_template_bytes_has_expected_header() -> None:
    payload = build_shader_template_bytes("texture/example.dds")

    assert payload[:4] == b"FORM"
    size = struct.unpack(">I", payload[4:8])[0]
    assert size + 8 == len(payload)
    assert payload[8:12] == b"SSHT"
    assert b"texture/example.dds\x00" in payload
    assert DEFAULT_EFFECT.encode("utf-8") + b"\x00" in payload


def test_reads_and_converts_shader_template(tmp_path: Path) -> None:
    source = tmp_path / "legacy.sht"
    source.write_bytes(build_shader_template_bytes("texture/legacy.dds"))

    info = read_shader_template_info(source)

    assert info.texture_name == "texture/legacy.dds"
    assert info.effect_name == DEFAULT_EFFECT

    output = tmp_path / "converted.sht"
    convert_shader_template(source, output)

    converted_info = read_shader_template_info(output)
    assert converted_info.texture_name == "texture/legacy.dds"


def test_conversion_normalizes_texture_name(tmp_path: Path) -> None:
    source = tmp_path / "legacy_no_ext.sht"
    source.write_bytes(build_shader_template_bytes("texture/legacy"))

    result = convert_shader_template_with_report(source, tmp_path / "converted.sht")

    assert result.texture_name == "texture/legacy.dds"
    assert "Added missing .dds extension" in result.notes


def test_conversion_preserves_png_texture_extension(tmp_path: Path) -> None:
    source = tmp_path / "legacy_png.sht"
    source.write_bytes(build_shader_template_bytes("texture/legacy.png"))

    result = convert_shader_template_with_report(source, tmp_path / "converted_png.sht")

    assert result.texture_name == "texture/legacy.png"
    assert "Added missing .dds extension" not in result.notes


def test_conversion_normalizes_absolute_windows_texture_path_to_texture_root(
    tmp_path: Path,
) -> None:
    source = tmp_path / "legacy_abs_png.sht"
    source.write_bytes(
        build_shader_template_bytes(
            "C:/Users/swg/Desktop/star wars galaxies/texture/ui_notice.png"
        )
    )

    result = convert_shader_template_with_report(
        source, tmp_path / "converted_abs_png.sht"
    )

    assert result.texture_name == "texture/ui_notice.png"
    assert (
        "Converted absolute texture path to game-relative texture reference"
        in result.notes
    )


def test_normalize_texture_reference_trims_absolute_path_to_texture_segment() -> None:
    normalized, notes = normalize_texture_reference(
        "C:/Users/swg/Desktop/star wars galaxies/texture/ui_notice.png"
    )

    assert normalized == "texture/ui_notice.png"
    assert "Converted absolute texture path to game-relative texture reference" in notes


def test_conversion_preserves_pgn_texture_extension(tmp_path: Path) -> None:
    source = tmp_path / "legacy_pgn.sht"
    source.write_bytes(build_shader_template_bytes("texture/legacy.pgn"))

    result = convert_shader_template_with_report(source, tmp_path / "converted_pgn.sht")

    assert result.texture_name == "texture/legacy.pgn"
    assert "Added missing .dds extension" not in result.notes


def test_conversion_preserves_unsupported_texture_extension_with_note(
    tmp_path: Path,
) -> None:
    source = tmp_path / "legacy_tga.sht"
    source.write_bytes(build_shader_template_bytes("texture/legacy.tga"))

    result = convert_shader_template_with_report(source, tmp_path / "converted_tga.sht")

    assert result.texture_name == "texture/legacy.tga"
    assert (
        "Preserved unsupported texture extension .tga; no normalization applied"
        in result.notes
    )


def test_conversion_infers_png_texture_from_sibling_when_name_missing(
    tmp_path: Path,
) -> None:
    source = tmp_path / "legacy_png_missing_name.sht"
    source.write_bytes(build_shader_template_bytes(""))
    (tmp_path / "legacy_png_missing_name.png").write_bytes(b"png")

    result = convert_shader_template_with_report(
        source, tmp_path / "converted_png_missing_name.sht"
    )

    assert result.texture_name == "legacy_png_missing_name.png"
    assert (
        "Inferred texture name from sibling texture file: legacy_png_missing_name.png"
        in result.notes
    )


def test_conversion_infers_pgn_texture_from_sibling_when_name_missing(
    tmp_path: Path,
) -> None:
    source = tmp_path / "legacy_pgn_missing_name.sht"
    source.write_bytes(build_shader_template_bytes(""))
    (tmp_path / "legacy_pgn_missing_name.pgn").write_bytes(b"pgn")

    result = convert_shader_template_with_report(
        source, tmp_path / "converted_pgn_missing_name.sht"
    )

    assert result.texture_name == "legacy_pgn_missing_name.pgn"
    assert (
        "Inferred texture name from sibling texture file: legacy_pgn_missing_name.pgn"
        in result.notes
    )


def test_conversion_infers_deterministic_texture_priority_from_mixed_siblings(
    tmp_path: Path,
) -> None:
    source = tmp_path / "legacy_mixed_missing_name.sht"
    source.write_bytes(build_shader_template_bytes(""))
    (tmp_path / "legacy_mixed_missing_name.png").write_bytes(b"png")
    (tmp_path / "legacy_mixed_missing_name.pgn").write_bytes(b"pgn")
    (tmp_path / "legacy_mixed_missing_name.dds").write_bytes(b"dds")

    result = convert_shader_template_with_report(
        source, tmp_path / "converted_mixed_missing_name.sht"
    )

    assert result.texture_name == "legacy_mixed_missing_name.dds"
    assert (
        "Inferred texture name from sibling texture file: legacy_mixed_missing_name.dds"
        in result.notes
    )


def test_batch_create_shader_templates_discovers_png_by_default(tmp_path: Path) -> None:
    texture_dir = tmp_path / "textures"
    texture_dir.mkdir()
    (texture_dir / "a.png").write_bytes(b"png")
    (texture_dir / "b.dds").write_bytes(b"dds")

    output_dir = tmp_path / "shader"
    from swg_tool.shader import create_shader_templates_from_dir, load_shader_profiles

    summary = create_shader_templates_from_dir(
        texture_dir,
        output_dir,
        load_shader_profiles(),
        "high",
    )

    assert summary.created == 2
    assert (output_dir / "a.sht").exists()
    assert (output_dir / "b.sht").exists()
