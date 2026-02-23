// ============================================================================
//
// FirstDirect3d10.h
//
// Bootstrapping header that mirrors the long-standing Direct3d9 convention.
// Including FirstSharedFoundation ensures the Windows wrapper and associated
// project-wide definitions are applied before any system headers are pulled
// in.  This avoids subtle build issues caused by missing SAL annotations or
// inconsistent Windows configuration when compiling with the legacy toolchain.
//
// ============================================================================

#ifndef INCLUDED_FirstDirect3d10_H
#define INCLUDED_FirstDirect3d10_H

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/FirstSharedFoundation.h"

#endif // INCLUDED_FirstDirect3d10_H

