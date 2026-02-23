"""Content publishing helpers."""

from __future__ import annotations

import json
import shutil
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Optional


class PublishError(RuntimeError):
    """Raised when a publish operation cannot be completed."""


@dataclass
class PublishResult:
    version: str
    output_directory: Path
    manifest_path: Path


class Publisher:
    """Creates versioned publish bundles for the modernised toolchain."""

    VERSION_FILE = "releases.json"

    def publish(
        self,
        content_directory: Path,
        destination: Path,
        *,
        manifest: Optional[Dict[str, object]] = None,
        label: str = "snapshot",
    ) -> PublishResult:
        if not content_directory.exists():
            raise PublishError(f"Content directory does not exist: {content_directory}")

        releases = self._load_versions(destination)
        next_version = releases.get("next", 1)
        version = f"{datetime.now(timezone.utc):%Y.%m.%d}.{next_version:03d}"

        bundle_dir = destination / version
        bundle_dir.mkdir(parents=True, exist_ok=False)

        for item in content_directory.iterdir():
            target = bundle_dir / item.name
            if item.is_dir():
                shutil.copytree(item, target)
            else:
                shutil.copy2(item, target)

        metadata = {
            "label": label,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "version": version,
        }
        if manifest is not None:
            metadata["manifest"] = manifest

        manifest_path = bundle_dir / "publish.json"
        manifest_path.write_text(json.dumps(metadata, indent=2))

        releases.setdefault("history", []).append(metadata)
        releases["next"] = next_version + 1
        (destination / self.VERSION_FILE).write_text(json.dumps(releases, indent=2))

        return PublishResult(version=version, output_directory=bundle_dir, manifest_path=manifest_path)

    def _load_versions(self, destination: Path) -> Dict[str, object]:
        version_file = destination / self.VERSION_FILE
        if not version_file.exists():
            return {"next": 1, "history": []}
        return json.loads(version_file.read_text())


__all__ = ["Publisher", "PublishResult", "PublishError"]
