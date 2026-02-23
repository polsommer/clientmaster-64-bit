#!/usr/bin/env python3
"""Convenience entrypoint for the TreeFileBuilder GUI shipped with swg-tool."""
from __future__ import annotations

import sys
from pathlib import Path

SWG_TOOL_PATH = Path(__file__).resolve().parent / "swg-tool"
if str(SWG_TOOL_PATH) not in sys.path:
    sys.path.append(str(SWG_TOOL_PATH))

from swg_tool.treefile_gui import main


if __name__ == "__main__":
    main()
