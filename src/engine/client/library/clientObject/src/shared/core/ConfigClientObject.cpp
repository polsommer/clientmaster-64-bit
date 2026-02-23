//===================================================================
//
// ConfigClientObject.cpp
// copyright 2000, verant interactive
//
//===================================================================

#include "clientObject/FirstClientObject.h"
#include "clientObject/ConfigClientObject.h"

#include "sharedFoundation/ConfigFile.h"

//===================================================================

namespace ConfigClientObjectNamespace
{
        bool         ms_forceLowDetailLevels            = false;
        bool         ms_forceHighDetailLevels           = false;
        float        ms_detailLevelBias                 = 0.0f;
        float        ms_detailLevelStretch              = 0.0f;
        float        ms_detailOverlapFraction           = 0.0f;
        float        ms_detailOverlapCap                = 0.0f;
        bool         ms_preloadDetailLevels             = false;
        bool         ms_detailAppearancesWithoutSprites = false;
        float        ms_interiorShadowAlpha             = 0.0f;
        const char * ms_screenShader                   = 0;
        bool         ms_disableMeshTestShapes           = false;
}
using namespace ConfigClientObjectNamespace;

//===================================================================

float ConfigClientObject::getDetailLevelStretch()
{
	return ms_detailLevelStretch;
}

// ------------------------------------------------------------------

float ConfigClientObject::getDetailOverlapFraction()
{
	return ms_detailOverlapFraction;
}

// ------------------------------------------------------------------

float ConfigClientObject::getDetailOverlapCap()
{
	return ms_detailOverlapCap;
}

//-------------------------------------------------------------------

bool ConfigClientObject::getPreloadDetailLevels()
{
	return ms_preloadDetailLevels;
}

//-------------------------------------------------------------------

bool ConfigClientObject::getDetailAppearancesWithoutSprites()
{
	return ms_detailAppearancesWithoutSprites;
}

//-------------------------------------------------------------------

float ConfigClientObject::getInteriorShadowAlpha()
{
	return ms_interiorShadowAlpha;
}

//-------------------------------------------------------------------

const char * ConfigClientObject::getScreenShader()
{
	return ms_screenShader;
}

//----------------------------------------------------------------------

float ConfigClientObject::getDetailLevelBias()
{
	return ms_detailLevelBias;
}

//----------------------------------------------------------------------

bool ConfigClientObject::getDisableMeshTestShapes()
{
	return ms_disableMeshTestShapes;
}

//===================================================================

namespace
{
        bool getKeyBool(const char * name, bool defaultValue)
        {
                return ConfigFile::getKeyBool("ClientObject", name, defaultValue);
        }

        float getKeyFloat(const char * name, float defaultValue)
        {
                return ConfigFile::getKeyFloat("ClientObject", name, defaultValue);
        }

        const char * getKeyString(const char * name, const char * defaultValue)
        {
                return ConfigFile::getKeyString("ClientObject", name, defaultValue);
        }
}

//===================================================================

void ConfigClientObject::install()
{
        ms_forceLowDetailLevels            = getKeyBool("forceLowDetailLevels", false);
        ms_forceHighDetailLevels           = getKeyBool("forceHighDetailLevels", false);
        ms_detailLevelBias                 = getKeyFloat("detailLevelBias", 1.0f);
        ms_detailLevelStretch              = getKeyFloat("detailLevelStretch", 10.0f);
        ms_detailOverlapFraction           = getKeyFloat("detailOverlapFraction", 0.10f);
        ms_detailOverlapCap                = getKeyFloat("detailOverlapCap", 25.0f);
        ms_preloadDetailLevels             = getKeyBool("preloadDetailLevels", false);
        ms_detailAppearancesWithoutSprites = getKeyBool("detailAppearancesWithoutSprites", false);
        ms_interiorShadowAlpha             = getKeyFloat("interiorShadowAlpha", 0.1f);
        ms_screenShader                    = getKeyString("screenShader", 0);
        ms_disableMeshTestShapes           = getKeyBool("disableMeshTestShapes", false);
}

//===================================================================
