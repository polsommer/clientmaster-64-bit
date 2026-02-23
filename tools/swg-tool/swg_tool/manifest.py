"""Manifest validation helpers."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class ValidationError:
    path: Path
    message: str

    def __str__(self) -> str:
        return f"{self.path}: {self.message}"


class ManifestValidator:
    """Performs light-weight structural validation on plugin/content manifests."""

    REQUIRED_FIELDS = {"name", "version", "entryPoint", "library"}

    def validate_manifest(self, manifest_path: Path) -> List[ValidationError]:
        try:
            data = json.loads(manifest_path.read_text())
        except json.JSONDecodeError as exc:
            return [ValidationError(manifest_path, f"invalid JSON: {exc}")]

        errors: List[ValidationError] = []

        missing = self.REQUIRED_FIELDS - data.keys()
        for field in sorted(missing):
            errors.append(ValidationError(manifest_path, f"missing required field '{field}'"))

        version = data.get("version")
        if isinstance(version, str):
            parts = version.split(".")
            if len(parts) != 3 or not all(part.isdigit() for part in parts):
                errors.append(ValidationError(manifest_path, "version must follow MAJOR.MINOR.PATCH"))
        else:
            errors.append(ValidationError(manifest_path, "version must be a string"))

        api_version = data.get("apiVersion", {})
        if not isinstance(api_version, dict):
            errors.append(ValidationError(manifest_path, "apiVersion must be an object"))
        else:
            if "minimum" not in api_version or "maximum" not in api_version:
                errors.append(ValidationError(manifest_path, "apiVersion.minimum and apiVersion.maximum are required"))

        deps = data.get("dependencies", {})
        if deps and not isinstance(deps, dict):
            errors.append(ValidationError(manifest_path, "dependencies must be an object"))

        return errors


__all__ = ["ManifestValidator", "ValidationError"]
