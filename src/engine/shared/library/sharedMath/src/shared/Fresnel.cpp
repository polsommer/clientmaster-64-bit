//===================================================================
//
// Fresnel.cpp
// Provides helper functions for evaluating Fresnel reflectance curves
// with Schlick's approximation.
//
//===================================================================

#include "sharedMath/FirstSharedMath.h"
#include "sharedMath/Fresnel.h"

#include "sharedMath/PackedRgb.h"
#include "sharedMath/VectorArgb.h"

namespace
{
        inline float clampUnit(float value)
        {
                if (value < 0.0f)
                        return 0.0f;
                if (value > 1.0f)
                        return 1.0f;
                return value;
        }
}

float const Fresnel::DefaultWaterReflectivity = 0.02f;

float Fresnel::computeReflectance(float cosTheta, float baseReflectivity)
{
        cosTheta = clampUnit(cosTheta);
        baseReflectivity = clampUnit(baseReflectivity);

        const float oneMinusCosine = 1.0f - cosTheta;
        const float oneMinusCosine2 = oneMinusCosine * oneMinusCosine;
        const float oneMinusCosine4 = oneMinusCosine2 * oneMinusCosine2;
        const float oneMinusCosine5 = oneMinusCosine4 * oneMinusCosine;

        return baseReflectivity + (1.0f - baseReflectivity) * oneMinusCosine5;
}

PackedRgb Fresnel::applyToColor(PackedRgb const &color, float cosTheta, float baseReflectivity)
{
        const float fresnel = computeReflectance(cosTheta, baseReflectivity);

        VectorArgb linearColor = color.convert(1.0f);
        linearColor.r *= fresnel;
        linearColor.g *= fresnel;
        linearColor.b *= fresnel;

        PackedRgb result;
        result.convert(linearColor);
        return result;
}

