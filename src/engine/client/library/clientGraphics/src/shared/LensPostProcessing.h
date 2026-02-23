#ifndef INCLUDED_LensPostProcessing_H
#define INCLUDED_LensPostProcessing_H

class Texture;

namespace LensPostProcessing
{
        void apply(Texture &destination, Texture const &source, float chromaticAberrationStrength, float lensFlareStrength, float lensStreakStrength, float vignetteStrength, bool enableColorGrading, float colorGradeStrength, float colorGradeContrast, float colorGradeSaturation, float colorGradeTintStrength);
}

#endif // INCLUDED_LensPostProcessing_H
