from __future__ import annotations

import argparse
import tempfile
from pathlib import Path
from unittest import TestCase

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool import cli  # noqa: E402
from swg_tool.shader import read_shader_template_info  # noqa: E402


class GenerateResponseCommandTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def _create_file(self, relative: str, contents: str = "data") -> Path:
        path = self.tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
        return path

    def test_generate_response_writes_expected_file(self) -> None:
        source_dir = self.tmp_path / "content"
        first = self._create_file("content/a.txt")
        second = self._create_file("content/sub/b.txt")
        destination = self.tmp_path / "out.rsp"

        args = argparse.Namespace(
            destination=destination,
            sources=[source_dir],
            entry_root=None,
            allow_overrides=False,
        )

        cli._run_generate_response(args)

        self.assertTrue(destination.exists())
        contents = destination.read_text().splitlines()
        expected = [
            f"a.txt @ {first.resolve()}",
            f"sub/b.txt @ {second.resolve()}",
        ]
        self.assertEqual(expected, contents)

    def test_generate_response_requires_sources(self) -> None:
        destination = self.tmp_path / "out.rsp"

        args = argparse.Namespace(
            destination=destination,
            sources=None,
            entry_root=None,
            allow_overrides=False,
        )

        with self.assertRaises(SystemExit) as exc:
            cli._run_generate_response(args)

        self.assertEqual(2, exc.exception.code)

    def test_generate_response_surfaces_builder_errors(self) -> None:
        destination = self.tmp_path / "out.rsp"
        missing = self.tmp_path / "missing"

        args = argparse.Namespace(
            destination=destination,
            sources=[missing],
            entry_root=None,
            allow_overrides=False,
        )

        with self.assertRaises(SystemExit) as exc:
            cli._run_generate_response(args)

        self.assertEqual(1, exc.exception.code)


class GenerateRspConfigCommandTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def test_generate_rsp_config_writes_file(self) -> None:
        root = self.tmp_path / "data"
        root.mkdir()
        output = self.tmp_path / "TreeFileRspBuilder.cfg"

        args = argparse.Namespace(
            output=output,
            entries=[f"Root data={root}"],
            allow_missing=False,
            no_header=False,
        )

        cli._run_generate_rsp_config(args)

        self.assertTrue(output.exists())
        contents = output.read_text()
        self.assertIn("# Root data", contents)
        self.assertIn(root.resolve().as_posix(), contents)

    def test_generate_rsp_config_errors_on_missing_path(self) -> None:
        missing = self.tmp_path / "missing"
        output = self.tmp_path / "TreeFileRspBuilder.cfg"

        args = argparse.Namespace(
            output=output,
            entries=[str(missing)],
            allow_missing=False,
            no_header=False,
        )

        with self.assertRaises(SystemExit) as exc:
            cli._run_generate_rsp_config(args)

        self.assertEqual(1, exc.exception.code)

    def test_generate_rsp_config_allows_missing_when_requested(self) -> None:
        missing = self.tmp_path / "missing"
        output = self.tmp_path / "TreeFileRspBuilder.cfg"

        args = argparse.Namespace(
            output=output,
            entries=[str(missing)],
            allow_missing=True,
            no_header=True,
        )

        cli._run_generate_rsp_config(args)

        self.assertTrue(output.exists())


class CreateShaderCommandTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def test_create_shader_accepts_png_texture_without_allow_non_dds(self) -> None:
        output = self.tmp_path / "shader" / "example.sht"

        args = argparse.Namespace(
            textures=["texture/example.png"],
            texture_dirs=[],
            output=output,
            output_dir=self.tmp_path / "shader",
            effect="effect\\simplemt1z.eft",
            effect_root=None,
            strict_effects=False,
            profile_map=cli.DEFAULT_PROFILE_MAP,
            quality=cli.DEFAULT_QUALITY_ORDER[-1],
            pattern="*.*",
            force=False,
            allow_non_dds=False,
        )

        cli._run_create_shader(args)

        self.assertTrue(output.exists())

    def test_create_shader_rejects_unknown_extension_without_allow_non_dds(self) -> None:
        output = self.tmp_path / "shader" / "example.sht"

        args = argparse.Namespace(
            textures=["texture/example.tga"],
            texture_dirs=[],
            output=output,
            output_dir=self.tmp_path / "shader",
            effect="effect\\simplemt1z.eft",
            effect_root=None,
            strict_effects=False,
            profile_map=cli.DEFAULT_PROFILE_MAP,
            quality=cli.DEFAULT_QUALITY_ORDER[-1],
            pattern="*.*",
            force=False,
            allow_non_dds=False,
        )

        with self.assertRaises(SystemExit) as exc:
            cli._run_create_shader(args)

        self.assertEqual(1, exc.exception.code)

    def test_create_shader_normalizes_absolute_windows_png_path(self) -> None:
        output = self.tmp_path / "shader" / "example.sht"

        args = argparse.Namespace(
            textures=["C:/Users/swg/Desktop/star wars galaxies/texture/ui_notice.png"],
            texture_dirs=[],
            output=output,
            output_dir=self.tmp_path / "shader",
            effect="effect\\simplemt1z.eft",
            effect_root=None,
            strict_effects=False,
            profile_map=cli.DEFAULT_PROFILE_MAP,
            quality=cli.DEFAULT_QUALITY_ORDER[-1],
            pattern="*.*",
            force=False,
            allow_non_dds=False,
        )

        cli._run_create_shader(args)

        self.assertTrue(output.exists())
        info = read_shader_template_info(output)
        self.assertEqual("texture/ui_notice.png", info.texture_name)
