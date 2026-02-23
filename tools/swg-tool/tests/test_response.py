from __future__ import annotations

import tempfile
from pathlib import Path
from unittest import TestCase, mock

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool.response import (  # noqa: E402
    ResponseFileBuilder,
    ResponseFileBuilderError,
)


class ResponseFileBuilderTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def _create_file(self, relative: str, contents: str = "data") -> Path:
        path = self.tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
        return path

    def test_write_response_from_directory(self) -> None:
        source_dir = self.tmp_path / "content"
        file_a = self._create_file("content/a/b.txt")
        file_b = self._create_file("content/c/d.txt")
        destination = self.tmp_path / "response.rsp"

        builder = ResponseFileBuilder()
        result = builder.write(destination=destination, sources=[source_dir])

        self.assertEqual(destination.resolve(), result.path)
        self.assertEqual(2, len(result.entries))
        entry_paths = [entry.entry for entry in result.entries]
        self.assertEqual(["a/b.txt", "c/d.txt"], entry_paths)
        self.assertTrue(destination.exists())
        contents = destination.read_text().splitlines()
        expected_lines = [
            f"a/b.txt @ {file_a.resolve()}",
            f"c/d.txt @ {file_b.resolve()}",
        ]
        self.assertEqual(expected_lines, contents)

    def test_write_response_with_entry_root(self) -> None:
        source_dir = self.tmp_path / "content"
        self._create_file("content/foo/bar.txt")
        destination = self.tmp_path / "response.rsp"

        builder = ResponseFileBuilder(entry_root=source_dir)
        result = builder.write(destination=destination, sources=[source_dir])

        self.assertEqual(["foo/bar.txt"], [entry.entry for entry in result.entries])

    def test_missing_source_raises_error(self) -> None:
        builder = ResponseFileBuilder()
        with self.assertRaises(ResponseFileBuilderError):
            builder.write(destination=self.tmp_path / "out.rsp", sources=[self.tmp_path / "missing"])

    def test_duplicate_entries_raise_error_by_default(self) -> None:
        file_path = self._create_file("content/file.txt")
        destination = self.tmp_path / "response.rsp"

        builder = ResponseFileBuilder()
        with self.assertRaises(ResponseFileBuilderError):
            builder.write(destination=destination, sources=[file_path, file_path])

    def test_duplicate_entries_allow_overrides(self) -> None:
        file_a = self._create_file("content/base.txt")
        file_b = self._create_file("content/update.txt")
        destination = self.tmp_path / "response.rsp"

        builder = ResponseFileBuilder(allow_overrides=True)
        with mock.patch.object(
            ResponseFileBuilder,
            "_entry_for_file",
            side_effect=["duplicate", "duplicate"],
        ):
            result = builder.write(destination=destination, sources=[file_a, file_b])

        self.assertEqual(["duplicate"], [entry.entry for entry in result.entries])
        self.assertEqual(file_b.resolve(), result.entries[0].source)
