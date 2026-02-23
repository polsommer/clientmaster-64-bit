// ======================================================================
//
// PostProcessingEffectsManager.h
// Copyright 2004, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_PostProcessingEffectsManager_H
#define INCLUDED_PostProcessingEffectsManager_H

// ======================================================================

#include <string>
#include <vector>

class Texture;
class StaticShader;

//----------------------------------------------------------------------

class PostProcessingEffectsManager
{
public:

	static void install();

	static bool isSupported();
	static bool isEnabled();
	static void setEnabled(bool enabled);
		
	static void preSceneRender();
	static void postSceneRender();

	static Texture * getPrimaryBuffer();
	static Texture * getSecondaryBuffer();
        static Texture * getTertiaryBuffer();

        static StaticShader * getHeatCompositingShader();

static void swapBuffers();

static void setAntialiasEnabled(bool enabled);
static bool getAntialiasEnabled();

static void applyHighQualityPreset(float cinematicStrength = 1.0f);

        static void setColorGradePreset(int profileIndex, std::string const &presetName);
        static void setColorGradeProfile(int profileIndex);
        static void setColorGradeBlendPreset(std::string const &presetName, float blendWeight);

        static void updateSceneColorGradingMetadata(std::vector<std::string> const &sceneTags, std::string const &biomeName);
        static void applyAiColorGradeSuggestion(std::string const &primaryPreset, std::string const &blendPreset, float blendWeight, int profileIndex = -1);

        static std::string const & getActiveColorGradePreset();
        static std::string const & getActiveColorGradeBlendPreset();
        static float getActiveColorGradeBlendWeight();

private:

	static void remove();

	static void enable();
	static void disable();

	PostProcessingEffectsManager();
	PostProcessingEffectsManager(PostProcessingEffectsManager const &);
	~PostProcessingEffectsManager();
};

// ======================================================================

#endif
