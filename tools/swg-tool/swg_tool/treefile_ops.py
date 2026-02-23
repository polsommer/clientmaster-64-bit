"""Helpers for listing and extracting TreeFile archives."""
from __future__ import annotations

from dataclasses import dataclass
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Iterable, Sequence


_TOOLS_DIR = Path(__file__).resolve().parents[2]

DEFAULT_EXECUTABLE_NAMES = [
    "TreeFileExtractor",
    "TreeFileExtractor.exe",
    str(_TOOLS_DIR / "TreeFileExtractor.exe"),
    str(_TOOLS_DIR / "TreeFileExtractor"),
]

SUPPORTED_TREEFILE_EXTENSIONS = {".tre", ".tres", ".tresx"}
TREEFILE_PASSPHRASE_ENV = "SWG_TREEFILE_PASSPHRASE"
_PASSPHRASE_ERROR_CODES = {
    2147483651,  # TreeFileExtractor encryption failure (0x80000003)
    -2147483645,  # Signed interpretation of 0x80000003
}
_PASSPHRASE_ERROR_KEYWORDS = (
    "passphrase",
    "password",
    "decrypt",
    "decryption",
    "invalid",
    "incorrect",
    "bad key",
    "authentication",
    "mac",
)


class TreeFileExtractorError(RuntimeError):
    """Raised when TreeFileExtractor cannot complete a request."""

    def __init__(
        self,
        message: str,
        *,
        stdout: str | None = None,
        stderr: str | None = None,
        passphrase_error: bool = False,
    ) -> None:
        super().__init__(message)
        self.stdout = stdout
        self.stderr = stderr
        self.passphrase_error = passphrase_error


@dataclass(frozen=True)
class TreeFileListEntry:
    path: str
    offset: int | None


@dataclass(frozen=True)
class TreeFileListResult:
    entries: list[TreeFileListEntry]
    stdout: str
    stderr: str


@dataclass(frozen=True)
class TreeFileExtractResult:
    extracted_paths: list[Path]
    stdout: str
    stderr: str
    output_dir: Path
    used_temp_dir: bool


def treefile_requires_passphrase(treefile: Path) -> bool:
    return treefile.suffix.lower() in {".tres", ".tresx"}


def resolve_treefile_extractor(executable: Path | None = None) -> Path:
    if executable is not None:
        resolved = executable.expanduser().resolve()
        if resolved.is_file():
            return resolved
        raise TreeFileExtractorError(f"TreeFileExtractor executable not found: {resolved}")

    for candidate in DEFAULT_EXECUTABLE_NAMES:
        candidate_path = Path(candidate)
        if candidate_path.is_file():
            return candidate_path.resolve()
        if candidate_path.name == candidate:
            resolved = shutil.which(candidate)
            if resolved:
                return Path(resolved).resolve()

    raise TreeFileExtractorError(
        "TreeFileExtractor executable not found. "
        "Ensure TreeFileExtractor.exe is available in tools/ or on PATH."
    )


def list_treefile(
    treefile: Path,
    *,
    extractor: Path | None = None,
    passphrase: str | None = None,
) -> TreeFileListResult:
    treefile = treefile.expanduser().resolve()
    _validate_treefile(treefile)
    passphrase = _normalize_passphrase(treefile, passphrase)
    extractor_path = resolve_treefile_extractor(extractor)

    command = _build_command(
        extractor_path,
        ["-l", str(treefile)],
        passphrase,
    )
    completed = _run_extractor(command, extractor_path.parent)
    _raise_on_error(completed, "list", treefile, passphrase=passphrase)
    entries = _parse_listing(completed.stdout)
    return TreeFileListResult(entries=entries, stdout=completed.stdout, stderr=completed.stderr)


def extract_treefile(
    treefile: Path,
    output_dir: Path,
    *,
    extractor: Path | None = None,
    passphrase: str | None = None,
    entries: Iterable[str] | None = None,
) -> TreeFileExtractResult:
    treefile = treefile.expanduser().resolve()
    _validate_treefile(treefile)
    passphrase = _normalize_passphrase(treefile, passphrase)
    extractor_path = resolve_treefile_extractor(extractor)
    output_dir = output_dir.expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    selected_entries = [entry for entry in (entries or []) if entry]
    if selected_entries:
        return _extract_selected_entries(
            extractor_path,
            treefile,
            output_dir,
            selected_entries,
            passphrase,
        )

    list_result = list_treefile(treefile, extractor=extractor_path, passphrase=passphrase)
    expected_paths = [
        output_dir / Path(*entry.path.split("/")) for entry in list_result.entries
    ]
    command = _build_command(
        extractor_path,
        ["-e", str(treefile), str(output_dir)],
        passphrase,
    )
    completed = _run_extractor(command, extractor_path.parent)
    _raise_on_error(completed, "extract", treefile, passphrase=passphrase)
    return TreeFileExtractResult(
        extracted_paths=expected_paths,
        stdout=completed.stdout,
        stderr=completed.stderr,
        output_dir=output_dir,
        used_temp_dir=False,
    )


def _extract_selected_entries(
    extractor_path: Path,
    treefile: Path,
    output_dir: Path,
    entries: Sequence[str],
    passphrase: str | None,
) -> TreeFileExtractResult:
    normalized_entries = [entry.replace("\\", "/") for entry in entries]
    with tempfile.TemporaryDirectory(prefix="swg_treefile_") as temp_dir:
        temp_path = Path(temp_dir)
        command = _build_command(
            extractor_path,
            ["-e", str(treefile), str(temp_path)],
            passphrase,
        )
        completed = _run_extractor(command, extractor_path.parent)
        _raise_on_error(completed, "extract", treefile, passphrase=passphrase)

        extracted_paths: list[Path] = []
        for entry in normalized_entries:
            relative = Path(*entry.split("/"))
            source = temp_path / relative
            if not source.exists():
                raise TreeFileExtractorError(
                    f"Extracted file not found: {entry}",
                    stdout=completed.stdout,
                    stderr=completed.stderr,
                )
            destination = output_dir / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            extracted_paths.append(destination)

        return TreeFileExtractResult(
            extracted_paths=extracted_paths,
            stdout=completed.stdout,
            stderr=completed.stderr,
            output_dir=output_dir,
            used_temp_dir=True,
        )


def _validate_treefile(treefile: Path) -> None:
    if not treefile.is_file():
        raise TreeFileExtractorError(f"Tree file not found: {treefile}")
    if treefile.suffix.lower() not in SUPPORTED_TREEFILE_EXTENSIONS:
        raise TreeFileExtractorError(
            "Unsupported tree file extension. "
            "Expected .tre, .tres, or .tresx files."
        )


def _normalize_passphrase(treefile: Path, passphrase: str | None) -> str | None:
    if passphrase is None:
        env_passphrase = os.getenv(TREEFILE_PASSPHRASE_ENV)
        if env_passphrase is None:
            return None
        passphrase = env_passphrase
    normalized = passphrase.rstrip("\r\n")
    if not normalized.strip():
        return ""
    return normalized


def _build_command(
    executable: Path,
    args: Sequence[str],
    passphrase: str | None,
) -> list[str]:
    command = [str(executable), *args]
    if passphrase:
        command.extend(["--", f"SharedFile.treeFileEncryptionPassphrase={passphrase}"])
    return command


def _run_extractor(command: Sequence[str], cwd: Path | None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(  # nosec B603 - executable is controlled by user or repo
        list(command),
        capture_output=True,
        text=True,
        check=False,
        cwd=str(cwd) if cwd else None,
    )


def _raise_on_error(
    completed: subprocess.CompletedProcess[str],
    action: str,
    treefile: Path,
    *,
    passphrase: str | None = None,
) -> None:
    passphrase_error = _is_passphrase_error(completed)
    failure_lines = _extract_failure_lines(completed.stdout) + _extract_failure_lines(
        completed.stderr
    )
    if completed.returncode == 0 and not failure_lines:
        return

    if failure_lines:
        detail = " ".join(failure_lines)
        message = f"TreeFileExtractor failed to {action} {treefile}: {detail}"
    else:
        message = (
            f"TreeFileExtractor failed to {action} {treefile} "
            f"(exit code {completed.returncode})."
        )
        if treefile_requires_passphrase(treefile):
            if passphrase:
                hint = (
                    "This .tres/.tresx archive is encrypted; verify the passphrase."
                )
            else:
                hint = (
                    "This .tres/.tresx archive is encrypted; provide a passphrase "
                    "(use --passphrase or set SWG_TREEFILE_PASSPHRASE)."
                )
            message = f"{message} {hint}"

    raise TreeFileExtractorError(
        message,
        stdout=completed.stdout,
        stderr=completed.stderr,
        passphrase_error=passphrase_error,
    )


def _extract_failure_lines(output: str) -> list[str]:
    lines = []
    for line in output.splitlines():
        lowered = line.strip().lower()
        if lowered.startswith("error:") or "fatal" in lowered:
            lines.append(line.strip())
    return lines


def _is_passphrase_error(completed: subprocess.CompletedProcess[str]) -> bool:
    if completed.returncode in _PASSPHRASE_ERROR_CODES:
        return True
    combined = " ".join(
        chunk.strip().lower()
        for chunk in (completed.stdout or "", completed.stderr or "")
        if chunk
    )
    return any(keyword in combined for keyword in _PASSPHRASE_ERROR_KEYWORDS)


def _parse_listing(output: str) -> list[TreeFileListEntry]:
    entries: list[TreeFileListEntry] = []
    for line in output.splitlines():
        cleaned = line.strip()
        if not cleaned:
            continue
        if cleaned.lower().startswith("error:"):
            continue
        if "\t" in cleaned:
            name, offset_text = cleaned.split("\t", 1)
            offset = None
            try:
                offset = int(offset_text.strip())
            except ValueError:
                offset = None
            entries.append(
                TreeFileListEntry(path=name.strip().replace("\\", "/"), offset=offset)
            )
        else:
            entries.append(TreeFileListEntry(path=cleaned.replace("\\", "/"), offset=None))
    return entries
