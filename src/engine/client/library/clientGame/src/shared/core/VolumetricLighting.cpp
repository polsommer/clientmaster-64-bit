// ======================================================================
//
// VolumetricLighting.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/VolumetricLighting.h"

#include "clientGame/Game.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/PostProcessingEffectsManager.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/Texture.h"
#include "clientTerrain/GroundEnvironment.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedMath/Vector.h"
#include "sharedMath/VectorArgb.h"
#include "sharedFoundation/FloatMath.h"
#include "sharedUtility/LocalMachineOptionManager.h"

#include <algorithm>
#include <cmath>

// ======================================================================

namespace VolumetricLightingNamespace
{
        namespace
        {
                inline float clampInclusive(float value, float minValue, float maxValue)
                {
                        return value < minValue ? minValue : (value > maxValue ? maxValue : value);
                }

                inline float saturate(float value)
                {
                        return clampInclusive(value, 0.0f, 1.0f);
                }
        }

        void remove();
        void deviceLost();
        void deviceRestored();

        bool  ms_enable = true;
        bool  ms_enabled = false;
        bool  ms_callbacksRegistered = false;

	float ms_fogDensityScale = 1.35f;
	float ms_heightFogFalloff = 0.0015f;
	float ms_maxFogAlpha = 0.65f;
	float ms_temporalSmoothing = 0.15f;

	float ms_lightShaftIntensity = 0.9f;
	float ms_lightShaftLength = 0.85f;
	float ms_lightShaftBeamFrequency = 6.0f;
	float ms_lightShaftBeamSharpness = 2.0f;
	float ms_lightShaftEdgeAttenuation = 0.12f;
	float ms_minFogDensityForLighting = 0.0004f;
	float ms_sunDistance = 8192.0f;
	float ms_minSunElevation = -0.25f;

	float ms_smoothedFogAlpha = 0.0f;
	float ms_smoothedLightAlpha = 0.0f;
}

using namespace VolumetricLightingNamespace;

// ======================================================================

namespace
{
	inline float lerp(float a, float b, float t)
	{
	        return a + (b - a) * t;
	}
}

// ======================================================================

void VolumetricLighting::install()
{
	InstallTimer const installTimer("VolumetricLighting::install");

	ExitChain::add(VolumetricLightingNamespace::remove, "VolumetricLighting::remove");

	LocalMachineOptionManager::registerOption(ms_enable, "ClientGame/VolumetricLighting", "enable");
	LocalMachineOptionManager::registerOption(ms_fogDensityScale, "ClientGame/VolumetricLighting", "fogDensityScale");
	LocalMachineOptionManager::registerOption(ms_heightFogFalloff, "ClientGame/VolumetricLighting", "heightFogFalloff");
	LocalMachineOptionManager::registerOption(ms_maxFogAlpha, "ClientGame/VolumetricLighting", "maxFogAlpha");
	LocalMachineOptionManager::registerOption(ms_temporalSmoothing, "ClientGame/VolumetricLighting", "temporalSmoothing");
	LocalMachineOptionManager::registerOption(ms_lightShaftIntensity, "ClientGame/VolumetricLighting", "lightShaftIntensity");
	LocalMachineOptionManager::registerOption(ms_lightShaftLength, "ClientGame/VolumetricLighting", "lightShaftLength");
	LocalMachineOptionManager::registerOption(ms_lightShaftBeamFrequency, "ClientGame/VolumetricLighting", "lightShaftBeamFrequency");
	LocalMachineOptionManager::registerOption(ms_lightShaftBeamSharpness, "ClientGame/VolumetricLighting", "lightShaftBeamSharpness");
	LocalMachineOptionManager::registerOption(ms_lightShaftEdgeAttenuation, "ClientGame/VolumetricLighting", "lightShaftEdgeAttenuation");
	LocalMachineOptionManager::registerOption(ms_minFogDensityForLighting, "ClientGame/VolumetricLighting", "minFogDensityForLighting");
	LocalMachineOptionManager::registerOption(ms_sunDistance, "ClientGame/VolumetricLighting", "sunDistance");
	LocalMachineOptionManager::registerOption(ms_minSunElevation, "ClientGame/VolumetricLighting", "minSunElevation");

	ms_enable = ConfigFile::getKeyBool("ClientGame/VolumetricLighting", "enable", ms_enable);
	ms_fogDensityScale = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "fogDensityScale", ms_fogDensityScale);
	ms_heightFogFalloff = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "heightFogFalloff", ms_heightFogFalloff);
	ms_maxFogAlpha = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "maxFogAlpha", ms_maxFogAlpha);
	ms_temporalSmoothing = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "temporalSmoothing", ms_temporalSmoothing);
	ms_lightShaftIntensity = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "lightShaftIntensity", ms_lightShaftIntensity);
	ms_lightShaftLength = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "lightShaftLength", ms_lightShaftLength);
	ms_lightShaftBeamFrequency = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "lightShaftBeamFrequency", ms_lightShaftBeamFrequency);
	ms_lightShaftBeamSharpness = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "lightShaftBeamSharpness", ms_lightShaftBeamSharpness);
	ms_lightShaftEdgeAttenuation = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "lightShaftEdgeAttenuation", ms_lightShaftEdgeAttenuation);
	ms_minFogDensityForLighting = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "minFogDensityForLighting", ms_minFogDensityForLighting);
	ms_sunDistance = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "sunDistance", ms_sunDistance);
	ms_minSunElevation = ConfigFile::getKeyFloat("ClientGame/VolumetricLighting", "minSunElevation", ms_minSunElevation);

	ms_maxFogAlpha = clampInclusive(ms_maxFogAlpha, 0.0f, 1.0f);
	ms_temporalSmoothing = clampInclusive(ms_temporalSmoothing, 0.0f, 1.0f);
	ms_lightShaftIntensity = std::max(ms_lightShaftIntensity, 0.0f);
	ms_lightShaftLength = std::max(ms_lightShaftLength, 0.0f);
	ms_lightShaftBeamFrequency = std::max(ms_lightShaftBeamFrequency, 0.0f);
	ms_lightShaftBeamSharpness = std::max(ms_lightShaftBeamSharpness, 0.0f);
	ms_lightShaftEdgeAttenuation = clampInclusive(ms_lightShaftEdgeAttenuation, 0.0f, 1.0f);
	ms_minFogDensityForLighting = std::max(ms_minFogDensityForLighting, 0.0f);
	ms_sunDistance = std::max(ms_sunDistance, 0.0f);

#ifdef _DEBUG
	DebugFlags::registerFlag(ms_enable, "ClientGame/VolumetricLighting", "enabled");
#endif

	if (ms_enable)
	        VolumetricLighting::enable();
}

// ----------------------------------------------------------------------

void VolumetricLighting::remove()
{
	VolumetricLighting::disable();
}

// ----------------------------------------------------------------------

bool VolumetricLighting::isSupported()
{
	return PostProcessingEffectsManager::isSupported();
}

// ----------------------------------------------------------------------

bool VolumetricLighting::isEnabled()
{
	return ms_enable;
}

// ----------------------------------------------------------------------

void VolumetricLighting::setEnabled(bool const enable)
{
	ms_enable = enable;
}

// ----------------------------------------------------------------------

void VolumetricLighting::enable()
{
        if (!ms_enabled)
        {
                if (VolumetricLighting::isSupported())
                {
                        if (!ms_callbacksRegistered)
                        {
                                Graphics::addDeviceLostCallback(VolumetricLightingNamespace::deviceLost);
                                Graphics::addDeviceRestoredCallback(VolumetricLightingNamespace::deviceRestored);
                                ms_callbacksRegistered = true;
                        }

                        VolumetricLightingNamespace::deviceRestored();
                }
                else
                {
                        ms_enable = false;
                        ms_enabled = false;
	        }
	}
}

// ----------------------------------------------------------------------

void VolumetricLighting::disable()
{
        if (ms_enabled)
        {
                VolumetricLightingNamespace::deviceLost();

                if (ms_callbacksRegistered)
                {
                        Graphics::removeDeviceLostCallback(VolumetricLightingNamespace::deviceLost);
                        Graphics::removeDeviceRestoredCallback(VolumetricLightingNamespace::deviceRestored);
                        ms_callbacksRegistered = false;
                }

                ms_enable = false;
        }
}

// ----------------------------------------------------------------------

void VolumetricLighting::preSceneRender()
{
	if (ms_enabled && !ms_enable)
	        VolumetricLighting::disable();
	else if (!ms_enabled && ms_enable)
	        VolumetricLighting::enable();
}

// ----------------------------------------------------------------------

namespace
{
	void applyFogGradient(int width, int height, VectorArgb const &fogColor, float fogAlpha)
	{
	        if (fogAlpha <= 0.0001f)
	                return;

	        VertexBufferFormat format;
	        format.setPosition();
	        format.setTransformed();
	        format.setColor0();

	        DynamicVertexBuffer vertexBuffer(format);
	        vertexBuffer.lock(4);
	        {
	                VectorArgb const topColor(fogAlpha * 0.35f, fogColor.r, fogColor.g, fogColor.b);
	                VectorArgb const bottomColor(fogAlpha, fogColor.r, fogColor.g, fogColor.b);

	                VertexBufferWriteIterator it = vertexBuffer.begin();

	                it.setPosition(Vector(-0.5f, -0.5f, 1.0f));
	                it.setOoz(1.0f);
	                it.setColor0(topColor);
	                ++it;

	                it.setPosition(Vector(static_cast<float>(width) - 0.5f, -0.5f, 1.0f));
	                it.setOoz(1.0f);
	                it.setColor0(topColor);
	                ++it;

	                it.setPosition(Vector(static_cast<float>(width) - 0.5f, static_cast<float>(height) - 0.5f, 1.0f));
	                it.setOoz(1.0f);
	                it.setColor0(bottomColor);
	                ++it;

	                it.setPosition(Vector(-0.5f, static_cast<float>(height) - 0.5f, 1.0f));
	                it.setOoz(1.0f);
	                it.setColor0(bottomColor);
	        }
	        vertexBuffer.unlock();

	        Graphics::setStaticShader(ShaderTemplateList::get2dVertexColorAStaticShader());
	        Graphics::setVertexBuffer(vertexBuffer);
	        Graphics::drawTriangleFan();
	}

	void applyLightShafts(int width, int height, float sunScreenX, float sunScreenY, VectorArgb const &fogColor, float intensity)
	{
	        if (intensity <= 0.0001f)
	                return;

	        int const segments = 24;

	        VertexBufferFormat format;
	        format.setPosition();
	        format.setTransformed();
	        format.setColor0();

	        DynamicVertexBuffer vertexBuffer(format);
	        vertexBuffer.lock(segments + 2);
	        {
	                float const radius = static_cast<float>(std::max(width, height)) * ms_lightShaftLength;
	                float const centerAlpha = intensity;
	                float const rimBaseAlpha = centerAlpha * ms_lightShaftEdgeAttenuation;

	                VertexBufferWriteIterator it = vertexBuffer.begin();

	                VectorArgb centerColor(centerAlpha, fogColor.r, fogColor.g, fogColor.b);
	                it.setPosition(Vector(sunScreenX - 0.5f, sunScreenY - 0.5f, 1.0f));
	                it.setOoz(1.0f);
	                it.setColor0(centerColor);
	                ++it;

	                for (int i = 0; i <= segments; ++i)
	                {
	                        float const t = static_cast<float>(i) / static_cast<float>(segments);
	                        float const angle = t * PI_TIMES_2;

	                        float const directionalMod = std::max(0.0f, std::cos(angle * ms_lightShaftBeamFrequency));
	                        float const beamStrength = directionalMod > 0.0f ? std::pow(directionalMod, ms_lightShaftBeamSharpness) : 0.0f;
	                        float const vertexAlpha = saturate(rimBaseAlpha + (centerAlpha * 0.75f * beamStrength));

	                        float const offsetX = std::cos(angle) * radius;
	                        float const offsetY = std::sin(angle) * radius;

	                        float const x = clampInclusive(sunScreenX + offsetX, -static_cast<float>(width), static_cast<float>(width) * 2.0f) - 0.5f;
	                        float const y = clampInclusive(sunScreenY + offsetY, -static_cast<float>(height), static_cast<float>(height) * 2.0f) - 0.5f;

	                        VectorArgb vertexColor(vertexAlpha, fogColor.r, fogColor.g, fogColor.b);

	                        it.setPosition(Vector(x, y, 1.0f));
	                        it.setOoz(1.0f);
	                        it.setColor0(vertexColor);
	                        ++it;
	                }
	        }
	        vertexBuffer.unlock();

	        Graphics::setStaticShader(ShaderTemplateList::get2dVertexColorAStaticShader());
	        Graphics::setVertexBuffer(vertexBuffer);
	        Graphics::drawTriangleFan();
	}
}

// ----------------------------------------------------------------------

void VolumetricLighting::postSceneRender()
{
	if (!ms_enabled)
	        return;

	if (Game::isSpace())
	{
	        ms_smoothedFogAlpha = lerp(ms_smoothedFogAlpha, 0.0f, ms_temporalSmoothing);
	        ms_smoothedLightAlpha = lerp(ms_smoothedLightAlpha, 0.0f, ms_temporalSmoothing);
	        return;
	}

	Texture * const primaryBuffer = PostProcessingEffectsManager::getPrimaryBuffer();
	Texture * const renderTarget = primaryBuffer;

	int frameWidth = Graphics::getCurrentRenderTargetWidth();
	int frameHeight = Graphics::getCurrentRenderTargetHeight();

	if (renderTarget)
	{
	        frameWidth = renderTarget->getWidth();
	        frameHeight = renderTarget->getHeight();
	        Graphics::setRenderTarget(renderTarget, CF_none, 0);
	}
	else
	{
	        Graphics::setRenderTarget(NULL, CF_none, 0);
	}

	if (frameWidth <= 0 || frameHeight <= 0)
	{
	        ms_smoothedFogAlpha = lerp(ms_smoothedFogAlpha, 0.0f, ms_temporalSmoothing);
	        ms_smoothedLightAlpha = lerp(ms_smoothedLightAlpha, 0.0f, ms_temporalSmoothing);
	        return;
	}

	Graphics::setViewport(0, 0, frameWidth, frameHeight, 0.0f, 1.0f);

	GroundEnvironment & groundEnvironment = GroundEnvironment::getInstance();
	float const fogDensity = groundEnvironment.getFogDensity();

	VectorArgb fogColor = groundEnvironment.getFogColor().convert();
	fogColor.a = 1.0f;

	float fogAlphaTarget = 0.0f;

	if (fogDensity > 0.0f)
	{
	        float const altitude = std::max(0.0f, ShaderPrimitiveSorter::getCurrentCamera().getPosition_w().y);
	        float const altitudeFactor = std::exp(-altitude * std::max(0.0f, ms_heightFogFalloff));

	        fogAlphaTarget = saturate(fogDensity * ms_fogDensityScale * altitudeFactor);
	        fogAlphaTarget = std::min(fogAlphaTarget, ms_maxFogAlpha);
	}

	ms_smoothedFogAlpha = lerp(ms_smoothedFogAlpha, fogAlphaTarget, ms_temporalSmoothing);
	float const fogAlpha = ms_smoothedFogAlpha;

	float lightAlphaTarget = 0.0f;
	float sunScreenX = 0.0f;
	float sunScreenY = 0.0f;

	if (fogDensity > ms_minFogDensityForLighting)
	{
	        Camera const & camera = ShaderPrimitiveSorter::getCurrentCamera();
	        Vector const cameraForward = camera.getObjectFrameK_w();
	        Vector const sunDirection = -groundEnvironment.getMainLight().getObjectFrameK_w();

	        float const viewDot = std::max(0.0f, cameraForward.dot(sunDirection));
	        float const sunElevation = saturate((sunDirection.y - ms_minSunElevation) / (1.0f - ms_minSunElevation));

	        if (viewDot > 0.0f && sunElevation > 0.0f)
	        {
	                Vector const sunPosition_w = camera.getPosition_w() - sunDirection * ms_sunDistance;

	                if (camera.projectInWorldSpace(sunPosition_w, &sunScreenX, &sunScreenY, 0, false))
	                {
	                        lightAlphaTarget = saturate(fogAlpha * ms_lightShaftIntensity * viewDot * sunElevation);
	                }
	        }
	}

	ms_smoothedLightAlpha = lerp(ms_smoothedLightAlpha, lightAlphaTarget, ms_temporalSmoothing);
	float const lightAlpha = ms_smoothedLightAlpha;

	GlFillMode const previousFillMode = Graphics::getFillMode();
	Graphics::setFillMode(GFM_solid);

	if (fogAlpha > 0.0001f)
	        applyFogGradient(frameWidth, frameHeight, fogColor, fogAlpha);

	if (lightAlpha > 0.0001f)
	        applyLightShafts(frameWidth, frameHeight, sunScreenX, sunScreenY, fogColor, lightAlpha);

	Graphics::setFillMode(previousFillMode);
}

// ----------------------------------------------------------------------

void VolumetricLightingNamespace::remove()
{
        VolumetricLighting::disable();
}

// ----------------------------------------------------------------------

void VolumetricLightingNamespace::deviceLost()
{
        ms_enabled = false;
        ms_smoothedFogAlpha = 0.0f;
        ms_smoothedLightAlpha = 0.0f;
}

// ----------------------------------------------------------------------

void VolumetricLightingNamespace::deviceRestored()
{
        if (!VolumetricLighting::isSupported())
        {
                ms_enable = false;
                ms_enabled = false;
                ms_smoothedFogAlpha = 0.0f;
                ms_smoothedLightAlpha = 0.0f;
                return;
        }

        ms_smoothedFogAlpha = 0.0f;
        ms_smoothedLightAlpha = 0.0f;
        ms_enabled = true;
}

// ----------------------------------------------------------------------

void VolumetricLighting::setFogDensityScale(float const scale)
{
	ms_fogDensityScale = std::max(scale, 0.0f);
}

// ----------------------------------------------------------------------

float VolumetricLighting::getFogDensityScale()
{
	return ms_fogDensityScale;
}

// ----------------------------------------------------------------------

void VolumetricLighting::setLightShaftIntensity(float const intensity)
{
	ms_lightShaftIntensity = std::max(intensity, 0.0f);
}

// ----------------------------------------------------------------------

float VolumetricLighting::getLightShaftIntensity()
{
	return ms_lightShaftIntensity;
}

// ======================================================================

