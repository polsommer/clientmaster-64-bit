#!/usr/bin/env python3
"""Convenience entry point for the swg-tool CLI when running from the repository root."""

from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str((REPO_ROOT / "swg-tool").resolve()))

from swg_tool.cli import main  # noqa: E402  pylint: disable=wrong-import-position

if __name__ == "__main__":
    main()
