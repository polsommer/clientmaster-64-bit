// PRIVATE.  Do not export this header file outside the package.

// ======================================================================
//
// FoundationTypesWin32.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FoundationTypesWin32_H
#define INCLUDED_FoundationTypesWin32_H

// ======================================================================
// specify what platform we're running on.

#define PLATFORM_WIN32

// ======================================================================
// basic types that we assume to be around

#ifdef uint8
#undef uint8
#endif
typedef unsigned char          uint8;

#ifdef uint16
#undef uint16
#endif
typedef unsigned short         uint16;

#ifdef uint32
#undef uint32
#endif
typedef unsigned long          uint32;

#ifdef uint64
#undef uint64
#endif
typedef unsigned __int64       uint64;

#ifdef int8
#undef int8
#endif
typedef signed char            int8;

#ifdef int16
#undef int16
#endif
typedef signed short           int16;

#ifdef int32
#undef int32
#endif
typedef signed long            int32;

#ifdef int64
#undef int64
#endif
typedef signed __int64         int64;

#ifdef FILE_HANDLE
#undef FILE_HANDLE
#endif
typedef int                    FILE_HANDLE;

// ======================================================================

#endif
