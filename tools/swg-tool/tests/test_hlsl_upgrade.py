from __future__ import annotations

import tempfile
from pathlib import Path
from unittest import TestCase

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool.hlsl_upgrade import (  # noqa: E402
    DEFAULT_TARGET_PROFILE,
    upgrade_hlsl_content,
    upgrade_hlsl_in_dir,
)


class HlslUpgradeTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def test_upgrade_hlsl_content_adds_target(self) -> None:
        content = "//hlsl vs_1_1 vs_2_0 vs_3_0\nfloat4 main(): POSITION0 { return 0; }\n"

        updated, old_header, new_header, reason = upgrade_hlsl_content(
            content,
            DEFAULT_TARGET_PROFILE,
            "add",
            False,
        )

        self.assertEqual("upgraded", reason)
        self.assertEqual("//hlsl vs_1_1 vs_2_0 vs_3_0", old_header)
        self.assertEqual("//hlsl vs_1_1 vs_2_0 vs_3_0 vs_4_0", new_header)
        self.assertIn("vs_4_0", updated.splitlines()[0])

    def test_upgrade_hlsl_content_replaces_target(self) -> None:
        content = "//hlsl vs_2_0 vs_3_0\nfloat4 main(): POSITION0 { return 0; }\n"

        updated, old_header, new_header, reason = upgrade_hlsl_content(
            content,
            DEFAULT_TARGET_PROFILE,
            "replace",
            False,
        )

        self.assertEqual("upgraded", reason)
        self.assertEqual("//hlsl vs_2_0 vs_3_0", old_header)
        self.assertEqual("//hlsl vs_4_0", new_header)
        self.assertTrue(updated.startswith("//hlsl vs_4_0"))

    def test_upgrade_hlsl_content_upgrades_missing_profile(self) -> None:
        content = "//hlsl\nfloat4 main(): POSITION0 { return 0; }\n"

        updated, old_header, new_header, reason = upgrade_hlsl_content(
            content,
            DEFAULT_TARGET_PROFILE,
            "add",
            False,
        )

        self.assertEqual("upgraded", reason)
        self.assertEqual("//hlsl", old_header)
        self.assertEqual("//hlsl vs_4_0", new_header)
        self.assertTrue(updated.startswith("//hlsl vs_4_0"))

    def test_upgrade_hlsl_content_smart_replaces_legacy(self) -> None:
        content = "//hlsl vs_1_1 vs_2_0\nfloat4 main(): POSITION0 { return 0; }\n"

        updated, old_header, new_header, reason = upgrade_hlsl_content(
            content,
            DEFAULT_TARGET_PROFILE,
            "smart",
            False,
        )

        self.assertEqual("upgraded", reason)
        self.assertEqual("//hlsl vs_1_1 vs_2_0", old_header)
        self.assertEqual("//hlsl vs_4_0", new_header)
        self.assertTrue(updated.startswith("//hlsl vs_4_0"))

    def test_upgrade_hlsl_in_dir_writes_output(self) -> None:
        input_dir = self.tmp_path / "clientHLSL files"
        input_dir.mkdir()
        source = input_dir / "sample.vsh"
        source.write_text("//hlsl vs_2_0\nfloat4 main(): POSITION0 { return 0; }\n")

        output_dir = self.tmp_path / "upgraded"
        summary = upgrade_hlsl_in_dir(
            input_dir,
            output_dir,
            target_profile=DEFAULT_TARGET_PROFILE,
            mode="add",
            pattern="*.vsh",
            workers=1,
            use_gpu=False,
            dry_run=False,
            enforce_dx9=False,
        )

        output_path = output_dir / "sample.vsh"
        self.assertTrue(output_path.exists())
        self.assertEqual(1, summary.changed_files)
        self.assertIn("vs_4_0", output_path.read_text())
