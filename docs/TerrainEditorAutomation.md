# Terrain Editor automation guide

The automation stack that previously lived in the standalone Python prototype
now ships directly inside the Terrain Editor. When a new terrain document is
created, the editor invokes the **TerrainAutoPainter** to synthesise a layered
heightfield, water table recommendations, and actionable authoring guidance.

## Auto generation pipeline

1. **Heightfield synthesis** – The TerrainAutoPainter runs an in-editor
   implementation of the diamond-square algorithm, thermal erosion passes, and
   plateau biasing to craft a natural looking base heightfield.
2. **Layer deployment** – Two terrain layers are authored automatically:
   * `AI Base Terrain` uses an `AffectorHeightFractal` and colour constant to
     block out primary relief.
   * `AI Ridge Sculpt` overlays a higher frequency fractal to carve ridgelines
     and highlight hero elevation changes.
3. **World parameters** – Seeds, tile sizes, and the global water table are set
   to sensible defaults so that flora, radial, and shader systems immediately
   benefit from the generated terrain.
4. **Guidance surfaces** – The SmartTerrainAnalyzer surfaces the generated
   blueprint inside the Guided Creation panel, including biome breakdowns and
   suggested settlement coordinates.

## Working with the auto-generated terrain

* Open the **Guided Creation** window to review the automation summary, biome
  notes, and recommended follow-up actions. The quick action list includes the
  blueprint headline alongside individual settlement suggestions.
* Toggle the guidance overlay, heatmap preview, and guideline grid to visualise
  the generated terrain metrics while sculpting.
* Use the blueprint queue to blend the AI-authored layers with manual edits or
  to iterate on different random seeds by creating a new document.
* Inspect the **Automation toolkit** entries surfaced by the AI. The river
  lattice assistant highlights freshly carved channels that can be turned into
  spline-based river affectors, while the flora enrichment pass outlines bands
  ready for immediate vegetation seeding. Additional tools now cover corridor
  planning, cinematic lighting, encounter scripting, and weather tuning so the
  editor can queue follow-up passes directly from the quick action list.
* Review the hotspot annotations to understand where ridges, shoreline bands and
  settlement-friendly valleys cluster. These insights feed directly into the new
  automation hooks listed in the Guided Creation quick action panel, including
  AI-authored travel corridors, biome rebalance notes, and logistics checklists.

## Accelerated AI tooling

* Use the new **AI** top-level menu to rerun terrain synthesis or trigger a
  comprehensive Smart Terrain Audit at any point. Selecting **Regenerate Auto
  Terrain** queues the TerrainAutoPainter with enhanced automation features,
  while **Run Smart Audit** streams the latest foresight report to the console.
* The TerrainAutoPainter now returns expanded guidance: biome rebalance
  suggestions, logistics hub zoning, recommended travel corridors, cinematic
  beats, encounter scripts, and a weather timeline. The Guided Creation panel
  surfaces these new strings automatically so designers can jump straight into
  polish tasks.

The built-in automation drastically reduces the setup time for a fresh world,
allowing designers to focus on storytelling, biome polish, and hero encounter
placement rather than repetitive boilerplate.
