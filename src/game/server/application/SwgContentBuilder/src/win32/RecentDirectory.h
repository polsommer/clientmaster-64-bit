// ======================================================================
//
// RecentDirectory.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_RecentDirectory_H
#define INCLUDED_RecentDirectory_H

// ======================================================================

class RecentDirectory
{
public:
	static void install(LPCTSTR registryKey);
	static void remove();

	static LPCTSTR find(LPCTSTR type);
	static bool        update(LPCTSTR type, LPCTSTR path);

private:

	static HKEY registryKey;
	static TCHAR buffer[_MAX_PATH];
};

// ======================================================================

#endif
