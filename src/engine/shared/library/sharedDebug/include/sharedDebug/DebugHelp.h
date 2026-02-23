// ======================================================================
//
// DebugHelp.h
// Wrapper include that exposes the platform specific DebugHelp interface
// through the public sharedDebug include path.  Some legacy projects expect
// to include "sharedDebug/DebugHelp.h" directly; providing this header keeps
// those includes working without requiring project include path changes.
//
// ======================================================================

#ifndef INCLUDED_sharedDebug_DebugHelp_H
#define INCLUDED_sharedDebug_DebugHelp_H

#if defined(_WIN32)
#include "../../src/win32/DebugHelp.h"
#else
#include "../../src/linux/DebugHelp.h"
#endif

#endif // INCLUDED_sharedDebug_DebugHelp_H

