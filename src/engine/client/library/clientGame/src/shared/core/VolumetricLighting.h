// ======================================================================
//
// VolumetricLighting.h
// Copyright 2024
//
// ======================================================================

#ifndef INCLUDED_VolumetricLighting_H
#define INCLUDED_VolumetricLighting_H

// ======================================================================

class VolumetricLighting
{
public:
	static void install();
	static void remove();

	static bool isSupported();
	static bool isEnabled();
	static void setEnabled(bool enable);
	static void enable();
	static void disable();

	static void preSceneRender();
	static void postSceneRender();

        static void setFogDensityScale(float scale);
        static float getFogDensityScale();

        static void setLightShaftIntensity(float intensity);
        static float getLightShaftIntensity();
};

// ======================================================================

#endif // INCLUDED_VolumetricLighting_H

