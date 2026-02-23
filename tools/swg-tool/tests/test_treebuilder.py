from __future__ import annotations

import builtins
import io
import struct
import subprocess
import tempfile
from pathlib import Path
from unittest import TestCase, mock

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from swg_tool import cli  # noqa: E402
from swg_tool.treebuilder import (  # noqa: E402
    TreeBuildResult,
    TreeFileBuilder,
    TreeFileBuilderCapabilities,
    TreeFileBuilderError,
)

TAG_TREE = 0x54524545
TAG_TRESX = 0x54525358
HEADER_STRUCT = struct.Struct("<9I")


class TreeFileBuilderTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)
        self.builder = self.tmp_path / "TreeFileBuilder.exe"
        self.builder.write_text("echo")
        self.response = self.tmp_path / "example.rsp"
        self.response.write_text("content")
        self.output = self.tmp_path / "out.tres"

    def _create_builder(
        self, capabilities: TreeFileBuilderCapabilities | None = None
    ) -> TreeFileBuilder:
        if capabilities is None:
            capabilities = TreeFileBuilderCapabilities()

        with mock.patch.object(
            TreeFileBuilder, "_probe_capabilities", return_value=capabilities
        ):
            return TreeFileBuilder(executable=self.builder)

    def test_invokes_treefilebuilder_with_expected_arguments(self) -> None:
        builder = self._create_builder()

        completed = subprocess.CompletedProcess(
            args=[], returncode=0, stdout="ok", stderr=""
        )
        with mock.patch.object(
            builder, "_run_builder", return_value=completed
        ) as run_mock:
            result = builder.build(
                response_file=self.response,
                output_file=self.output,
                force_encrypt=True,
                passphrase="secret",
            )

        expected_command = [
            str(self.builder.resolve()),
            f"--responseFile={self.response}",
            "--encrypt",
            "--passphrase",
            "secret",
            str(self.output.resolve()),
        ]
        run_mock.assert_called_once()
        call_args = run_mock.call_args.args
        self.assertEqual(expected_command, call_args[0])
        self.assertIsNone(call_args[1])
        self.assertIsNone(call_args[2])
        self.assertEqual("ok", result.stdout)
        self.assertEqual(self.output.resolve(), result.output)

    def test_does_not_require_existing_response_file(self) -> None:
        builder = self._create_builder()

        missing_response = self.tmp_path / "missing.rsp"
        completed = subprocess.CompletedProcess(
            args=[], returncode=0, stdout="ok", stderr=""
        )

        with mock.patch.object(
            builder, "_run_builder", return_value=completed
        ) as run_mock:
            result = builder.build(
                response_file=missing_response, output_file=self.output
            )

        expected_command = [
            str(self.builder.resolve()),
            f"--responseFile={missing_response}",
            str(self.output.resolve()),
        ]

        self.assertEqual(expected_command, run_mock.call_args.args[0])
        self.assertEqual("ok", result.stdout)

    def test_raises_error_when_treefilebuilder_fails(self) -> None:
        builder = self._create_builder()

        completed = subprocess.CompletedProcess(
            args=[], returncode=1, stdout="", stderr="boom"
        )
        with mock.patch.object(
            builder, "_run_builder", return_value=completed
        ):
            with self.assertRaises(TreeFileBuilderError) as ctx:
                builder.build(response_file=self.response, output_file=self.output)

        self.assertIn("exited with code 1", str(ctx.exception))
        self.assertIsNotNone(ctx.exception.result)
        self.assertEqual(1, ctx.exception.result.returncode)

    def test_run_builder_streaming_uses_line_buffering(self) -> None:
        builder = self._create_builder()

        stdout = io.StringIO("line1\nline2\n")
        stderr = io.StringIO("warn\n")

        fake_process = mock.Mock()
        fake_process.stdout = stdout
        fake_process.stderr = stderr
        fake_process.wait.return_value = 0

        with mock.patch("subprocess.Popen", return_value=fake_process) as popen:
            stdout_captured: list[str] = []
            stderr_captured: list[str] = []
            result = builder._run_builder(
                ["TreeFileBuilder"], stdout_captured.append, stderr_captured.append
            )

        self.assertEqual(["line1\n", "line2\n"], stdout_captured)
        self.assertEqual(["warn\n"], stderr_captured)
        self.assertEqual(0, result.returncode)
        self.assertEqual("line1\nline2\n", result.stdout)
        self.assertEqual("warn\n", result.stderr)

        self.assertTrue(popen.called)
        self.assertEqual(1, popen.call_args.kwargs.get("bufsize"))

    def test_conflicting_encryption_flags(self) -> None:
        builder = self._create_builder()

        with self.assertRaises(TreeFileBuilderError):
            builder.build(
                response_file=self.response,
                output_file=self.output,
                force_encrypt=True,
                disable_encrypt=True,
            )

    def test_error_when_passphrase_not_supported(self) -> None:
        capabilities = TreeFileBuilderCapabilities(supports_passphrase=False)
        builder = self._create_builder(capabilities)

        with self.assertRaises(TreeFileBuilderError) as ctx:
            builder.build(
                response_file=self.response,
                output_file=self.output,
                passphrase="secret",
            )

        self.assertIn("--passphrase", str(ctx.exception))

    def test_error_when_encrypt_not_supported(self) -> None:
        capabilities = TreeFileBuilderCapabilities(supports_encrypt=False)
        builder = self._create_builder(capabilities)

        with self.assertRaises(TreeFileBuilderError) as ctx:
            builder.build(
                response_file=self.response,
                output_file=self.output,
                force_encrypt=True,
            )

        self.assertIn("--encrypt", str(ctx.exception))

    def test_resolves_default_executable_from_repo_tools_directory(self) -> None:
        builder_path = self.tmp_path / "TreeFileBuilder.exe"
        builder_path.write_text("echo")

        with mock.patch(
            "swg_tool.treebuilder.DEFAULT_EXECUTABLE_NAMES",
            (str(builder_path),),
        ), mock.patch.object(
            TreeFileBuilder, "_probe_capabilities", return_value=TreeFileBuilderCapabilities()
        ):
            builder = TreeFileBuilder()

        self.assertEqual(builder_path.resolve(), builder.executable)

    def test_force_internal_bypasses_external_detection(self) -> None:
        with mock.patch(
            "swg_tool.treebuilder.DEFAULT_EXECUTABLE_NAMES",
            (str(self.builder),),
        ), mock.patch.object(TreeFileBuilder, "_probe_capabilities") as probe:
            builder = TreeFileBuilder(executable=self.builder, force_internal=True)

        self.assertTrue(builder.using_internal)
        self.assertIsNone(builder.executable)
        self.assertTrue(builder.capabilities.supports_encrypt)
        probe.assert_not_called()

    def test_streams_output_via_callbacks(self) -> None:
        builder = self._create_builder()

        def _fake_run(
            command: list[str],
            stdout_callback,
            stderr_callback,
        ) -> subprocess.CompletedProcess[str]:
            if stdout_callback:
                stdout_callback("out\n")
            if stderr_callback:
                stderr_callback("err\n")
            return subprocess.CompletedProcess(
                args=command, returncode=0, stdout="out\n", stderr="err\n"
            )

        stdout_chunks: list[str] = []
        stderr_chunks: list[str] = []

        with mock.patch.object(builder, "_run_builder", side_effect=_fake_run):
            result = builder.build(
                response_file=self.response,
                output_file=self.output,
                stdout_callback=stdout_chunks.append,
                stderr_callback=stderr_chunks.append,
            )

        self.assertEqual(["out\n"], stdout_chunks)
        self.assertEqual(["err\n"], stderr_chunks)
        self.assertEqual("out\n", result.stdout)
        self.assertEqual("err\n", result.stderr)

    def test_probe_capabilities_from_help_output(self) -> None:
        help_text = """
Usage:
  --encrypt
  --noEncrypt
  --passphrase
  --noCreate
  --quiet
"""

        completed = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=help_text, stderr=""
        )

        with mock.patch("subprocess.run", return_value=completed):
            builder = TreeFileBuilder(executable=self.builder)

        capabilities = builder.capabilities
        self.assertTrue(capabilities.supports_encrypt)
        self.assertTrue(capabilities.supports_no_encrypt)
        self.assertTrue(capabilities.supports_passphrase)
        self.assertTrue(capabilities.supports_dry_run)
        self.assertTrue(capabilities.supports_quiet)

    def test_internal_fallback_builds_tree_file(self) -> None:
        payload_dir = self.tmp_path / "content"
        payload_file = payload_dir / "example.txt"
        payload_file.parent.mkdir(parents=True, exist_ok=True)
        payload_file.write_text("payload")

        response = self.tmp_path / "input.rsp"
        response.write_text(f"example.txt @ {payload_file}\\n")
        output = self.tmp_path / "out.tre"

        with mock.patch(
            "swg_tool.treebuilder.DEFAULT_EXECUTABLE_NAMES", tuple()
        ), mock.patch("swg_tool.treebuilder.shutil.which", return_value=None):
            builder = TreeFileBuilder()
            result = builder.build(response_file=response, output_file=output)

        self.assertTrue(builder.using_internal)
        self.assertEqual(output, result.output)
        self.assertTrue(output.exists())

        header = HEADER_STRUCT.unpack(output.read_bytes()[: HEADER_STRUCT.size])
        self.assertEqual(TAG_TREE, header[0])

    def test_internal_tresx_encryption_changes_payload(self) -> None:
        payload_dir = self.tmp_path / "content"
        payload_file = payload_dir / "example.txt"
        payload_file.parent.mkdir(parents=True, exist_ok=True)
        payload_file.write_text("payload")

        response = self.tmp_path / "input.rsp"
        response.write_text(f"example.txt @ {payload_file}\\n")
        encrypted_out = self.tmp_path / "out.tresx"
        plain_out = self.tmp_path / "out_plain.tresx"

        builder = TreeFileBuilder(executable=self.builder, force_internal=True)
        self.assertTrue(builder.using_internal)
        builder.build(
            response_file=response,
            output_file=plain_out,
            disable_encrypt=True,
        )
        builder.build(
            response_file=response,
            output_file=encrypted_out,
            force_encrypt=True,
            passphrase="secret",
        )

        plain_bytes = plain_out.read_bytes()
        encrypted_bytes = encrypted_out.read_bytes()
        self.assertEqual(TAG_TRESX, HEADER_STRUCT.unpack(plain_bytes[: HEADER_STRUCT.size])[0])
        self.assertEqual(
            TAG_TRESX,
            HEADER_STRUCT.unpack(encrypted_bytes[: HEADER_STRUCT.size])[0],
        )
        self.assertNotEqual(
            plain_bytes[HEADER_STRUCT.size :],
            encrypted_bytes[HEADER_STRUCT.size :],
        )


class BuildTreeCommandTests(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.tmp_path = Path(self.tempdir.name)

    def _default_args(self) -> mock.Mock:
        return mock.Mock(
            response=None,
            sources=None,
            entry_root=None,
            output=Path(self.tmp_path / "out.tre"),
            builder=None,
            internal_builder=False,
            no_toc_compression=False,
            no_file_compression=False,
            dry_run=False,
            quiet=False,
            encrypt=False,
            no_encrypt=False,
            passphrase=None,
            debug=False,
        )

    def test_generates_response_from_sources(self) -> None:
        content_dir = self.tmp_path / "content"
        file_path = content_dir / "foo" / "bar.txt"
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.write_text("payload")

        args = self._default_args()
        args.response = self.tmp_path / "generated.rsp"
        args.sources = [content_dir]

        expected_result = TreeBuildResult(
            command=["TreeFileBuilder"],
            stdout="ok",
            stderr="",
            returncode=0,
            output=args.output,
        )

        with mock.patch.object(cli, "TreeFileBuilder") as builder_cls:
            builder_instance = builder_cls.return_value
            builder_instance.build.return_value = expected_result

            cli._run_build_tree(args)

        builder_cls.assert_called_once_with(executable=None, force_internal=False)
        builder_instance.build.assert_called_once()
        call_kwargs = builder_instance.build.call_args.kwargs
        stdout_callback = call_kwargs.pop("stdout_callback")
        stderr_callback = call_kwargs.pop("stderr_callback")
        self.assertIsNotNone(stdout_callback)
        self.assertIsNotNone(stderr_callback)
        self.assertEqual(args.response.resolve(), call_kwargs["response_file"])

        response_contents = args.response.read_text().splitlines()
        self.assertEqual([f"foo/bar.txt @ {file_path.resolve()}"], response_contents)

    def test_quiet_mode_suppresses_buffered_output(self) -> None:
        args = self._default_args()
        args.quiet = True
        args.response = self.tmp_path / "existing.rsp"
        args.response.write_text("contents")

        expected_result = TreeBuildResult(
            command=["TreeFileBuilder"],
            stdout="-\b",
            stderr="ignored",
            returncode=0,
            output=args.output,
        )

        with mock.patch.object(cli, "TreeFileBuilder") as builder_cls:
            builder_instance = builder_cls.return_value
            builder_instance.build.return_value = expected_result

            with mock.patch.object(builtins, "print") as print_mock:
                cli._run_build_tree(args)

        printed_messages = [" ".join(str(arg) for arg in call.args) for call in print_mock.call_args_list]
        self.assertNotIn(expected_result.stdout, printed_messages)
        self.assertIn(f"Tree file written to {args.output}", printed_messages)

    def test_internal_builder_flag_passed_through(self) -> None:
        args = self._default_args()
        args.response = self.tmp_path / "existing.rsp"
        args.response.write_text("contents")
        args.builder = self.tmp_path / "TreeFileBuilder.exe"
        args.builder.write_text("echo")
        args.internal_builder = True

        expected_result = TreeBuildResult(
            command=["TreeFileBuilder"],
            stdout="ok",
            stderr="",
            returncode=0,
            output=args.output,
        )

        with mock.patch.object(cli, "TreeFileBuilder") as builder_cls:
            builder_instance = builder_cls.return_value
            builder_instance.build.return_value = expected_result

            cli._run_build_tree(args)

        builder_cls.assert_called_once_with(
            executable=args.builder,
            force_internal=True,
        )

    def test_missing_explicit_builder_errors(self) -> None:
        args = self._default_args()
        args.response = self.tmp_path / "existing.rsp"
        args.response.write_text("contents")
        args.builder = self.tmp_path / "missing.exe"

        with mock.patch.object(builtins, "print") as print_mock:
            with self.assertRaises(SystemExit) as ctx:
                cli._run_build_tree(args)

        self.assertEqual(1, ctx.exception.code)
        printed_messages = [
            " ".join(str(arg) for arg in call.args) for call in print_mock.call_args_list
        ]
        self.assertTrue(
            any(message.startswith("[ERROR] TreeFileBuilder executable not found") for message in printed_messages)
        )
