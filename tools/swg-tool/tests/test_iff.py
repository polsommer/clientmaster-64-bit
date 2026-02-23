from __future__ import annotations

import argparse
import json
import struct
import tempfile
from pathlib import Path
from unittest import TestCase

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool.iff import IffBuilder, IffDefinitionError  # noqa: E402


class IffBuilderTests(TestCase):
    def test_builds_chunk_with_padding(self) -> None:
        builder = IffBuilder.from_definition({"chunk": "TEST", "data": "abc"})

        payload = builder.build_bytes()

        expected = b"TEST" + struct.pack(">I", 3) + b"abc\x00"
        self.assertEqual(expected, payload)

    def test_builds_nested_form(self) -> None:
        definition = {
            "form": "ROOT",
            "children": [
                {"chunk": "INFO", "data": "A"},
                {
                    "form": "DATA",
                    "children": [
                        {"chunk": "NAME", "data": "abc"},
                    ],
                },
            ],
        }

        payload = IffBuilder.from_definition(definition).build_bytes()

        info_chunk = b"INFO" + struct.pack(">I", 1) + b"A\x00"
        name_chunk = b"NAME" + struct.pack(">I", 3) + b"abc\x00"
        data_form = b"FORM" + struct.pack(">I", 16) + b"DATA" + name_chunk
        expected = b"FORM" + struct.pack(">I", 38) + b"ROOT" + info_chunk + data_form

        self.assertEqual(expected, payload)

    def test_describe_outputs_tree(self) -> None:
        builder = IffBuilder.from_definition({"form": "ROOT", "children": [{"chunk": "DATA", "data": "x"}]})

        description = builder.describe()

        self.assertIn("FORM ROOT", description)
        self.assertIn("CHUNK DATA", description)

    def test_rejects_invalid_tags(self) -> None:
        with self.assertRaises(IffDefinitionError):
            IffBuilder.from_definition({"chunk": "TOO-LONG", "data": "x"})


class CreateIffCommandTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmpdir = Path(self.tempdir.name)

    def test_create_iff_writes_expected_file(self) -> None:
        from swg_tool import cli  # noqa: WPS433 (import inside test)

        definition_path = self.tmpdir / "definition.json"
        definition_path.write_text(json.dumps({"chunk": "TEST", "data": "hi"}))
        output_path = self.tmpdir / "output.iff"

        args = argparse.Namespace(
            definition=definition_path,
            output=output_path,
            preview=False,
        )

        cli._run_create_iff(args)

        self.assertTrue(output_path.exists())
        expected = b"TEST" + struct.pack(">I", 2) + b"hi"
        self.assertEqual(expected, output_path.read_bytes())

    def test_create_iff_reports_json_error(self) -> None:
        from swg_tool import cli  # noqa: WPS433 (import inside test)

        definition_path = self.tmpdir / "definition.json"
        definition_path.write_text("{invalid json}")
        output_path = self.tmpdir / "output.iff"

        args = argparse.Namespace(
            definition=definition_path,
            output=output_path,
            preview=False,
        )

        with self.assertRaises(SystemExit) as exc:
            cli._run_create_iff(args)

        self.assertEqual(1, exc.exception.code)
