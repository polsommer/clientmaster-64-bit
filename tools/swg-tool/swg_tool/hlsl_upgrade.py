"""Upgrade legacy HLSL vertex shader headers."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional
import re
import concurrent.futures
import importlib
import importlib.util
import os
import time


DEFAULT_INPUT_DIR = Path("clientHLSL files")
DEFAULT_PATTERN = "*.vsh"
DEFAULT_TARGET_PROFILE = "vs_4_0"
SUPPORTED_DX9_PROFILES = {"vs_1_1", "vs_2_0", "ps_2_0", "ps_3_0"}
SUPPORTED_DX10_PROFILES = {"vs_4_0", "ps_4_0"}
DX9_MAX_PROFILES = {"vs": "vs_2_0", "ps": "ps_3_0"}
PROFILE_TOKEN_PATTERN = re.compile(r"^(?:vs|ps)_\d_\d$", re.IGNORECASE)
PROFILE_EXTRACT_PATTERN = re.compile(r"(?:vs|ps)_\d_\d", re.IGNORECASE)
DEFAULT_TARGET_BY_FAMILY = {"vs": "vs_4_0", "ps": "ps_4_0"}


@dataclass(frozen=True)
class UpgradeResult:
    path: Path
    output_path: Path
    changed: bool
    reason: str
    old_header: Optional[str] = None
    new_header: Optional[str] = None


@dataclass(frozen=True)
class UpgradeSummary:
    duration_s: float
    total_files: int
    changed_files: int
    skipped_files: int
    results: tuple[UpgradeResult, ...]
    gpu_used: bool
    gpu_reason: str


class GpuHeaderMatcher:
    def __init__(self, enabled: bool) -> None:
        self.enabled = enabled
        self.available = False
        self.reason = "GPU acceleration disabled"
        self._torch = None
        self._patterns = None
        self._device = None

        if not enabled:
            return

        spec = importlib.util.find_spec("torch")
        if spec is None:
            self.reason = "torch not installed"
            return

        torch = importlib.import_module("torch")
        if not torch.cuda.is_available():
            self.reason = "GPU device not available"
            return

        self.available = True
        self._torch = torch
        self._device = torch.device("cuda")
        self._patterns = [
            torch.tensor(list(profile.encode("ascii")), device=self._device, dtype=torch.uint8)
            for profile in SUPPORTED_DX9_PROFILES.union(SUPPORTED_DX10_PROFILES)
        ]
        if torch.version.hip:
            self.reason = "GPU acceleration enabled (ROCm)"
        else:
            self.reason = "GPU acceleration enabled (CUDA)"

    def contains_profile_token(self, line: str) -> bool:
        if not self.available:
            return bool(PROFILE_TOKEN_PATTERN.search(line))

        torch = self._torch
        encoded = line.encode("utf-8", errors="ignore")
        for pattern in self._patterns or []:
            if len(encoded) < len(pattern):
                continue
            haystack = torch.tensor(list(encoded), device=self._device, dtype=torch.uint8)
            windows = haystack.unfold(0, pattern.numel(), 1)
            matches = (windows == pattern).all(dim=1)
            if bool(matches.any().item()):
                return True
        return False


def _profile_family(profile: str) -> Optional[str]:
    lowered = profile.lower()
    if lowered.startswith("vs_"):
        return "vs"
    if lowered.startswith("ps_"):
        return "ps"
    return None


def normalize_target_profile(target_profile: str, enforce_dx9: bool) -> tuple[str, Optional[str]]:
    normalized = target_profile.strip().lower()
    if not enforce_dx9:
        return normalized, None
    if normalized in SUPPORTED_DX9_PROFILES:
        return normalized, None

    family = _profile_family(normalized)
    fallback = DX9_MAX_PROFILES.get(family, DX9_MAX_PROFILES["vs"])
    return fallback, (
        f"Target {target_profile} exceeds Direct3D 9 support; using {fallback}."
    )


def _dedupe_profiles(profiles: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    deduped: list[str] = []
    for profile in profiles:
        if profile in seen:
            continue
        seen.add(profile)
        deduped.append(profile)
    return deduped


def _extract_profiles(line: str) -> list[str]:
    return _dedupe_profiles(
        match.group(0).lower() for match in PROFILE_EXTRACT_PATTERN.finditer(line)
    )


def _normalize_header_line(line: str) -> tuple[str, list[str]]:
    line_content = line.rstrip("\r\n")
    leading = line_content[: len(line_content) - len(line_content.lstrip())]
    profiles = _extract_profiles(line_content)
    return leading, profiles


def _is_profile_token(token: str) -> bool:
    return bool(PROFILE_TOKEN_PATTERN.match(token))


def _parse_profile(profile: str) -> Optional[tuple[str, int, int]]:
    match = PROFILE_TOKEN_PATTERN.match(profile)
    if not match:
        return None
    family, major, minor = profile.lower().split("_")
    return family, int(major), int(minor)


def _is_older_profile(profile: str, target_profile: str) -> bool:
    parsed_profile = _parse_profile(profile)
    parsed_target = _parse_profile(target_profile)
    if not parsed_profile or not parsed_target:
        return False
    if parsed_profile[0] != parsed_target[0]:
        return False
    return (parsed_profile[1], parsed_profile[2]) < (parsed_target[1], parsed_target[2])


def upgrade_hlsl_header_line(
    line: str,
    target_profile: str,
    mode: str,
    enforce_dx9: bool,
) -> tuple[str, Optional[str]]:
    target_profile = target_profile.strip().lower()
    leading, profiles = _normalize_header_line(line)
    updated = list(profiles)
    target_family = _profile_family(target_profile)
    inferred_target = False
    if not updated and target_profile:
        updated = [target_profile]
        inferred_target = True
    elif not any(_is_profile_token(profile) for profile in updated):
        return line, None
    profile_indices = [
        index
        for index, profile in enumerate(updated)
        if _is_profile_token(profile)
        and (target_family is None or _profile_family(profile) == target_family)
    ]
    all_profile_indices = [
        index for index, profile in enumerate(updated) if _is_profile_token(profile)
    ]
    if mode == "smart":
        target_for_family = target_profile
        if not target_family:
            target_for_family = target_profile
        if target_family in DEFAULT_TARGET_BY_FAMILY:
            target_for_family = DEFAULT_TARGET_BY_FAMILY[target_family]

        family_profiles = [
            profile
            for profile in updated
            if _is_profile_token(profile) and _profile_family(profile) == target_family
        ]
        if target_for_family not in updated and family_profiles:
            if all(_is_older_profile(profile, target_for_family) for profile in family_profiles):
                updated = [
                    profile
                    for profile in updated
                    if not (_is_profile_token(profile) and _profile_family(profile) == target_family)
                ]
                updated.append(target_for_family)
            else:
                insert_at = max(profile_indices or all_profile_indices, default=-1) + 1
                updated.insert(insert_at, target_for_family)
        elif target_for_family and target_for_family not in updated:
            updated.append(target_for_family)
    elif mode == "replace":
        updated = [
            profile
            for profile in updated
            if not (_is_profile_token(profile) and _profile_family(profile) == target_family)
        ]
        if target_profile not in updated:
            updated.append(target_profile)
    else:
        if target_profile in updated and not inferred_target:
            return line, None
        insert_at = max(profile_indices or all_profile_indices, default=-1) + 1
        updated.insert(insert_at, target_profile)

    if enforce_dx9:
        updated = [
            profile
            for profile in updated
            if not _is_profile_token(profile) or profile in SUPPORTED_DX9_PROFILES
        ]
        if target_profile not in updated:
            updated.append(target_profile)

    updated = _dedupe_profiles(updated)
    new_header = f"{leading}//hlsl {' '.join(updated)}".rstrip()
    return new_header, new_header


def upgrade_hlsl_content(
    content: str,
    target_profile: str,
    mode: str,
    enforce_dx9: bool,
    matcher: Optional[GpuHeaderMatcher] = None,
) -> tuple[str, Optional[str], Optional[str], str]:
    lines = content.splitlines(keepends=True)
    if not lines:
        return content, None, None, "empty file"

    first_line = lines[0]
    stripped = first_line.lstrip()
    if not stripped.startswith("//hlsl"):
        return content, None, None, "missing hlsl header"

    profiles = _extract_profiles(first_line)
    if not profiles and matcher:
        matcher.contains_profile_token(first_line)

    new_header, updated = upgrade_hlsl_header_line(
        first_line,
        target_profile,
        mode,
        enforce_dx9,
    )
    if updated is None or new_header == first_line.rstrip("\r\n"):
        return content, first_line.rstrip("\r\n"), None, "already upgraded"

    line_ending = ""
    if first_line.endswith("\r\n"):
        line_ending = "\r\n"
    elif first_line.endswith("\n"):
        line_ending = "\n"
    lines[0] = f"{new_header}{line_ending}"
    return "".join(lines), first_line.rstrip("\r\n"), new_header, "upgraded"


def upgrade_hlsl_file(
    path: Path,
    output_path: Path,
    target_profile: str,
    mode: str,
    enforce_dx9: bool,
    matcher: Optional[GpuHeaderMatcher],
    dry_run: bool,
) -> UpgradeResult:
    content = path.read_text(encoding="utf-8", errors="surrogateescape")
    updated_content, old_header, new_header, reason = upgrade_hlsl_content(
        content,
        target_profile,
        mode,
        enforce_dx9,
        matcher,
    )
    changed = updated_content != content

    if changed and not dry_run:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(updated_content, encoding="utf-8", errors="surrogateescape")

    return UpgradeResult(
        path=path,
        output_path=output_path,
        changed=changed,
        reason=reason,
        old_header=old_header,
        new_header=new_header,
    )


def upgrade_hlsl_in_dir(
    input_dir: Path,
    output_dir: Optional[Path],
    target_profile: str,
    mode: str,
    pattern: str,
    workers: Optional[int],
    use_gpu: bool,
    dry_run: bool,
    enforce_dx9: bool,
) -> UpgradeSummary:
    matcher = GpuHeaderMatcher(use_gpu)
    start = time.perf_counter()

    files = sorted(input_dir.rglob(pattern))

    if output_dir:
        output_dir.mkdir(parents=True, exist_ok=True)

    def _target_path(source: Path) -> Path:
        if not output_dir:
            return source
        return output_dir / source.relative_to(input_dir)

    results: list[UpgradeResult] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {
            executor.submit(
                upgrade_hlsl_file,
                path,
                _target_path(path),
                target_profile,
                mode,
                enforce_dx9,
                matcher,
                dry_run,
            ): path
            for path in files
        }
        for future in concurrent.futures.as_completed(futures):
            results.append(future.result())

    duration = time.perf_counter() - start
    changed = sum(1 for result in results if result.changed)
    skipped = len(results) - changed

    return UpgradeSummary(
        duration_s=duration,
        total_files=len(results),
        changed_files=changed,
        skipped_files=skipped,
        results=tuple(sorted(results, key=lambda item: item.path.as_posix())),
        gpu_used=matcher.available,
        gpu_reason=matcher.reason,
    )


def iter_changed_results(summary: UpgradeSummary) -> Iterable[UpgradeResult]:
    return (result for result in summary.results if result.changed)


__all__ = [
    "DEFAULT_INPUT_DIR",
    "DEFAULT_PATTERN",
    "DEFAULT_TARGET_PROFILE",
    "SUPPORTED_DX9_PROFILES",
    "SUPPORTED_DX10_PROFILES",
    "GpuHeaderMatcher",
    "normalize_target_profile",
    "UpgradeResult",
    "UpgradeSummary",
    "iter_changed_results",
    "upgrade_hlsl_content",
    "upgrade_hlsl_file",
    "upgrade_hlsl_header_line",
    "upgrade_hlsl_in_dir",
]
