// ======================================================================
//
// OsNewDel.h
// Wrapper include that exposes the platform specific new/delete helpers
// used by the sharedMemoryManager library.  The original headers reside in
// the platform directories under src; by including them from this public
// location we satisfy includes of "sharedMemoryManager/OsNewDel.h" without
// altering existing project configurations.
//
// ======================================================================

#ifndef INCLUDED_sharedMemoryManager_OsNewDel_H
#define INCLUDED_sharedMemoryManager_OsNewDel_H

#if defined(_WIN32)
#include "../../src/win32/OsNewDel.h"
#else
#include "../../src/linux/OsNewDel.h"
#endif

#endif // INCLUDED_sharedMemoryManager_OsNewDel_H

