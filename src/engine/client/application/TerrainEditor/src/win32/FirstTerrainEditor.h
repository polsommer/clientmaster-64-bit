//
// FirstTerrainEditor.h
// asommers 
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#ifndef INCLUDED_FirstTerrainEditor_H
#define INCLUDED_FirstTerrainEditor_H

//-------------------------------------------------------------------

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

//-------------------------------------------------------------------
// TerrainEditor was authored against the multi-byte MFC build where
// TCHAR-based APIs resolved to char.  Modern Visual Studio defaults to
// UNICODE builds which flip the TCHAR based declarations to wide
// character versions and cause a large number of compilation errors
// when the legacy code passes narrow string literals or CString
// instances to engine functions that still expect narrow strings.  We
// explicitly drop the UNICODE/_UNICODE defines here so that this module
// continues to build against the multi-byte interfaces that it was
// originally written for.

#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif

//-------------------------------------------------------------------

#define NOMINMAX

//-------------------------------------------------------------------

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxcview.h>
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//-------------------------------------------------------------------

#include "sharedFoundation/FirstSharedFoundation.h"
#include "NumberEdit.h"
//-------------------------------------------------------------------

#endif 
