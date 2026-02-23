========================================================================
       MICROSOFT FOUNDATION CLASS LIBRARY : UiBuilder
========================================================================


AppWizard has created this UiBuilder application for you.  This application
not only demonstrates the basics of using the Microsoft Foundation classes
but is also a starting point for writing your application.

=============================
UIBuilderV2 modern experience
=============================

UIBuilderV2 reimagines the classic tooling with a modern workflow tailored for
building Star Wars Galaxies interfaces:

* Design-forward chrome – jump between Obsidian Pulse, Glacier Glass,
  Hologrid Indigo, Verdant Bloom, Amber Drift, Synthwave Vapor and Arctic Mist
  (View → Themes) to rapidly restyle the canvas while evaluating readability
  and contrast.
* Production overlays – enable layout guides, cinematic safe zones and enriched
  status metrics directly from the View menu.  These overlays provide immediate
  feedback on grid alignment, selection bounds and workspace health.
* Persisted preferences – appearance, overlays and metrics are written to a
  dedicated UIBuilderV2 registry hive so every launch resumes exactly where you
  left off.

This file contains a summary of what you will find in each of the files that
make up your UiBuilder application.

UiBuilder.dsp
    This file (the project file) contains information at the project level and
    is used to build a single project or subproject. Other users can share the
    project (.dsp) file, but they should export the makefiles locally.

UiBuilder.h
    This is the main header file for the application.  It includes other
    project specific headers (including Resource.h) and declares the
    CUiBuilderApp application class.

UiBuilder.cpp
    This is the main application source file that contains the application
    class CUiBuilderApp.

UiBuilder.rc
    This is a listing of all of the Microsoft Windows resources that the
    program uses.  It includes the icons, bitmaps, and cursors that are stored
    in the RES subdirectory.  This file can be directly edited in Microsoft
	Visual C++.

UiBuilder.clw
    This file contains information used by ClassWizard to edit existing
    classes or add new classes.  ClassWizard also uses this file to store
    information needed to create and edit message maps and dialog data
    maps and to create prototype member functions.

res\UiBuilder.ico
    This is an icon file, which is used as the application's icon.  This
    icon is included by the main resource file UiBuilder.rc.

res\UiBuilder.rc2
    This file contains resources that are not edited by Microsoft 
	Visual C++.  You should place all resources not editable by
	the resource editor in this file.



/////////////////////////////////////////////////////////////////////////////

For the main frame window:

MainFrm.h, MainFrm.cpp
    These files contain the frame class CMainFrame, which is derived from
    CFrameWnd and controls all SDI frame features.

res\Toolbar.bmp
    This bitmap file is used to create tiled images for the toolbar.
    The initial toolbar and status bar are constructed in the CMainFrame
    class. Edit this toolbar bitmap using the resource editor, and
    update the IDR_MAINFRAME TOOLBAR array in UiBuilder.rc to add
    toolbar buttons.
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////

Help Support:

hlp\UiBuilder.hpj
    This file is the Help Project file used by the Help compiler to create
    your application's Help file.

hlp\*.bmp
    These are bitmap files required by the standard Help file topics for
    Microsoft Foundation Class Library standard commands.

hlp\*.rtf
    This file contains the standard help topics for standard MFC
    commands and screen objects.

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named UiBuilder.pch and a precompiled types file named StdAfx.obj.

Resource.h
    This is the standard header file, which defines new resource IDs.
    Microsoft Visual C++ reads and updates this file.

/////////////////////////////////////////////////////////////////////////////
Other notes:

AppWizard uses "TODO:" to indicate parts of the source code you
should add to or customize.

If your application uses MFC in a shared DLL, and your application is
in a language other than the operating system's current language, you
will need to copy the corresponding localized resources MFC42XXX.DLL
from the Microsoft Visual C++ CD-ROM onto the system or system32 directory,
and rename it to be MFCLOC.DLL.  ("XXX" stands for the language abbreviation.
For example, MFC42DEU.DLL contains resources translated to German.)  If you
don't do this, some of the UI elements of your application will remain in the
language of the operating system.

/////////////////////////////////////////////////////////////////////////////

Modernisation notes (2024 update):

The UI Builder now resolves its asset search paths through a layered,
profile-aware configuration system that is easier to share across teams.
Paths are collected from the following sources (later entries override
earlier ones):

1. The working directory (`./`) is always searched first.
2. `uibuilder_paths.cfg` provides the primary configuration surface.  It
   supports named profiles, inheritance (via the `extends` directive) and
   nested includes so studios can define reusable layouts such as
   "default", "artists" or "build".  Each profile lists one `path = ...`
   directive per line; relative paths are resolved relative to the file
   that declares them, and environment variables or `~` are expanded.
   Select a profile at runtime by exporting `UIBUILDER_PROFILE`.
3. Environment variables remain available for rapid overrides.  Use
   `UIBUILDER_SEARCH_PATHS` or `UIBUILDER_EXTRA_PATHS` with a semicolon-
   or comma-separated list to add paths on the fly.
4. The legacy `uibuilder_searchpaths.cfg` file is still honoured for
   backwards compatibility.

Every discovered directory is normalised, validated and deduplicated
before the builder consumes it.  Results are also cached so that repeated
UI lookups stay fast even on very large projects.
