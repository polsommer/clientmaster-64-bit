//===================================================================
//
// ConfigClientTerrain.cpp
// copyright 2000, verant interactive
//
//===================================================================

#include "clientTerrain/FirstClientTerrain.h"
#include "clientTerrain/ConfigClientTerrain.h"

#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Production.h"
#include "sharedTerrain/ConfigSharedTerrain.h"

#include <algorithm>

//===================================================================

namespace
{
	bool  ms_useOcclusion;
	bool  ms_useRealGeometryForOcclusion;
	bool  ms_useClientServerProceduralTerrainAppearanceTemplate;
	float ms_highLevelOfDetailThreshold;

	bool  ms_terrainMultiThreaded;

	bool  ms_radialFloraSortFrontToBack;

	float ms_threshold;
	bool  ms_showChunkExtents;
	bool  ms_heightBiasDisabled;
	int   ms_heightBiasMax;
	float ms_heightBiasFactor;

	bool  ms_enableFlora;
	bool  ms_preloadGroups;

	bool  ms_disableTerrainClouds;

	float ms_environmentStartTime;
	float ms_environmentNormalizedStartTime;
	bool  ms_useNormalizedTime;
	bool  ms_disableTimeOfDay;

	bool  ms_disableTerrainBlending;
	bool  ms_shaderGroupUseFirstChildOnly;

	bool  ms_disableClouds;

        bool  ms_enableLightScaling;

        bool  ms_dynamicFarFloraEnabled;
        bool  ms_dynamicNearFloraEnabled;
        bool  ms_staticNonCollidableFloraEnabled;

        float ms_interiorLightBoost;
        float ms_interiorAmbientDampen;
        float ms_interiorFogDensityScale;

	int   ms_maximumNumberOfChunksAllowed;
	bool  ms_useHighQualityTerrainProfile;

	void applyHighQualityTerrainProfile()
	{
		// Force expensive terrain rendering features that dramatically
		// improve visual fidelity on modern hardware.
		ms_useOcclusion = true;
		ms_useRealGeometryForOcclusion = true;
		ms_useClientServerProceduralTerrainAppearanceTemplate = true;
		ms_terrainMultiThreaded = true;
		ms_radialFloraSortFrontToBack = true;
		ms_enableFlora = true;
		ms_preloadGroups = true;
		ms_disableTerrainClouds = false;
		ms_disableTimeOfDay = false;
		ms_disableTerrainBlending = false;
		ms_shaderGroupUseFirstChildOnly = false;
		ms_disableClouds = false;
		ms_enableLightScaling = true;
		ms_dynamicFarFloraEnabled = true;
		ms_dynamicNearFloraEnabled = true;
		ms_staticNonCollidableFloraEnabled = true;

		// Tune scalar values towards a higher quality presentation while
		// respecting any explicit overrides from a configuration file.
		ms_highLevelOfDetailThreshold = std::min(ms_highLevelOfDetailThreshold, 4.0f);
		ms_threshold = std::min(ms_threshold, 2.0f);
		ms_heightBiasDisabled = true;
		ms_heightBiasMax = std::max(ms_heightBiasMax, 64);
		ms_heightBiasFactor = std::max(ms_heightBiasFactor, 12.0f);
		ms_maximumNumberOfChunksAllowed = std::max(ms_maximumNumberOfChunksAllowed, 24 * 1024);
}
}

//===================================================================

bool ConfigClientTerrain::getUseOcclusion ()
{
	return ms_useOcclusion;
}

//-----------------------------------------------------------------

bool ConfigClientTerrain::getUseClientServerProceduralTerrainAppearanceTemplate ()
{
	return ms_useClientServerProceduralTerrainAppearanceTemplate;
}

//-----------------------------------------------------------------

float ConfigClientTerrain::getHighLevelOfDetailThreshold ()
{
	return ms_highLevelOfDetailThreshold;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getTerrainMultiThreaded ()
{
	return ms_terrainMultiThreaded;
}

// ----------------------------------------------------------------------

bool ConfigClientTerrain::getRadialFloraSortFrontToBack ()
{
	return ms_radialFloraSortFrontToBack;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getThreshold ()
{
	return ms_threshold;
}

//-----------------------------------------------------------------

bool ConfigClientTerrain::getShowChunkExtents ()
{
	return ms_showChunkExtents;
}

//-----------------------------------------------------------------

bool ConfigClientTerrain::getHeightBiasDisabled ()
{
	return ms_heightBiasDisabled;
}

//-----------------------------------------------------------------

int ConfigClientTerrain::getHeightBiasMax ()
{
	return ms_heightBiasMax;
}

//-----------------------------------------------------------------

float ConfigClientTerrain::getHeightBiasFactor ()
{
	return ms_heightBiasFactor;
}

//-----------------------------------------------------------------

bool ConfigClientTerrain::getEnableFlora ()
{
	return ms_enableFlora;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getPreloadGroups ()
{
	return ms_preloadGroups;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getUseRealGeometryForOcclusion ()
{
	return ms_useRealGeometryForOcclusion;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getDisableTerrainClouds ()
{
	return ms_disableTerrainClouds;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getEnvironmentStartTime ()
{
	return ms_environmentStartTime;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getEnvironmentNormalizedStartTime ()
{
	return ms_environmentNormalizedStartTime;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getUseNormalizedTime ()
{
	return ms_useNormalizedTime;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getDisableTimeOfDay ()
{
	return ms_disableTimeOfDay;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getDisableTerrainBlending ()
{
	return ms_disableTerrainBlending;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getShaderGroupUseFirstChildOnly ()
{
	return ms_shaderGroupUseFirstChildOnly;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getDisableClouds ()
{
	return ms_disableClouds;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getEnableLightScaling ()
{
	return ms_enableLightScaling;
}

//----------------------------------------------------------------------

bool  ConfigClientTerrain::getDynamicFarFloraEnabled     ()
{
	return ms_dynamicFarFloraEnabled;
}

//----------------------------------------------------------------------

bool  ConfigClientTerrain::getDynamicNearFloraEnabled    ()
{
	return ms_dynamicNearFloraEnabled;
}

//----------------------------------------------------------------------

bool  ConfigClientTerrain::getStaticNonCollidableFloraEnabled ()
{
	return ms_staticNonCollidableFloraEnabled;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getDynamicNearFloraDistanceDefault ()
{
	return 32.f;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getStaticNonCollidableFloraDistanceDefault ()
{
	return 64.f;
}

//-------------------------------------------------------------------

int ConfigClientTerrain::getMaximumNumberOfChunksAllowed ()
{
	return ms_maximumNumberOfChunksAllowed;
}

//-------------------------------------------------------------------

bool ConfigClientTerrain::getUseHighQualityTerrainProfile ()
{
        return ms_useHighQualityTerrainProfile;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getInteriorLightBoost ()
{
        return ms_interiorLightBoost;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getInteriorAmbientDampen ()
{
        return ms_interiorAmbientDampen;
}

//-------------------------------------------------------------------

float ConfigClientTerrain::getInteriorFogDensityScale ()
{
        return ms_interiorFogDensityScale;
}

//===================================================================

#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("ClientTerrain", #a, b))
#define KEY_FLOAT(a,b)   (ms_ ## a = ConfigFile::getKeyFloat("ClientTerrain", #a, b))
#define KEY_INT(a,b)     (ms_ ## a = ConfigFile::getKeyInt("ClientTerrain", #a, b))
//#define KEY_STRING(a,b)  (ms_ ## a = ConfigFile::getKeyString("ClientTerrain", #a, b))

//===================================================================

void ConfigClientTerrain::install()
{
	DEBUG_REPORT_LOG_PRINT(ConfigSharedTerrain::getDebugReportInstall(), ("ConfigClientTerrain::install\n"));

	// Occlusion and rendering optimization
	KEY_BOOL(useOcclusion, true);
	KEY_BOOL(useClientServerProceduralTerrainAppearanceTemplate, true);
	KEY_BOOL(useRealGeometryForOcclusion, true);
	KEY_FLOAT(highLevelOfDetailThreshold, 6.0f);

	// Multithreading for terrain
	KEY_BOOL(terrainMultiThreaded, true);

	// Flora rendering improvements
	KEY_BOOL(radialFloraSortFrontToBack, true);

	// Level of detail threshold (LOD) and terrain bias settings
	KEY_FLOAT(threshold, 4.f);      // LOD transition threshold
	KEY_BOOL(showChunkExtents, false);    // For debugging chunk loading
	KEY_BOOL(heightBiasDisabled, true);
	KEY_INT(heightBiasMax, 50);
	KEY_FLOAT(heightBiasFactor, 8);

	// Flora enablement
	KEY_BOOL(enableFlora, true);
	KEY_BOOL(preloadGroups, true);     // Always preload if not testing

	// Visuals and environmental effects
	KEY_BOOL(disableTerrainClouds, false);    // Enable clouds
	KEY_BOOL(disableTimeOfDay, false);    // Keep dynamic lighting
	KEY_FLOAT(environmentStartTime, 300.f);    // Default: dawn
	KEY_BOOL(useNormalizedTime, false);
	KEY_FLOAT(environmentNormalizedStartTime, 0.525f);

	// Terrain and shader blending
	KEY_BOOL(disableTerrainBlending, false);
	KEY_BOOL(shaderGroupUseFirstChildOnly, false);    // Use full shader trees

        // Cloud and lighting rendering
        KEY_BOOL(disableClouds, false);
        KEY_BOOL(enableLightScaling, true);     // Better lighting range control

        // Interior presentation tuning
        KEY_FLOAT(interiorLightBoost, 1.25f);           // Lift diffuse lighting indoors
        KEY_FLOAT(interiorAmbientDampen, 0.85f);        // Keep contrast indoors
        KEY_FLOAT(interiorFogDensityScale, 1.15f);      // Slightly denser interior fog

	// Flora types
	KEY_BOOL(dynamicFarFloraEnabled, true);
	KEY_BOOL(dynamicNearFloraEnabled, true);
	KEY_BOOL(staticNonCollidableFloraEnabled, true);

	// Chunk streaming and memory allocation
	KEY_INT(maximumNumberOfChunksAllowed, 20 * 1024); // Increased for modern RAM
	KEY_BOOL(useHighQualityTerrainProfile, true);

	if (ms_useHighQualityTerrainProfile)
	{
		applyHighQualityTerrainProfile();
	}
}
//===================================================================
