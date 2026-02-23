# High-end graphics defaults

The client setup tool now aggressively enables the highest safe fidelity when
no explicit configuration is present. The updated behaviour includes:

- Automatically promoting fullscreen installs that still use the legacy
  1024×768 defaults to the highest monitor resolution that DirectX reports,
  prioritising refresh rate when multiple modes have the same pixel count.
- Unlocking advanced shader features on modern GPUs by disabling the fallback
  code paths for bump mapping, multi-pass lighting, and baked textures whenever
  hardware resources meet contemporary baselines (≥128 MB VRAM for bump
  mapping, ≥192 MB for multi-pass on Shader Model 2.0 parts, and ≥256 MB to keep
  texture baking enabled).
- Keeping all mip levels and normal maps when the system has at least 512 MB of
  system RAM, ensuring the highest texture quality possible.

These changes retain the runtime safety checks that prevent features from being
forced on unsupported hardware, so the client will still gracefully fall back
on older machines.
