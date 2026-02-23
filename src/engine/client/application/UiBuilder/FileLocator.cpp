// ======================================================================
//
// FileLocator.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstUiBuilder.h"
#include "FileLocator.h"

#include <algorithm>
#include <stdio.h>

namespace
{
        // -----------------------------------------------------------------
        inline void trimWhitespace(UINarrowString &value)
        {
                const UINarrowString::size_type first = value.find_first_not_of(" \t\r\n");

                if (first == UINarrowString::npos)
                {
                        value.clear();
                        return;
                }

                const UINarrowString::size_type last = value.find_last_not_of(" \t\r\n");
                value = value.substr(first, last - first + 1);
        }

        // -----------------------------------------------------------------
        inline void normaliseSlashes(UINarrowString &value)
        {
                std::replace(value.begin(), value.end(), '\\', '/');

                UINarrowString cleaned;
                cleaned.reserve(value.size());

                bool previousWasSlash = false;

                UINarrowString::size_type index = 0;
                for (UINarrowString::const_iterator iter = value.begin(); iter != value.end(); ++iter, ++index)
                {
                        const char character = *iter;

                        if (character == '/')
                        {
                                if (previousWasSlash)
                                {
                                        const bool isUncPrefix = (index == 1) && !cleaned.empty() && cleaned[0] == '/';

                                        if (!isUncPrefix)
                                                continue;
                                }

                                previousWasSlash = true;
                        }
                        else
                        {
                                previousWasSlash = false;
                        }

                        cleaned.push_back(character);
                }

                value.swap(cleaned);
        }

        // -----------------------------------------------------------------
        inline void removeTrailingSlash(UINarrowString &value)
        {
                if (value.size() <= 1)
                        return;

                while (value.size() > 1 && value[value.size() - 1] == '/')
                {
                        const bool hasDriveSpecifier = (value.size() >= 2) && (value[value.size() - 2] == ':');

                        if (hasDriveSpecifier)
                                break;

                        value.erase(value.size() - 1);
                }
        }

        // -----------------------------------------------------------------
        inline void sanitisePath(UINarrowString &value)
        {
                trimWhitespace(value);
                normaliseSlashes(value);
                removeTrailingSlash(value);
        }

        // -----------------------------------------------------------------
        inline bool fileExists(UINarrowString const &path)
        {
                FILE * fl = fopen(path.c_str(), "r");

                if (!fl)
                        return false;

                fclose(fl);
                return true;
        }
}

// ======================================================================
FileLocator *   FileLocator::ms_gFileLocator = 0;

//-----------------------------------------------------------------

FileLocator::FileLocator () :
m_paths ()
{
}

//-----------------------------------------------------------------

const bool  FileLocator::findFile (const char * filename, UINarrowString & result)
{
        if (!filename || !*filename)
                return false;

        const UINarrowString fileKey(filename ? filename : "");

        if (!fileKey.empty())
        {
                const std::unordered_map<UINarrowString, UINarrowString>::const_iterator cacheIter = m_cachedResults.find(fileKey);

                if (cacheIter != m_cachedResults.end())
                {
                        if (fileExists(cacheIter->second))
                        {
                                result = cacheIter->second;
                                return true;
                        }

                        m_cachedResults.erase(cacheIter);
                }
        }

        for (PathVector_t::iterator iter = m_paths.begin (); iter != m_paths.end (); ++iter)
        {
                UINarrowString pathToCheck = *iter + "/" + filename;

                if (!fileExists(pathToCheck))
                        continue;

                result = pathToCheck;

                if (!fileKey.empty())
                        m_cachedResults[fileKey] = result;

                return true;
        }

        return false;
}
//-----------------------------------------------------------------
void  FileLocator::addPath (UINarrowString path)
{
        sanitisePath(path);

        if (path.empty())
                return;

        if (containsPath(path))
                return;

        m_paths.push_back (path);
        m_cachedResults.clear();
}
//-----------------------------------------------------------------
void FileLocator::addPaths(const std::vector<UINarrowString> &paths)
{
        for (std::vector<UINarrowString>::const_iterator iter = paths.begin(); iter != paths.end(); ++iter)
                addPath(*iter);
}
//-----------------------------------------------------------------
void FileLocator::setPaths(const std::vector<UINarrowString> &paths)
{
        clearPaths();
        addPaths(paths);
}
//-----------------------------------------------------------------
void FileLocator::clearPaths()
{
        m_paths.clear();
        m_cachedResults.clear();
}
//-----------------------------------------------------------------
std::vector<UINarrowString> FileLocator::getPaths() const
{
        return m_paths;
}
//-----------------------------------------------------------------
void FileLocator::ExplicitDestroy ()
{
        delete ms_gFileLocator;
        ms_gFileLocator = 0;
}

//-----------------------------------------------------------------
bool FileLocator::containsPath(UINarrowString const &path) const
{
        return std::find(m_paths.begin(), m_paths.end(), path) != m_paths.end();
}

// ======================================================================
