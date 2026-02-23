"""Prototype navmesh generation utilities."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class Heightmap:
    width: int
    height: int
    samples: List[float]

    def sample(self, x: int, y: int) -> float:
        return self.samples[y * self.width + x]


class NavMeshGenerator:
    def __init__(self, walkable_threshold: float = 15.0) -> None:
        self.walkable_threshold = walkable_threshold

    def generate_from_heightmap(self, heightmap_path: Path) -> dict:
        heightmap = self._load_heightmap(heightmap_path)
        cells = []
        for y in range(heightmap.height - 1):
            for x in range(heightmap.width - 1):
                slope = self._estimate_slope(heightmap, x, y)
                walkable = slope <= self.walkable_threshold
                cells.append(
                    {
                        "x": x,
                        "y": y,
                        "slope": round(slope, 3),
                        "walkable": walkable,
                    }
                )
        return {
            "metadata": {
                "source": str(heightmap_path),
                "walkableThreshold": self.walkable_threshold,
                "gridSize": [heightmap.width, heightmap.height],
            },
            "cells": cells,
        }

    def _load_heightmap(self, path: Path) -> Heightmap:
        rows = [line.strip() for line in path.read_text().splitlines() if line.strip()]
        grid = [[float(value) for value in row.split()] for row in rows]
        height = len(grid)
        if height == 0:
            raise ValueError("heightmap is empty")
        width = len(grid[0])
        if any(len(row) != width for row in grid):
            raise ValueError("heightmap rows must be rectangular")
        samples = [value for row in grid for value in row]
        return Heightmap(width=width, height=height, samples=samples)

    @staticmethod
    def _estimate_slope(heightmap: Heightmap, x: int, y: int) -> float:
        center = heightmap.sample(x, y)
        right = heightmap.sample(x + 1, y)
        bottom = heightmap.sample(x, y + 1)
        dx = right - center
        dy = bottom - center
        gradient = math.sqrt(dx * dx + dy * dy)
        slope_rad = math.atan2(gradient, 1.0)
        return math.degrees(slope_rad)


__all__ = ["NavMeshGenerator"]
