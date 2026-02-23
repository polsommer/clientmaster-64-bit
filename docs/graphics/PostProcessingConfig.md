# Post-processing effect configuration

The post-processing pipeline now builds a pass graph that shares reusable HDR render targets. Each pass can be toggled and tuned via `ClientGraphics/PostProcessingEffects` keys.

## Toggle flags
- `enableBloom` (bool, default: true)
- `enableSSAO` (bool, default: true)
- `enableDepthOfField` (bool, default: true)
- `enableTAA` (bool, default: true)
- `enableVignette` (bool, default: true)
- `enableLensArtifacts` (bool, default: true)
- `enableColorGrading` (bool, default: true)
- `enableColorGradeShader` (bool, default: true) — uses the LUT-enabled shader path when available
- `enableAiColorGrading` (bool, default: true) — allow scene metadata to drive LUT selection
- `aiColorGradeFallbackToManual` (bool, default: true) — fall back to the configured presets when metadata is missing or AI is disabled

## Quality tiers
Quality values are clamped between 0 (low) and 3 (cinematic).

- `bloomQuality` (default: 2)
- `ssaoQuality` (default: 2)
- `depthOfFieldQuality` (default: 1)
- `taaQuality` (default: 1)
- `vignetteQuality` (default: 1)

The high quality preset will push bloom and SSAO to the highest tier while nudging depth-of-field, TAA, and vignette upward.

## Low-spec / 32-bit preset
Use this preset to reduce memory pressure on 32-bit clients by disabling the highest-cost effects and forcing all quality tiers to the lowest level. Quality values are clamped between 0 (low) and 3 (cinematic).

```ini
[ClientGraphics/PostProcessingEffects]
enableSSAO=false
enableDepthOfField=false
enableTAA=false
enableLensArtifacts=false
bloomQuality=0
ssaoQuality=0
depthOfFieldQuality=0
taaQuality=0
vignetteQuality=0
```

## LUT-driven color grading
- `lutDirectory` (string, default: `texture/luts`) — base folder for `.dds` or `.cube` LUT assets
- `lutPresetLow` / `lutPresetMedium` / `lutPresetHigh` / `lutPresetCinematic` — default preset names for each profile
- `lutProfile` (int) — selects which preset to load at startup (0–3)
- `lutBlendPreset` (string) — optional secondary LUT to cross-fade toward
- `lutBlendWeight` (float, default: 0.0) — blend factor between the active preset and `lutBlendPreset`

When AI-driven selection is enabled, call `PostProcessingEffectsManager::updateSceneColorGradingMetadata(tags, biome)` with the scene tags and biome names you have available. The manager will derive a best-effort preset/profile choice, optionally blending toward the configured `lutBlendPreset`. You can override or refine the inference at runtime with `PostProcessingEffectsManager::applyAiColorGradeSuggestion(primaryPreset, blendPreset, blendWeight, profileIndex)`. Manual profiles remain in control whenever AI selection is disabled or when metadata is absent and `aiColorGradeFallbackToManual` is true.

When `enableColorGradeShader` is true, the final post-process pass binds the active LUTs to the color grading shader. The shader receives:

- `TAG(MAIN)`: the resolved scene color buffer
- `TAG(LUT0)`: the active profile LUT
- `TAG(LUT1)`: the blend LUT (only when `lutBlendWeight > 0`)
- `TAG(GRD0)` factor: packed strength/contrast/saturation/blend weight
- `TAG(TINT)` factor: tint strength and flag indicating whether a LUT is present

Call `PostProcessingEffectsManager::setColorGradePreset`, `setColorGradeProfile`, and `setColorGradeBlendPreset` at runtime to swap presets per profile or animate blends without recreating materials.

## Buffer lifecycle
Render targets for the pass graph are recreated in the `deviceRestored` callback and released in `deviceLost`, keeping their sizes synchronized with the current framebuffer dimensions.
