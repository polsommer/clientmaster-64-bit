// ======================================================================
//
// RecentDirectory.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgContentBuilder.h"
#include "RecentDirectory.h"

#include <assert.h>
#include <tchar.h>

// ======================================================================

HKEY RecentDirectory::registryKey;
TCHAR RecentDirectory::buffer[_MAX_PATH];

// ======================================================================

void RecentDirectory::install(LPCTSTR path)
{
	const LONG result = RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &registryKey, NULL);
	assert(result == ERROR_SUCCESS);
}

// ----------------------------------------------------------------------

void RecentDirectory::remove()
{
	RegCloseKey(registryKey);
}

// ----------------------------------------------------------------------

LPCTSTR RecentDirectory::find(LPCTSTR type)
{
	DWORD dword = sizeof(buffer);
	DWORD dataType = 0;
	const LONG result = RegQueryValueEx(registryKey, type, NULL, &dataType, reinterpret_cast<BYTE*>(buffer), &dword);
	if (result == ERROR_SUCCESS && dataType == REG_SZ)
	        return buffer;

	return NULL;
}

// ----------------------------------------------------------------------

bool RecentDirectory::update(LPCTSTR type, LPCTSTR fileName)
{
	// chop off the file name
	_tcsncpy(buffer, fileName, _countof(buffer));
	buffer[_countof(buffer) - 1] = 0;

	TCHAR *slash = _tcsrchr(buffer, '\\');
	if (slash)
		*slash = 0;

	const size_t length = _tcslen(buffer) + 1;
	const DWORD bytes = static_cast<DWORD>(length * sizeof(TCHAR));
	const LONG result = RegSetValueEx(registryKey, type, 0, REG_SZ, reinterpret_cast<const BYTE*>(buffer), bytes);
	return (result == ERROR_SUCCESS);
}
// ======================================================================
