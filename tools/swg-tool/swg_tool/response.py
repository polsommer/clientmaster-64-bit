"""Utilities for generating TreeFileBuilder response files."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Sequence


@dataclass(frozen=True)
class ResponseFileEntry:
    """Represents a single entry in a TreeFileBuilder response file."""

    entry: str
    source: Path


@dataclass(frozen=True)
class ResponseFileBuildResult:
    """Result of generating a response file."""

    path: Path
    entries: List[ResponseFileEntry]


class ResponseFileBuilderError(RuntimeError):
    """Raised when response file generation fails."""


class ResponseFileBuilder:
    """Create TreeFileBuilder response files from filesystem paths."""

    def __init__(
        self,
        *,
        entry_root: Optional[Path | str] = None,
        sort: bool = True,
        allow_overrides: bool = False,
    ) -> None:
        self.sort = sort
        self.allow_overrides = allow_overrides
        if entry_root is None:
            self.entry_root: Optional[Path] = None
        else:
            root = Path(entry_root).expanduser().resolve()
            if not root.is_dir():
                raise ResponseFileBuilderError(f"Entry root is not a directory: {root}")
            self.entry_root = root

    def _resolve_sources(self, sources: Sequence[Path | str]) -> List[Path]:
        resolved: List[Path] = []
        for source in sources:
            path = Path(source).expanduser().resolve()
            if not path.exists():
                raise ResponseFileBuilderError(f"Source path not found: {path}")
            resolved.append(path)

        if not resolved:
            raise ResponseFileBuilderError("At least one source path must be provided.")

        return resolved

    def _determine_root(self, resolved: Iterable[Path]) -> Path:
        if self.entry_root is not None:
            return self.entry_root

        base_paths = [path if path.is_dir() else path.parent for path in resolved]
        common = os.path.commonpath([str(path) for path in base_paths])
        root = Path(common)
        if not root.exists():
            raise ResponseFileBuilderError(
                f"Computed entry root does not exist: {root}"
            )
        return root

    def _entry_for_file(self, root: Path, file_path: Path) -> str:
        try:
            relative = file_path.relative_to(root)
        except ValueError as exc:  # pragma: no cover - handled via explicit error
            raise ResponseFileBuilderError(
                f"{file_path} is not contained within the entry root {root}"
            ) from exc

        return relative.as_posix()

    def _collect_entries(self, root: Path, resolved: Sequence[Path]) -> List[ResponseFileEntry]:
        entries: List[ResponseFileEntry] = []
        entry_lookup: dict[str, int] = {}

        for source in resolved:
            if source.is_dir():
                files = [path for path in source.rglob("*") if path.is_file()]
            else:
                files = [source]

            for file_path in files:
                entry_name = self._entry_for_file(root, file_path)
                if entry_name in entry_lookup:
                    if not self.allow_overrides:
                        raise ResponseFileBuilderError(
                            f"Duplicate tree entry detected for {file_path}: {entry_name}"
                        )
                    entries[entry_lookup[entry_name]] = ResponseFileEntry(
                        entry=entry_name, source=file_path
                    )
                    continue

                entry_lookup[entry_name] = len(entries)
                entries.append(ResponseFileEntry(entry=entry_name, source=file_path))

        if not entries:
            raise ResponseFileBuilderError("No files were found in the provided sources.")

        if self.sort:
            entries.sort(key=lambda entry: entry.entry)

        return entries

    def build_entries(self, sources: Sequence[Path | str]) -> List[ResponseFileEntry]:
        """Resolve filesystem paths into response file entries."""

        resolved = self._resolve_sources(sources)
        root = self._determine_root(resolved)
        return self._collect_entries(root, resolved)

    def write(self, *, destination: Path | str, sources: Sequence[Path | str]) -> ResponseFileBuildResult:
        """Generate a response file for the provided sources."""

        destination_path = Path(destination).expanduser().resolve()
        entries = self.build_entries(sources)
        destination_path.parent.mkdir(parents=True, exist_ok=True)

        with destination_path.open("w", encoding="utf-8", newline="\n") as handle:
            for entry in entries:
                handle.write(f"{entry.entry} @ {entry.source}\n")

        return ResponseFileBuildResult(path=destination_path, entries=entries)


__all__ = [
    "ResponseFileBuilder",
    "ResponseFileBuilderError",
    "ResponseFileBuildResult",
    "ResponseFileEntry",
]
