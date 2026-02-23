#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/LensPostProcessing.h"

#include "clientGraphics/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
        template <typename T>
        inline T clampValue(T value, T minValue, T maxValue)
        {
                if (value < minValue)
                        return minValue;
                if (value > maxValue)
                        return maxValue;
                return value;
        }

        inline float lerp(float a, float b, float t)
        {
                return a + (b - a) * t;
        }

        struct Color4f
        {
                float r;
                float g;
                float b;
                float a;
        };

        inline float clamp01(float value)
        {
                return clampValue(value, 0.0f, 1.0f);
        }

        inline Color4f readColor(const uint8 *pixel)
        {
                Color4f color;
                color.b = static_cast<float>(pixel[0]) * (1.0f / 255.0f);
                color.g = static_cast<float>(pixel[1]) * (1.0f / 255.0f);
                color.r = static_cast<float>(pixel[2]) * (1.0f / 255.0f);
                color.a = static_cast<float>(pixel[3]) * (1.0f / 255.0f);
                return color;
        }

        inline uint32 writeColor(const Color4f &color)
        {
                const uint8 r = static_cast<uint8>(clampValue(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
                const uint8 g = static_cast<uint8>(clampValue(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
                const uint8 b = static_cast<uint8>(clampValue(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
                const uint8 a = static_cast<uint8>(clampValue(color.a, 0.0f, 1.0f) * 255.0f + 0.5f);

                return (static_cast<uint32>(a) << 24) |
                        (static_cast<uint32>(r) << 16) |
                        (static_cast<uint32>(g) << 8) |
                        (static_cast<uint32>(b) << 0);
        }

        inline float filmicCurve(float x)
        {
                // John Hable's Uncharted 2 filmic curve constants
                const float A = 0.15f;
                const float B = 0.50f;
                const float C = 0.10f;
                const float D = 0.20f;
                const float E = 0.02f;
                const float F = 0.30f;
                return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
        }

        Color4f sampleBilinear(const uint8 *pixels, int pitch, int width, int height, float u, float v)
        {
                u = clamp01(u);
                v = clamp01(v);

                const float x = u * static_cast<float>(width - 1);
                const float y = v * static_cast<float>(height - 1);

                const int x0 = clampValue(static_cast<int>(std::floor(x)), 0, width - 1);
                const int y0 = clampValue(static_cast<int>(std::floor(y)), 0, height - 1);
                const int x1 = clampValue(x0 + 1, 0, width - 1);
                const int y1 = clampValue(y0 + 1, 0, height - 1);

                const float tx = x - static_cast<float>(x0);
                const float ty = y - static_cast<float>(y0);

                const uint8 *row0 = pixels + y0 * pitch;
                const uint8 *row1 = pixels + y1 * pitch;

                const Color4f c00 = readColor(row0 + x0 * 4);
                const Color4f c10 = readColor(row0 + x1 * 4);
                const Color4f c01 = readColor(row1 + x0 * 4);
                const Color4f c11 = readColor(row1 + x1 * 4);

                Color4f c0;
                c0.r = c00.r + (c10.r - c00.r) * tx;
                c0.g = c00.g + (c10.g - c00.g) * tx;
                c0.b = c00.b + (c10.b - c00.b) * tx;
                c0.a = c00.a + (c10.a - c00.a) * tx;

                Color4f c1;
                c1.r = c01.r + (c11.r - c01.r) * tx;
                c1.g = c01.g + (c11.g - c01.g) * tx;
                c1.b = c01.b + (c11.b - c01.b) * tx;
                c1.a = c01.a + (c11.a - c01.a) * tx;

                Color4f c;
                c.r = c0.r + (c1.r - c0.r) * ty;
                c.g = c0.g + (c1.g - c0.g) * ty;
                c.b = c0.b + (c1.b - c0.b) * ty;
                c.a = c0.a + (c1.a - c0.a) * ty;

                return c;
        }
}

void LensPostProcessing::apply(Texture &destination, Texture const &source, float chromaticAberrationStrength, float lensFlareStrength, float lensStreakStrength, float vignetteStrength, bool enableColorGrading, float colorGradeStrength, float colorGradeContrast, float colorGradeSaturation, float colorGradeTintStrength)
{
        const int width = source.getWidth();
        const int height = source.getHeight();

        if (width <= 0 || height <= 0)
                return;

        if (destination.getWidth() != width || destination.getHeight() != height)
                return;

        const float epsilon = 1.0e-5f;
        const bool hasChromaticAberration = std::fabs(chromaticAberrationStrength) > epsilon;
        const bool hasLensFlare = lensFlareStrength > epsilon;
        const bool hasLensStreak = lensStreakStrength > epsilon;
        const bool hasVignette = vignetteStrength > epsilon;
        const bool hasColorGrade = enableColorGrading &&
                (colorGradeStrength > epsilon || std::fabs(colorGradeContrast - 1.0f) > epsilon || std::fabs(colorGradeSaturation - 1.0f) > epsilon || colorGradeTintStrength > epsilon);

        if (!hasChromaticAberration && !hasLensFlare && !hasLensStreak && !hasVignette && !hasColorGrade)
        {
                if (&destination != &source)
                {
                        Texture::LockData readLock(TF_ARGB_8888, 0, 0, 0, width, height, false);
                        Texture::LockData writeLock(TF_ARGB_8888, 0, 0, 0, width, height, true);

                        source.lockReadOnly(readLock);
                        destination.lock(writeLock);

                        const uint8 *sourcePixels = static_cast<const uint8 *>(readLock.getPixelData());
                        uint8 *destinationPixels = static_cast<uint8 *>(writeLock.getPixelData());

                        if (sourcePixels && destinationPixels)
                        {
                                const int sourcePitch = readLock.getPitch();
                                const int destinationPitch = writeLock.getPitch();

                                for (int y = 0; y < height; ++y)
                                {
                                        std::memcpy(destinationPixels + y * destinationPitch, sourcePixels + y * sourcePitch, static_cast<size_t>(width) * 4);
                                }
                        }

                        destination.unlock(writeLock);
                        source.unlock(readLock);
                }

                return;
        }

        Texture::LockData sourceLock(TF_ARGB_8888, 0, 0, 0, width, height, false);
        Texture::LockData destinationLock(TF_ARGB_8888, 0, 0, 0, width, height, true);

        source.lockReadOnly(sourceLock);

        bool sourceLocked = true;
        std::vector<uint8> sourcePixelCopy;

        const uint8 *sourcePixels = static_cast<const uint8 *>(sourceLock.getPixelData());
        if (!sourcePixels)
        {
                source.unlock(sourceLock);
                return;
        }

        int sourcePitch = sourceLock.getPitch();
        if (sourcePitch <= 0)
        {
                source.unlock(sourceLock);
                return;
        }

        if (&destination == &source)
        {
                const int tightPitch = width * 4;

                sourcePixelCopy.resize(static_cast<size_t>(height) * static_cast<size_t>(tightPitch));
                if (!sourcePixelCopy.empty())
                {
                        uint8 *copyBase = &sourcePixelCopy[0];
                        for (int y = 0; y < height; ++y)
                        {
                                std::memcpy(copyBase + static_cast<size_t>(y) * static_cast<size_t>(tightPitch), sourcePixels + y * sourcePitch, static_cast<size_t>(tightPitch));
                        }

                        sourcePixels = copyBase;
                        sourcePitch = tightPitch;
                        source.unlock(sourceLock);
                        sourceLocked = false;
                }
                else
                {
                        source.unlock(sourceLock);
                        return;
                }
        }

        destination.lock(destinationLock);

        uint8 *destinationPixels = static_cast<uint8 *>(destinationLock.getPixelData());

        if (!destinationPixels)
        {
                destination.unlock(destinationLock);
                if (sourceLocked)
                        source.unlock(sourceLock);
                return;
        }

        const int destinationPitch = destinationLock.getPitch();

        const float invWidth = width > 0 ? 1.0f / static_cast<float>(width) : 1.0f;
        const float invHeight = height > 0 ? 1.0f / static_cast<float>(height) : 1.0f;
        const float clampedGradeStrength = clamp01(colorGradeStrength);
        const float clampedTintStrength = clamp01(colorGradeTintStrength);
        const float sanitizedContrast = std::max(colorGradeContrast, 0.0f);
        const float sanitizedSaturation = std::max(colorGradeSaturation, 0.0f);
        const float filmicWhiteValue = hasColorGrade ? std::max(filmicCurve(1.0f), epsilon) : 1.0f;
        const float filmicWhiteScale = hasColorGrade ? (1.0f / filmicWhiteValue) : 1.0f;

        for (int y = 0; y < height; ++y)
        {
                uint8 *destinationRow = destinationPixels + y * destinationPitch;

                const float v = (static_cast<float>(y) + 0.5f) * invHeight;
                const float vOffset = v - 0.5f;
                const float vOffsetSq = vOffset * vOffset;

                for (int x = 0; x < width; ++x)
                {
                        uint8 *destinationPixel = destinationRow + x * 4;

                        const float u = (static_cast<float>(x) + 0.5f) * invWidth;
                        const float uOffset = u - 0.5f;
                        const float uOffsetAbs = std::fabs(uOffset);

                        float radialDistance = 0.0f;
                        const float radialDistanceSq = uOffset * uOffset + vOffsetSq;
                        const bool requiresRadialDistance = hasLensFlare || hasVignette || hasChromaticAberration;
                        if (requiresRadialDistance)
                                radialDistance = std::sqrt(radialDistanceSq);

                        float aberrationOffsetU = 0.0f;
                        float aberrationOffsetV = 0.0f;
                        if (hasChromaticAberration)
                        {
                                aberrationOffsetU = uOffset * chromaticAberrationStrength;
                                aberrationOffsetV = vOffset * chromaticAberrationStrength;
                        }

                        const Color4f sampleBase = sampleBilinear(sourcePixels, sourcePitch, width, height, u, v);
                        Color4f sampleR = sampleBase;
                        Color4f sampleG = sampleBase;
                        Color4f sampleB = sampleBase;

                        if (hasChromaticAberration)
                        {
                                sampleR = sampleBilinear(sourcePixels, sourcePitch, width, height, u + aberrationOffsetU, v + aberrationOffsetV);
                                sampleB = sampleBilinear(sourcePixels, sourcePitch, width, height, u - aberrationOffsetU, v - aberrationOffsetV);
                        }

                        Color4f color;
                        color.r = sampleR.r;
                        color.g = sampleG.g;
                        color.b = sampleB.b;
                        color.a = sampleBase.a;

                        const float brightness = (sampleBase.r + sampleBase.g + sampleBase.b) / 3.0f;

                        float halo = 0.0f;
                        if (hasLensFlare)
                                halo = clamp01(1.0f - radialDistance * 1.25f) * lensFlareStrength * brightness;

                        float streak = 0.0f;
                        if (hasLensStreak)
                                streak = clamp01(1.0f - uOffsetAbs * 6.0f) * lensStreakStrength * brightness;

                        float chromaFringe = 0.0f;
                        if (hasChromaticAberration)
                                chromaFringe = clamp01(radialDistance * 1.5f) * chromaticAberrationStrength * 40.0f;

                        color.r += halo * 0.9f + streak * 0.6f + sampleBase.r * chromaFringe * 0.2f;
                        color.g += halo * 0.7f + streak * 0.4f;
                        color.b += halo * 0.5f + streak * 0.2f + sampleBase.b * chromaFringe * 0.25f;

                        if (hasVignette)
                        {
                                const float vignette = clamp01(1.0f - radialDistanceSq * vignetteStrength);
                                color.r *= vignette;
                                color.g *= vignette;
                                color.b *= vignette;
                        }

                        if (hasColorGrade)
                        {
                                Color4f graded = color;
                                graded.r = clamp01(filmicCurve(graded.r) * filmicWhiteScale);
                                graded.g = clamp01(filmicCurve(graded.g) * filmicWhiteScale);
                                graded.b = clamp01(filmicCurve(graded.b) * filmicWhiteScale);

                                const float luminance = clamp01(graded.r * 0.2126f + graded.g * 0.7152f + graded.b * 0.0722f);
                                Color4f tinted = graded;
                                if (clampedTintStrength > epsilon)
                                {
                                        const float shadowFactor = clamp01(1.0f - luminance);
                                        const float highlightFactor = luminance;
                                        tinted.r = clamp01(tinted.r + (highlightFactor * 0.08f + shadowFactor * 0.02f) * clampedTintStrength);
                                        tinted.g = clamp01(tinted.g + (highlightFactor * 0.02f + shadowFactor * 0.01f) * clampedTintStrength);
                                        tinted.b = clamp01(tinted.b + (shadowFactor * 0.10f - highlightFactor * 0.03f) * clampedTintStrength);
                                }

                                const float tintedLuminance = tinted.r * 0.2126f + tinted.g * 0.7152f + tinted.b * 0.0722f;
                                Color4f saturated;
                                saturated.r = clamp01(tintedLuminance + (tinted.r - tintedLuminance) * sanitizedSaturation);
                                saturated.g = clamp01(tintedLuminance + (tinted.g - tintedLuminance) * sanitizedSaturation);
                                saturated.b = clamp01(tintedLuminance + (tinted.b - tintedLuminance) * sanitizedSaturation);
                                saturated.a = color.a;

                                const float pivot = 0.5f;
                                Color4f contrasted;
                                contrasted.r = clamp01((saturated.r - pivot) * sanitizedContrast + pivot);
                                contrasted.g = clamp01((saturated.g - pivot) * sanitizedContrast + pivot);
                                contrasted.b = clamp01((saturated.b - pivot) * sanitizedContrast + pivot);
                                contrasted.a = color.a;

                                color.r = clamp01(lerp(color.r, contrasted.r, clampedGradeStrength));
                                color.g = clamp01(lerp(color.g, contrasted.g, clampedGradeStrength));
                                color.b = clamp01(lerp(color.b, contrasted.b, clampedGradeStrength));
                        }

                        const uint32 packedColor = writeColor(color);
                        destinationPixel[0] = static_cast<uint8>(packedColor & 0xff);
                        destinationPixel[1] = static_cast<uint8>((packedColor >> 8) & 0xff);
                        destinationPixel[2] = static_cast<uint8>((packedColor >> 16) & 0xff);
                        destinationPixel[3] = static_cast<uint8>((packedColor >> 24) & 0xff);
                }
        }

        destination.unlock(destinationLock);

        if (sourceLocked)
                source.unlock(sourceLock);
}
