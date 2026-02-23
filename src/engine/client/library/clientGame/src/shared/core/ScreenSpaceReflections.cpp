// ======================================================================
//
// ScreenSpaceReflections.cpp
//
// A minimal stub implementation that allows legacy tooling builds to
// link successfully even when the real screen space reflection effect is
// not available.  The community maintained Visual Studio projects do not
// currently build or link the modern renderer components that provide
// the feature.  Without these no-op hooks the UI layer attempts to call
// into missing symbols during its pre/post scene rendering passes,
// causing unresolved external errors at link time.  Providing an inert
// implementation keeps those builds functional while still allowing more
// complete pipelines to supply their own implementation by replacing
// this module.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ScreenSpaceReflections.h"

#include "sharedFoundation/ExitChain.h"

namespace
{
        bool s_installed = false;
        bool s_enabled = false;
}

// ----------------------------------------------------------------------

void ScreenSpaceReflections::install()
{
        if (s_installed)
                return;

        s_installed = true;
        s_enabled = false;

        ExitChain::add(ScreenSpaceReflections::remove, "ScreenSpaceReflections::remove");
}

// ----------------------------------------------------------------------

void ScreenSpaceReflections::remove()
{
        s_installed = false;
        s_enabled = false;
}

// ----------------------------------------------------------------------

bool ScreenSpaceReflections::isSupported()
{
        return false;
}

// ----------------------------------------------------------------------

bool ScreenSpaceReflections::isEnabled()
{
        return s_installed && s_enabled && isSupported();
}

// ----------------------------------------------------------------------

void ScreenSpaceReflections::setEnabled(bool const enabled)
{
        if (!s_installed)
                return;

        s_enabled = enabled && isSupported();
}

// ----------------------------------------------------------------------

void ScreenSpaceReflections::preSceneRender()
{
        if (!isEnabled())
                return;
}

// ----------------------------------------------------------------------

void ScreenSpaceReflections::postSceneRender()
{
        if (!isEnabled())
                return;
}

// ======================================================================

