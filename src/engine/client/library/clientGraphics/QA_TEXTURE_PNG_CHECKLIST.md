# clientGraphics PNG smoke checklist

## Purpose
Validate that PNG texture loading is wired correctly in build/link/runtime paths for `Texture.cpp`.

## Preconditions
- Build includes `CLIENTGRAPHICS_ENABLE_LIBPNG` for `clientGraphics`.
- Build has include paths for `libpng` and `zlib` headers.
- Link step includes `libpng` and `zlib` libraries where required.

## Smoke steps
1. Start a client that loads assets through `TextureList`.
2. Invoke `TextureList::fetch("texture/test_png_smoke.png")` using a known-good PNG in the asset tree.
3. Verify the returned texture is non-null.
4. Verify `texture->getName()` resolves to `texture/test_png_smoke.png` (not `TextureList::getDefaultTextureName()`).
5. Verify startup log includes the build banner:
   - `Texture PNG support: ENABLED (CLIENTGRAPHICS_ENABLE_LIBPNG)`.

## Expected result
- PNG texture is decoded and used directly, and default fallback texture path is not used.


## Build wiring (macro + link dependencies)
- `CLIENTGRAPHICS_ENABLE_LIBPNG` is set in `clientGraphics` Win32 project configurations:
  - `Debug|Win32`
  - `Optimized|Win32`
  - `Release|Win32`
- Win32 consumer targets that reference `clientGraphics` now include `libpng.lib` and `zlib.lib` plus corresponding `external/3rd/library/libpng/lib/win32` and `external/3rd/library/zlib/lib/win32` library paths in each configuration (`Debug`, `Optimized`, `Release`):
  - `ClientEffectEditor`
  - `Viewer`
  - `MayaExporter`
  - `ShipComponentEditor`
  - `ParticleEditor`
  - `AnimationEditor`
  - `NpcEditor`
  - `LightningEditor`
  - `TerrainEditor`
  - `SwgGodClient`
  - `SwgClient`
