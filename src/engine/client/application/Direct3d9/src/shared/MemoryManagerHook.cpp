// ======================================================================
//
// MemoryManagerHook.cpp
// copyright 1998 Bootprint Entertainment
// copyright 1998 Sony Online Entertainment
//
// ======================================================================

#include "FirstDirect3d9.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// ======================================================================

// we are using the arguments (except for file and line), but MSVC can't tell that.
#pragma warning(disable: 4100)

// ======================================================================
// this is here because MSVC won't let me call MemoryManager::allocate() directly from inline assembly

static void * __cdecl localAllocate(size_t size, uint32 owner, bool array, bool leakTest)
{
	return MemoryManager::allocate(size, owner, array, leakTest);
}

// ----------------------------------------------------------------------

static uint32 getAllocationOwner()
{
#if defined(_MSC_VER) && defined(_M_IX86)
	return static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress()));
#else
	return 0;
#endif
}

// ======================================================================

void *operator new(size_t size, MemoryManagerNotALeak)
{
	return localAllocate(size, getAllocationOwner(), false, false);
}

// ----------------------------------------------------------------------

void *operator new(size_t size)
{
	return localAllocate(size, getAllocationOwner(), false, true);
}

// ----------------------------------------------------------------------

void *operator new[](size_t size)
{
	return localAllocate(size, getAllocationOwner(), true, true);
}

// ----------------------------------------------------------------------

void *operator new(size_t size, const char *file, int line)
{
	return localAllocate(size, getAllocationOwner(), false, true);
}

// ----------------------------------------------------------------------

void *operator new[](size_t size, const char *file, int line)
{
	return localAllocate(size, getAllocationOwner(), true, true);
}

// ----------------------------------------------------------------------

void operator delete(void *pointer)
{
	if (pointer)
		MemoryManager::free(pointer, false);
}

// ----------------------------------------------------------------------

void operator delete[](void *pointer)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ----------------------------------------------------------------------

void operator delete(void *pointer, const char *file, int line)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ----------------------------------------------------------------------

void operator delete[](void *pointer, const char *file, int line)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ======================================================================
