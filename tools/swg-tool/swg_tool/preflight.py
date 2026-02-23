"""Preflight analysis for tree build inputs."""

from __future__ import annotations

import hashlib
import importlib.util
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, List, Optional

from .response import ResponseFileBuilder, ResponseFileBuilderError, ResponseFileEntry


_COMPRESSED_EXTENSIONS = {
    ".7z",
    ".avi",
    ".bz2",
    ".dds",
    ".flac",
    ".gif",
    ".gz",
    ".jpeg",
    ".jpg",
    ".lz4",
    ".m4a",
    ".mkv",
    ".mp3",
    ".mp4",
    ".ogg",
    ".opus",
    ".png",
    ".rar",
    ".tga",
    ".webp",
    ".wma",
    ".wmv",
    ".zip",
}

_HASH_CHUNK_SIZE = 4 * 1024 * 1024
_GPU_HASH_MASK = (1 << 64) - 1
TREEFILE_OFFSET_LIMIT = 2**31 - 1


@dataclass(frozen=True)
class TreePreflightDuplicate:
    """Group of files with identical contents."""

    digest: str
    size_bytes: int
    entries: List[ResponseFileEntry]


@dataclass(frozen=True)
class TreePreflightReport:
    """Summary of preflight analysis."""

    total_files: int
    total_bytes: int
    compressed_bytes: int
    compressed_ratio: float
    duplicates: List[TreePreflightDuplicate]
    recommendations: List[str]
    hash_strategy: str
    gpu_requested: bool
    gpu_used: bool
    gpu_reason: str


@dataclass(frozen=True)
class TreeSizeEstimate:
    """Lightweight estimate of tree payload size limits."""

    total_files: int
    total_bytes: int
    compressed_bytes: int
    compressed_ratio: float
    limit_bytes: int
    exceeds_limit: bool


class _Hasher:
    def __init__(self, *, use_gpu: bool) -> None:
        self._use_gpu = use_gpu
        self._torch = None
        self._device = None
        self._gpu_reason = "GPU hashing disabled."
        self._gpu_used = False

        if not use_gpu:
            self._hash_strategy = "cpu"
            return

        torch_spec = importlib.util.find_spec("torch")
        if torch_spec is None:
            self._hash_strategy = "cpu"
            self._gpu_reason = "torch is not installed."
            return

        import torch  # type: ignore[import-not-found]

        if not torch.cuda.is_available():
            self._hash_strategy = "cpu"
            self._gpu_reason = "CUDA/ROCm is not available."
            return

        self._torch = torch
        self._device = torch.device("cuda")
        self._hash_strategy = "gpu"
        device_name = "unknown device"
        try:
            device_name = torch.cuda.get_device_name(0)
        except (AttributeError, RuntimeError):
            pass

        if torch.version.hip:
            self._gpu_reason = f"GPU hashing enabled (ROCm: {device_name})."
        else:
            self._gpu_reason = f"GPU hashing enabled (CUDA: {device_name})."
        self._gpu_used = True

    @property
    def strategy(self) -> str:
        return self._hash_strategy

    @property
    def gpu_used(self) -> bool:
        return self._gpu_used

    @property
    def gpu_reason(self) -> str:
        return self._gpu_reason

    def hash_file(self, path: Path) -> str:
        if self._hash_strategy == "gpu":
            return self._hash_file_gpu(path)
        return self._hash_file_cpu(path)

    def _hash_file_cpu(self, path: Path) -> str:
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(_HASH_CHUNK_SIZE), b""):
                digest.update(chunk)
        return digest.hexdigest()

    def _hash_file_gpu(self, path: Path) -> str:
        torch = self._torch
        device = self._device
        if torch is None or device is None:
            return self._hash_file_cpu(path)

        checksum = 0
        total_sum = 0
        offset = 0

        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(_HASH_CHUNK_SIZE), b""):
                if hasattr(torch, "frombuffer"):
                    tensor = torch.frombuffer(memoryview(chunk), dtype=torch.uint8).to(device)
                else:
                    tensor = torch.tensor(list(chunk), dtype=torch.uint8, device=device)

                length = tensor.numel()
                indices = torch.arange(
                    offset + 1,
                    offset + length + 1,
                    dtype=torch.int64,
                    device=device,
                )
                data_i64 = tensor.to(torch.int64)
                checksum = (checksum + int((data_i64 * indices).sum().item())) & _GPU_HASH_MASK
                total_sum = (total_sum + int(data_i64.sum().item())) & _GPU_HASH_MASK
                offset += length

        digest = (checksum ^ (total_sum << 1) ^ offset) & _GPU_HASH_MASK
        return f"{digest:016x}"


def _format_bytes(value: int) -> str:
    units = ["B", "KB", "MB", "GB", "TB"]
    size = float(value)
    for unit in units:
        if size < 1024.0 or unit == units[-1]:
            return f"{size:.2f} {unit}"
        size /= 1024.0
    return f"{size:.2f} TB"


def _collect_entries(sources: Iterable[Path | str]) -> List[ResponseFileEntry]:
    builder = ResponseFileBuilder()
    return builder.build_entries(list(sources))


def estimate_tree_size(entries: Iterable[ResponseFileEntry]) -> TreeSizeEstimate:
    total_bytes = 0
    compressed_bytes = 0
    total_files = 0

    for entry in entries:
        size_bytes = entry.source.stat().st_size
        total_files += 1
        total_bytes += size_bytes
        if entry.source.suffix.lower() in _COMPRESSED_EXTENSIONS:
            compressed_bytes += size_bytes

    compressed_ratio = compressed_bytes / total_bytes if total_bytes else 0.0
    exceeds_limit = total_bytes > TREEFILE_OFFSET_LIMIT

    return TreeSizeEstimate(
        total_files=total_files,
        total_bytes=total_bytes,
        compressed_bytes=compressed_bytes,
        compressed_ratio=compressed_ratio,
        limit_bytes=TREEFILE_OFFSET_LIMIT,
        exceeds_limit=exceeds_limit,
    )


def _recommend_compression(
    total_bytes: int,
    compressed_bytes: int,
) -> str:
    if total_bytes <= 0:
        return "No files detected; compression recommendation unavailable."

    ratio = compressed_bytes / total_bytes
    if ratio >= 0.8:
        return (
            "Most data is already compressed; consider disabling file compression "
            "for faster builds."
        )
    if ratio <= 0.2:
        return (
            "Most data is uncompressed; keep file compression enabled to reduce "
            "archive size."
        )
    return "Mixed content; default compression settings are likely appropriate."


def _recommend_encryption(encrypting: bool) -> str:
    if encrypting:
        return "Encryption enabled for .tres output. Store the passphrase securely."
    return "Encryption disabled. Consider enabling it for sensitive content."


def run_tree_preflight(
    sources: Iterable[Path | str],
    *,
    use_gpu: bool = False,
    encrypting: bool = False,
    status_callback: Optional[Callable[[str], None]] = None,
) -> TreePreflightReport:
    def emit(message: str) -> None:
        if status_callback is not None:
            status_callback(message)

    emit("[INFO] Running preflight scan...")

    try:
        entries = _collect_entries(sources)
    except ResponseFileBuilderError as exc:
        emit(f"[ERROR] {exc}")
        raise

    hasher = _Hasher(use_gpu=use_gpu)

    total_bytes = 0
    compressed_bytes = 0
    hash_map: dict[tuple[str, int], List[ResponseFileEntry]] = {}

    for entry in entries:
        size_bytes = entry.source.stat().st_size
        total_bytes += size_bytes
        if entry.source.suffix.lower() in _COMPRESSED_EXTENSIONS:
            compressed_bytes += size_bytes

        digest = hasher.hash_file(entry.source)
        hash_map.setdefault((digest, size_bytes), []).append(entry)

    duplicates = [
        TreePreflightDuplicate(digest=digest, size_bytes=size_bytes, entries=group)
        for (digest, size_bytes), group in hash_map.items()
        if len(group) > 1
    ]

    if hasher.strategy == "gpu" and duplicates:
        cpu_hasher = _Hasher(use_gpu=False)
        cpu_hashes: dict[tuple[str, int], List[ResponseFileEntry]] = {}
        for duplicate in duplicates:
            for entry in duplicate.entries:
                cpu_digest = cpu_hasher.hash_file(entry.source)
                cpu_hashes.setdefault((cpu_digest, duplicate.size_bytes), []).append(entry)
        duplicates = [
            TreePreflightDuplicate(digest=digest, size_bytes=size_bytes, entries=group)
            for (digest, size_bytes), group in cpu_hashes.items()
            if len(group) > 1
        ]

    compressed_ratio = compressed_bytes / total_bytes if total_bytes else 0.0
    recommendations = [
        _recommend_compression(total_bytes, compressed_bytes),
        _recommend_encryption(encrypting),
    ]

    emit(
        f"[INFO] Preflight scanned {len(entries)} files "
        f"({_format_bytes(total_bytes)} total)."
    )

    return TreePreflightReport(
        total_files=len(entries),
        total_bytes=total_bytes,
        compressed_bytes=compressed_bytes,
        compressed_ratio=compressed_ratio,
        duplicates=duplicates,
        recommendations=recommendations,
        hash_strategy=hasher.strategy,
        gpu_requested=use_gpu,
        gpu_used=hasher.gpu_used,
        gpu_reason=hasher.gpu_reason,
    )


def format_preflight_report(report: TreePreflightReport) -> List[str]:
    lines = [
        f"[INFO] Preflight totals: {report.total_files} files, "
        f"{_format_bytes(report.total_bytes)}.",
        f"[INFO] Compressed data estimate: {_format_bytes(report.compressed_bytes)} "
        f"({report.compressed_ratio * 100:.1f}%).",
    ]
    if report.total_bytes > TREEFILE_OFFSET_LIMIT:
        lines.append(
            "[WARN] Total payload exceeds the 32-bit offset limit "
            f"({_format_bytes(TREEFILE_OFFSET_LIMIT)}). Split the archive or "
            "remove content before building."
        )

    if report.gpu_requested and not report.gpu_used:
        lines.append(f"[WARN] GPU hashing requested but unavailable: {report.gpu_reason}")
    else:
        lines.append(f"[INFO] Hash strategy: {report.hash_strategy} ({report.gpu_reason})")

    if report.duplicates:
        lines.append(
            f"[WARN] Detected {len(report.duplicates)} duplicate content group(s)."
        )
        for duplicate in report.duplicates[:5]:
            paths = ", ".join(str(entry.source) for entry in duplicate.entries[:3])
            remainder = ""
            if len(duplicate.entries) > 3:
                remainder = f" (+{len(duplicate.entries) - 3} more)"
            lines.append(
                "[WARN] Duplicate group "
                f"{duplicate.digest} ({_format_bytes(duplicate.size_bytes)}): "
                f"{paths}{remainder}"
            )
        if len(report.duplicates) > 5:
            lines.append(
                f"[WARN] {len(report.duplicates) - 5} additional duplicate group(s) omitted."
            )
    else:
        lines.append("[INFO] No duplicate content detected.")

    for recommendation in report.recommendations:
        lines.append(f"[INFO] {recommendation}")

    return lines


__all__ = [
    "TreePreflightDuplicate",
    "TreePreflightReport",
    "TreeSizeEstimate",
    "TREEFILE_OFFSET_LIMIT",
    "run_tree_preflight",
    "format_preflight_report",
    "estimate_tree_size",
]
