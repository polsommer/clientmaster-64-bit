# 32-bit graphics tuning quick reference

This guide consolidates the recommended knobs for keeping the 32-bit client stable on modern Windows while still allowing targeted performance reductions on low-spec machines.

## Direct3D 9Ex runtime recommendations

Use the `Direct3d9` configuration section (for example `clientdata/Direct3d9.cfg`) and prefer the 9Ex runtime for smoother windowed-mode presentation on modern Windows builds:

```ini
Direct3d9.preferDirect3d9Ex=true
Direct3d9.maximumFrameLatency=1
Direct3d9.gpuThreadPriority=0
Direct3d9.waitForVBlankAfterPresent=false
```

Notes:

- `Direct3d9.maximumFrameLatency` is clamped to `1-16` to avoid unstable values on lower-end 32-bit hardware.
- `Direct3d9.gpuThreadPriority` is clamped to `-7..7`, matching the documented Direct3D 9Ex range.
- `Direct3d9.waitForVBlankAfterPresent` is optional. Enable it only when you need extra tear reduction in windowed mode.

## Why you should avoid overriding high-end defaults

The setup tool already pushes safe, high-end defaults when no explicit configuration exists (resolution promotion, shader feature unlocks, and full texture fidelity). Those defaults are guarded by runtime capability checks, so overriding them can:

- Force lower-quality visuals on hardware that can handle the higher tiers.
- Disable fallback logic that keeps older systems stable.
- Create divergent baselines for QA and support.

Unless you are targeting a known low-spec machine or testing a specific regression, leave the high-end defaults intact and focus on the low-spec overrides below.

## Low-spec post-processing recommendation

For 32-bit or low-end GPUs, reduce or disable the most expensive post-processing passes in `ClientGraphics/PostProcessingEffects`. A safe starting point is:

```ini
enableSSAO=false
enableDepthOfField=false
enableTAA=false
ssaoQuality=0
depthOfFieldQuality=0
taaQuality=0
```

You can additionally lower `bloomQuality` or `vignetteQuality` to `0` if you need further savings. These values are clamped between `0` (low) and `3` (cinematic).


## DXVK and runtime precedence

You can opt into DXVK from the same `[Direct3d9]` section used by the native renderer knobs:

```ini
[Direct3d9]
enableDxvk=1
preferDxvkForAmd=1
```

Precedence is:

1. `enableDxvk=1`: force DXVK on supported systems.
2. `enableDxvk=0` and `preferDxvkForAmd=1`: allow policy-based auto-enable on AMD GPUs only.
3. Otherwise use native Direct3D 9, and when available continue preferring Direct3D 9Ex (`preferDirect3d9Ex=true`).

This preserves legacy behavior by default (`enableDxvk=0`, `preferDxvkForAmd=0`).
