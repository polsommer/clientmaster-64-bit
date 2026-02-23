// ======================================================================
//
// FileLocator.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FileLocator_H
#define INCLUDED_FileLocator_H

#include "UIString.h"
#include <vector>
#include <unordered_map>

// ======================================================================
class FileLocator
{
public:

        const bool                findFile (const char * filename, UINarrowString & result);

        void                      addPath (UINarrowString path);

        void                      addPaths (const std::vector<UINarrowString> &paths);
        void                      setPaths (const std::vector<UINarrowString> &paths);
        void                      clearPaths ();
        std::vector<UINarrowString> getPaths () const;

        static FileLocator &      gFileLocator ();
        static void               ExplicitDestroy ();

private:
	                          FileLocator ();
	                          FileLocator (const FileLocator & rhs);
	FileLocator &             operator=    (const FileLocator & rhs);

        typedef std::vector <UINarrowString> PathVector_t;

        bool                      containsPath (UINarrowString const &path) const;

        PathVector_t              m_paths;
        mutable std::unordered_map<UINarrowString, UINarrowString> m_cachedResults;

        static FileLocator *      ms_gFileLocator;
};

// ======================================================================

inline FileLocator & FileLocator::gFileLocator ()
{
	if (ms_gFileLocator)
		return *ms_gFileLocator;

	return *(ms_gFileLocator = new FileLocator ());
}
//-----------------------------------------------------------------


		
#endif
