"""Utility package for the SWG God Client workflow automation CLI."""

from importlib.metadata import version, PackageNotFoundError

try:  # pragma: no cover - metadata only available when installed as a package
    __version__ = version("swg-tool")
except PackageNotFoundError:  # pragma: no cover - fallback for in-tree usage
    __version__ = "0.1.0"

__all__ = ["__version__"]
