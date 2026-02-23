//===================================================================
//
// Fresnel.h
// Maintains helper routines for computing Fresnel reflectance terms
// used by water and other reflective surfaces.
//
//===================================================================

#ifndef INCLUDED_Fresnel_H
#define INCLUDED_Fresnel_H

#include "sharedMath/PackedRgb.h"

class Fresnel
{
public:
        static float const DefaultWaterReflectivity;

public:
        static float computeReflectance(float cosTheta, float baseReflectivity);
        static PackedRgb applyToColor(PackedRgb const &color, float cosTheta, float baseReflectivity);
};

#endif // INCLUDED_Fresnel_H

