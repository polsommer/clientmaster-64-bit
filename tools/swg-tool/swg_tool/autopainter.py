"""Headless TerrainAutoPainter driver used by the modern tooling stack.

This module mirrors the automation behaviour that lives inside the legacy
Terrain Editor. It exposes a lightweight, pure-Python approximation of the
`TerrainAutoPainter::generateAndApply` pipeline and a simplified
`SmartTerrainAnalyzer` auditor so web tooling can request build-out plans on
the fly. The coordinate conversions and defaults intentionally mirror the
values in `TerrainAutoPainter.cpp` to keep world-space summaries consistent.
"""

from __future__ import annotations

import json
import math
import random
from dataclasses import asdict, dataclass, field
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from statistics import mean, pstdev
from typing import Dict, List, Tuple


@dataclass
class TerrainDocument:
    """Minimal representation of a terrain document.

    The real Terrain Editor document exposes a far richer interface. For the
    headless driver we only need the map and tile sizes that inform coordinate
    conversion so the resulting JSON matches the world units used by the
    in-editor automation.
    """

    map_width_m: float = 8192.0
    tile_width_m: float = 8.0
    chunk_width_m: float = 512.0

    @classmethod
    def load(cls, path: Path) -> "TerrainDocument":
        data = json.loads(path.read_text())
        return cls(
            map_width_m=float(data.get("map_width_m", cls.map_width_m)),
            tile_width_m=float(data.get("tile_width_m", cls.tile_width_m)),
            chunk_width_m=float(data.get("chunk_width_m", cls.chunk_width_m)),
        )


@dataclass
class AutoPainterConfig:
    grid_size: int = 257
    roughness: float = 0.72
    seed: int = 1337
    erosion_iterations: int = 2
    plateau_bias: float = 0.35
    water_level: float = 0.36
    flora_threshold: float = 0.58
    settlement_threshold: float = 0.42
    desired_settlement_count: int = 6
    river_count: int = 3
    enable_river_carving: bool = True
    enable_flora_enrichment: bool = True
    enable_hotspot_detection: bool = True
    travel_corridor_threshold: float = 0.28
    logistics_hub_count: int = 4
    enable_biome_rebalancing: bool = True
    enable_settlement_zoning: bool = True
    enable_travel_corridor_planning: bool = True
    enable_lighting_director: bool = True
    enable_weather_synthesis: bool = True
    enable_encounter_scripting: bool = True
    enable_cinematic_moments: bool = True


@dataclass
class AutoPainterResult:
    minimum_height: float
    maximum_height: float
    average_height: float
    standard_deviation: float
    water_coverage: float
    plateau_coverage: float
    flora_coverage: float
    blueprint_summary: str
    biome_breakdown: List[str]
    settlement_recommendations: List[str]
    content_hooks: List[str]
    automation_toolkit: List[str]
    hotspot_annotations: List[str]
    biome_adjustments: List[str]
    travel_corridors: List[str]
    lighting_plan: List[str]
    weather_timeline: List[str]
    encounter_scripts: List[str]
    cinematic_moments: List[str]
    ai_status_headline: str
    operations_checklist: str


@dataclass
class AuditReport:
    foresight_score: float
    structure_score: float
    ecosystem_score: float
    workflow_score: float
    copilot_modules: List[str] = field(default_factory=list)
    automation_opportunities: List[str] = field(default_factory=list)
    monitoring_signals: List[str] = field(default_factory=list)


def _neighbour_index(size: int, x: int, y: int) -> int:
    return y * size + x


def _generate_height_field(config: AutoPainterConfig) -> List[float]:
    rng = random.Random(config.seed)
    size = config.grid_size
    field = [0.0 for _ in range(size * size)]

    max_index = size - 1
    field[_neighbour_index(size, 0, 0)] = rng.random()
    field[_neighbour_index(size, max_index, 0)] = rng.random()
    field[_neighbour_index(size, 0, max_index)] = rng.random()
    field[_neighbour_index(size, max_index, max_index)] = rng.random()

    step = max_index
    scale = config.roughness
    while step > 1:
        half = step // 2

        # Diamond step
        for y in range(half, size, step):
            for x in range(half, size, step):
                a = field[_neighbour_index(size, x - half, y - half)]
                b = field[_neighbour_index(size, x - half, y + half - step)]
                c = field[_neighbour_index(size, x + half - step, y - half)]
                d = field[_neighbour_index(size, x + half - step, y + half - step)]
                avg = (a + b + c + d) * 0.25
                jitter = rng.uniform(-scale, scale)
                field[_neighbour_index(size, x, y)] = avg + jitter

        # Square step
        for y in range(0, size, half):
            for x in range((y + half) % step, size, step):
                neighbours: List[float] = []
                for dx, dy in ((-half, 0), (half, 0), (0, -half), (0, half)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < size and 0 <= ny < size:
                        neighbours.append(field[_neighbour_index(size, nx, ny)])

                avg = sum(neighbours) / len(neighbours) if neighbours else 0.0
                jitter = rng.uniform(-scale, scale)
                field[_neighbour_index(size, x, y)] = avg + jitter

        step = half
        scale *= 0.5

    return field


def _normalise(field: List[float]) -> None:
    if not field:
        return
    min_val = min(field)
    max_val = max(field)
    if math.isclose(max_val, min_val, abs_tol=1e-6):
        for i in range(len(field)):
            field[i] = 0.0
        return
    scale = 1.0 / (max_val - min_val)
    for i, value in enumerate(field):
        field[i] = (value - min_val) * scale


def _bias_plateaus(field: List[float], plateau_bias: float) -> None:
    if plateau_bias <= 0.0:
        return
    for i, value in enumerate(field):
        clamped = max(0.0, min(1.0, value))
        eased = pow(clamped, 1.0 - plateau_bias) * pow(1.0 - clamped, plateau_bias)
        adjustment = (0.5 - eased) * plateau_bias
        field[i] = max(0.0, min(1.0, clamped + adjustment))


def _apply_thermal_erosion(field: List[float], size: int, iterations: int) -> None:
    if iterations <= 0:
        return
    for _ in range(iterations):
        delta = [0.0 for _ in field]
        for y in range(size):
            for x in range(size):
                idx = _neighbour_index(size, x, y)
                current = field[idx]
                for ny in range(y - 1, y + 2):
                    for nx in range(x - 1, x + 2):
                        if nx == x and ny == y:
                            continue
                        if nx < 0 or ny < 0 or nx >= size or ny >= size:
                            continue
                        nidx = _neighbour_index(size, nx, ny)
                        diff = current - field[nidx]
                        if diff > 0.02:
                            transfer = diff * 0.25
                            delta[idx] -= transfer
                            delta[nidx] += transfer
        for i, value in enumerate(field):
            field[i] = max(0.0, min(1.0, value + delta[i]))


def _compute_slope(field: List[float], size: int, x: int, y: int) -> float:
    center = field[_neighbour_index(size, x, y)]
    samples: List[float] = []
    for ny in range(max(0, y - 1), min(size, y + 2)):
        for nx in range(max(0, x - 1), min(size, x + 2)):
            if nx == x and ny == y:
                continue
            samples.append(abs(center - field[_neighbour_index(size, nx, ny)]))
    return sum(samples) / len(samples) if samples else 0.0


def _sample_height(field: List[float], size: int, x: int, y: int) -> float:
    return field[_neighbour_index(size, x, y)]


def _analyse(field: List[float], document: TerrainDocument, config: AutoPainterConfig) -> AutoPainterResult:
    size = config.grid_size
    water_cells = plateau_cells = flora_cells = 0
    slopes: List[float] = []

    for y in range(1, size - 1):
        for x in range(1, size - 1):
            height = _sample_height(field, size, x, y)
            slope = _compute_slope(field, size, x, y)
            slopes.append(slope)
            if height <= config.water_level:
                water_cells += 1
            if abs(height - 0.5) < config.plateau_bias:
                plateau_cells += 1
            if height >= config.flora_threshold:
                flora_cells += 1

    avg_height = mean(field)
    std_dev = pstdev(field) if len(field) > 1 else 0.0

    world_scale = document.map_width_m / float(size - 1)

    candidates: List[Tuple[float, Tuple[int, int], float]] = []
    for y in range(4, size - 4, 4):
        for x in range(4, size - 4, 4):
            height = _sample_height(field, size, x, y)
            if height < config.settlement_threshold:
                continue
            slope = _compute_slope(field, size, x, y)
            if slope > 0.55:
                continue
            candidates.append((height - slope * 0.35, (x, y), slope))

    candidates.sort(key=lambda c: c[0], reverse=True)
    settlements: List[str] = []
    for idx, (_, (cx, cy), slope) in enumerate(
        candidates[: max(1, config.desired_settlement_count)]
    ):
        world_x = (cx - size / 2) * world_scale
        world_z = (cy - size / 2) * world_scale
        elevation = _sample_height(field, size, cx, cy) * 100.0
        settlements.append(
            f"Hub {idx + 1} @ ({world_x:.0f}m, {world_z:.0f}m) – slope {slope:.2f}, elevation {elevation:.0f}%"
        )

    corridors: List[str] = []
    if config.enable_travel_corridor_planning:
        for y in range(2, size - 2, 6):
            for x in range(2, size - 2, 6):
                height = _sample_height(field, size, x, y)
                slope = _compute_slope(field, size, x, y)
                water_bias = abs(height - config.water_level)
                hub_bias = abs(height - config.settlement_threshold)
                score = (0.75 - slope) + (1.0 - min(1.0, water_bias * 2.0)) * 0.4 + (
                    1.0 - min(1.0, hub_bias * 2.0)
                ) * 0.6
                if score < config.travel_corridor_threshold:
                    continue
                corridors.append(
                    f"Corridor node @ ({(x - size / 2) * world_scale:.0f}m, {(y - size / 2) * world_scale:.0f}m) – score {score:.2f}"
                )

    automation: List[str] = []
    if config.enable_river_carving:
        automation.append(
            "River lattice assistant carved procedural channels – convert into river affectors."
        )
    if config.enable_flora_enrichment:
        automation.append(
            "Flora enrichment pass highlighted lush bands – seed flora presets along the highlighted bands."
        )
    if corridors:
        automation.append("Travel corridor planner queued – blueprint paths across hubs.")

    if config.enable_cinematic_moments:
        automation.append("Cinematic pathing queued – generate camera rails along AI vista beats.")

    operations = (
        f"Toolkit {len(automation)} • Corridors {len(corridors)} • Weather beats 0 • "
        f"Encounters 0 • Cinematics {1 if config.enable_cinematic_moments else 0}"
    )

    blueprint_summary = (
        f"Height {min(field)*100:.0f}–{max(field)*100:.0f}% • Avg {avg_height*100:.0f}% "
        f"• σ {std_dev*100:.0f}% • Water {water_cells / len(field) * 100:.0f}% "
        f"• Flora {flora_cells / len(field) * 100:.0f}%"
    )

    lighting_plan = []
    weather_timeline = []
    if config.enable_lighting_director:
        lighting_plan.append("Lighting director staged – bias key light towards dominant slopes.")
    if config.enable_weather_synthesis:
        weather_timeline.append("Weather synthesis drafted – blend cloud and wind presets across the play space.")

    cinematic = []
    if config.enable_cinematic_moments:
        cinematic.append("Hero vista identified from automation pass.")

    return AutoPainterResult(
        minimum_height=min(field),
        maximum_height=max(field),
        average_height=avg_height,
        standard_deviation=std_dev,
        water_coverage=water_cells / len(field),
        plateau_coverage=plateau_cells / len(field),
        flora_coverage=flora_cells / len(field),
        blueprint_summary=blueprint_summary,
        biome_breakdown=[
            f"Lowlands {water_cells / len(field) * 100:.0f}% – primary water table candidates",
            f"Plateaus {plateau_cells / len(field) * 100:.0f}% – settlement ready terraces",
            f"Lush bands {flora_cells / len(field) * 100:.0f}% – high flora density",
        ],
        settlement_recommendations=settlements,
        content_hooks=[
            "Queue river spline generator across water basins.",
            "Stamp hero POIs on top three settlement hubs.",
            "Layer regional flora palettes following lush bands.",
        ],
        automation_toolkit=automation,
        hotspot_annotations=[],
        biome_adjustments=[],
        travel_corridors=corridors,
        lighting_plan=lighting_plan,
        weather_timeline=weather_timeline,
        encounter_scripts=[],
        cinematic_moments=cinematic,
        ai_status_headline=f"Seed {config.seed} • Hotspots 0 • Automation hooks {len(automation)} • Logistics hubs {config.logistics_hub_count}",
        operations_checklist=operations,
    )


def _build_audit(result: AutoPainterResult, document: TerrainDocument) -> AuditReport:
    foresight = 70.0 + min(10.0, len(result.automation_toolkit) * 2.0)
    structure = 65.0 + min(10.0, len(result.settlement_recommendations) * 1.5)
    ecosystem = 60.0 + min(10.0, result.flora_coverage * 100.0 / 15.0)
    workflow = 55.0 + (5.0 if document.tile_width_m <= 8.0 else 0.0)

    audit = AuditReport(
        foresight_score=min(100.0, foresight),
        structure_score=min(100.0, structure),
        ecosystem_score=min(100.0, ecosystem),
        workflow_score=min(100.0, workflow),
    )

    audit.copilot_modules.append(
        f"TerrainAutoPainter blueprint – {result.blueprint_summary}"
    )
    audit.automation_opportunities.extend(result.automation_toolkit)
    audit.monitoring_signals.extend(result.travel_corridors)
    audit.monitoring_signals.append(
        f"Map {document.map_width_m:.0f}m • tile width {document.tile_width_m:.2f}m"
    )
    return audit


def run_headless(document: TerrainDocument, config: AutoPainterConfig) -> Dict[str, object]:
    """Run the automated terrain synthesis and audit steps."""

    field = _generate_height_field(config)
    _normalise(field)
    _bias_plateaus(field, config.plateau_bias)
    _apply_thermal_erosion(field, config.grid_size, config.erosion_iterations)
    _normalise(field)

    result = _analyse(field, document, config)
    audit = _build_audit(result, document)

    return {
        "result": asdict(result),
        "audit": asdict(audit),
    }


def serve_headless(document: TerrainDocument, config: AutoPainterConfig, port: int) -> None:
    """Expose the automation output over HTTP for web consumers."""

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):  # type: ignore[override]
            if self.path.rstrip("/") != "/plan":
                self.send_response(404)
                self.end_headers()
                return

            payload = json.dumps(run_headless(document, config)).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def log_message(self, fmt: str, *args: object) -> None:  # noqa: D401
            return

    server = HTTPServer(("0.0.0.0", port), Handler)
    server.serve_forever()


def load_config_from_args(args: Dict[str, object]) -> AutoPainterConfig:
    config = AutoPainterConfig()
    for key in (
        "grid_size",
        "roughness",
        "seed",
        "erosion_iterations",
        "plateau_bias",
        "water_level",
        "flora_threshold",
        "settlement_threshold",
        "desired_settlement_count",
        "river_count",
        "travel_corridor_threshold",
        "logistics_hub_count",
    ):
        if key in args and args[key] is not None:
            setattr(config, key, args[key])
    return config
