// ======================================================================
//
// FirstClientUserInterface.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FirstClientUserInterface_H
#define INCLUDED_FirstClientUserInterface_H

// ======================================================================

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/FirstSharedFoundation.h"
#include "_precompile.h"

// Enable volumetric lighting support for the UI layer only when explicitly requested
// by the build system.  The legacy Windows project files referenced by the community
// builds do not currently link the volumetric lighting implementation, which results
// in unresolved symbol errors when the UI attempts to invoke the pre/post scene hooks.
// Defaulting the feature flag to disabled keeps those builds functional while still
// allowing modernized build configurations to opt-in by defining the macro before
// including this header.
#ifndef CUI_SUPPORTS_VOLUMETRIC_LIGHTING
#define CUI_SUPPORTS_VOLUMETRIC_LIGHTING 0
#endif

// Screen space reflections rely on the modern post processing pipeline which is not
// available to the classic community build environment.  Mirror the volumetric
// lighting behaviour by disabling the feature unless the build explicitly opts in.
#ifndef CUI_SUPPORTS_SCREEN_SPACE_REFLECTIONS
#define CUI_SUPPORTS_SCREEN_SPACE_REFLECTIONS 0
#endif

// ======================================================================

#endif
