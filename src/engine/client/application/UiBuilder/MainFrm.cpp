// MainFrm.cpp : implementation of the CMainFrame class
//

#include "FirstUiBuilder.h"
#include "DefaultObjectPropertiesDialog.h"
#include "EditUtils.h"
#include "ObjectEditor.h"
#include "ObjectFactory.h"
#include "ObjectBrowserDialog.h"
#include "ObjectPropertiesDialog.h"
#include "ObjectPropertiesTreeDialog.h"
#include "SerializedObjectBuffer.h"
#include "UserWindowsMessages.h"

#include "UiBuilder.h"
#include "UIManager.h"
#include "UIPage.h"
#include "UIWidget.h"
#include "UIScriptEngine.h"
#include "UIDirect3DPrimaryCanvas.h"
#include "UIBuilderHistory.h"
#include "UIBuilderLoader.h"
#include "UITextStyleManager.h"
#include "UIVersion.h"
#include "UISaver.h"

#include "MainFrm.h"
#include "FileLocator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

/////////////////////////////////////////////////////////////////////////////

extern CUiBuilderApp theApp;

/////////////////////////////////////////////////////////////////////////////

namespace
{
        struct ConfiguredPath
        {
                std::string raw;
                std::string baseDirectory;
                std::string origin;
        };

        struct Profile
        {
                std::string key;
                std::string displayName;
                std::vector<ConfiguredPath> paths;
                std::vector<std::string> extends;
        };

        struct CandidatePath
        {
                std::string path;
                std::string source;
        };

        struct ResolvedSearchPaths
        {
                std::vector<CandidatePath> orderedPaths;
                std::string activeProfile;
                bool consumedLegacyFile;
                bool consumedEnvironment;

                ResolvedSearchPaths() :
                        orderedPaths(),
                        activeProfile(),
                        consumedLegacyFile(false),
                        consumedEnvironment(false)
                {
                }
        };

        // -----------------------------------------------------------------
        inline std::string trimCopy(const std::string &value)
        {
                const std::string::size_type first = value.find_first_not_of(" \t\r\n");

                if (first == std::string::npos)
                        return std::string();

                const std::string::size_type last = value.find_last_not_of(" \t\r\n");
                return value.substr(first, last - first + 1);
        }

        // -----------------------------------------------------------------
        inline std::string stripQuotes(const std::string &value)
        {
                if (value.size() >= 2)
                {
                        const char first = value.front();
                        const char last = value.back();

                        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
                                return value.substr(1, value.size() - 2);
                }

                return value;
        }

        // -----------------------------------------------------------------
        inline std::string toLowerCopy(const std::string &value)
        {
                std::string result = value;

                for (std::string::iterator iter = result.begin(); iter != result.end(); ++iter)
                        *iter = static_cast<char>(std::tolower(static_cast<unsigned char>(*iter)));

                return result;
        }

        // -----------------------------------------------------------------
        inline std::vector<std::string> splitList(const std::string &value)
        {
                std::vector<std::string> tokens;

                std::string::size_type start = 0;

                while (start < value.size())
                {
                        const std::string::size_type separator = value.find_first_of(";,", start);
                        const std::string token = value.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
                        const std::string cleaned = trimCopy(token);

                        if (!cleaned.empty())
                                tokens.push_back(cleaned);

                        if (separator == std::string::npos)
                                break;

                        start = separator + 1;
                }

                return tokens;
        }

        // -----------------------------------------------------------------
        inline std::string getDirectoryName(const std::string &path)
        {
                const std::string::size_type separator = path.find_last_of("/\\");

                if (separator == std::string::npos)
                        return std::string();

                return path.substr(0, separator);
        }

        // -----------------------------------------------------------------
        inline bool isAbsolutePath(const std::string &path)
        {
                if (path.empty())
                        return false;

                if (path[0] == '/' || path[0] == '\\')
                        return true;

                if (path.size() > 1 && path[1] == ':')
                        return true;

                return false;
        }

        // -----------------------------------------------------------------
        inline std::string joinPath(const std::string &base, const std::string &relative)
        {
                if (base.empty())
                        return relative;

                if (relative.empty())
                        return base;

                std::string result = base;

                const char last = result.empty() ? 0 : result[result.size() - 1];

                if (last != '/' && last != '\\')
                        result.push_back('/');

                result += relative;
                return result;
        }

        // -----------------------------------------------------------------
        inline std::string expandEnvironmentVariables(const std::string &value)
        {
#ifdef _WIN32
                if (value.find('%') == std::string::npos)
                        return value;

                char buffer[32767];
                const DWORD result = ExpandEnvironmentStringsA(value.c_str(), buffer, static_cast<DWORD>(sizeof(buffer)));

                if (result == 0 || result > static_cast<DWORD>(sizeof(buffer)))
                        return value;

                return std::string(buffer, result - 1);
#else
                std::string result;
                result.reserve(value.size());

                for (std::string::size_type index = 0; index < value.size();)
                {
                        const char current = value[index];

                        if (current != '$')
                        {
                                result.push_back(current);
                                ++index;
                                continue;
                        }

                        if (index + 1 < value.size() && value[index + 1] == '{')
                        {
                                const std::string::size_type closing = value.find('}', index + 2);

                                if (closing != std::string::npos)
                                {
                                        const std::string name = value.substr(index + 2, closing - (index + 2));
                                        const char * env = std::getenv(name.c_str());

                                        if (env)
                                                result.append(env);

                                        index = closing + 1;
                                        continue;
                                }
                        }

                        std::string::size_type start = index + 1;
                        std::string::size_type end = start;

                        while (end < value.size())
                        {
                                const unsigned char character = static_cast<unsigned char>(value[end]);

                                if (!std::isalnum(character) && character != '_')
                                        break;

                                ++end;
                        }

                        if (end > start)
                        {
                                const std::string name = value.substr(start, end - start);
                                const char * env = std::getenv(name.c_str());

                                if (env)
                                        result.append(env);

                                index = end;
                                continue;
                        }

                        result.push_back(current);
                        ++index;
                }

                return result;
#endif
        }

        // -----------------------------------------------------------------
        inline std::string expandUserDirectory(const std::string &value)
        {
                if (value.empty() || value[0] != '~')
                        return value;

                if (value.size() > 1 && value[1] != '/' && value[1] != '\\')
                        return value;

#ifdef _WIN32
                const char * home = std::getenv("USERPROFILE");
                std::string computedHome;

                if (!home)
                {
                        const char * drive = std::getenv("HOMEDRIVE");
                        const char * path = std::getenv("HOMEPATH");

                        if (drive && path)
                        {
                                computedHome = drive;
                                computedHome += path;
                                home = computedHome.c_str();
                        }
                }
#else
                const char * home = std::getenv("HOME");
#endif

                if (!home)
                        return value;

                return std::string(home) + value.substr(1);
        }

        // -----------------------------------------------------------------
        inline bool directoryExists(const std::string &path)
        {
#ifdef _WIN32
                const DWORD attributes = GetFileAttributesA(path.c_str());
                return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
                struct stat status;
                return stat(path.c_str(), &status) == 0 && (status.st_mode & S_IFDIR);
#endif
        }

        // -----------------------------------------------------------------
        inline std::string resolveConfiguredPath(const ConfiguredPath &entry)
        {
                std::string path = trimCopy(entry.raw);
                path = stripQuotes(path);
                path = expandUserDirectory(path);
                path = expandEnvironmentVariables(path);

                if (!entry.baseDirectory.empty() && !isAbsolutePath(path))
                        path = joinPath(entry.baseDirectory, path);

                return path;
        }

        // -----------------------------------------------------------------
        inline bool collectEnvironmentPaths(const char *variable, std::vector<CandidatePath> &output)
        {
                const char * value = std::getenv(variable);

                if (!value || !*value)
                        return false;

                const std::vector<std::string> tokens = splitList(value);
                bool appended = false;

                for (std::vector<std::string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
                {
                        const std::string cleaned = stripQuotes(trimCopy(*iter));

                        if (cleaned.empty())
                                continue;

                        CandidatePath candidate;
                        candidate.path = cleaned;
                        candidate.source = std::string("env/") + variable;
                        output.push_back(candidate);
                        appended = true;
                }

                return appended;
        }

        // -----------------------------------------------------------------
        inline bool loadLegacyPaths(const char *fileName, std::vector<CandidatePath> &output)
        {
                std::ifstream stream(fileName);

                if (!stream.is_open())
                        return false;

                std::string line;
                int lineNumber = 0;
                bool appended = false;

                while (std::getline(stream, line))
                {
                        ++lineNumber;

                        const std::string trimmed = trimCopy(line);

                        if (trimmed.empty())
                                continue;

                        if (trimmed[0] == '#' || trimmed[0] == ';')
                                continue;

                        CandidatePath candidate;
                        candidate.path = trimmed;

                        std::ostringstream source;
                        source << "legacy/" << fileName << ':' << lineNumber;
                        candidate.source = source.str();

                        output.push_back(candidate);
                        appended = true;
                }

                return appended;
        }

        // -----------------------------------------------------------------
        inline void parsePathsConfig(const std::string &fileName, std::map<std::string, Profile> &profiles, std::set<std::string> &visitedFiles, int depth = 0)
        {
                if (depth > 16)
                {
#ifdef _DEBUG
                        TRACE("UIBuilderV2: include depth exceeded while parsing '%s'.\n", fileName.c_str());
#endif
                        return;
                }

                const std::string visitedKey = toLowerCopy(fileName);

                if (!visitedFiles.insert(visitedKey).second)
                        return;

                std::ifstream stream(fileName.c_str());

                if (!stream.is_open())
                {
#ifdef _DEBUG
                        TRACE("UIBuilderV2: optional search path configuration '%s' was not found.\n", fileName.c_str());
#endif
                        return;
                }

                const std::string baseDirectory = getDirectoryName(fileName);

                Profile &defaultProfile = profiles[toLowerCopy("default")];

                if (defaultProfile.key.empty())
                {
                        defaultProfile.key = "default";
                        defaultProfile.displayName = "default";
                }

                Profile *activeProfile = &defaultProfile;

                std::string line;
                int lineNumber = 0;
                std::vector<std::string> includes;

                while (std::getline(stream, line))
                {
                        ++lineNumber;

                        std::string trimmed = trimCopy(line);

                        if (trimmed.empty())
                                continue;

                        const char firstCharacter = trimmed[0];

                        if (firstCharacter == '#' || firstCharacter == ';')
                                continue;

                        if (trimmed.front() == '[' && trimmed.back() == ']')
                        {
                                const std::string name = trimCopy(trimmed.substr(1, trimmed.size() - 2));
                                const std::string key = toLowerCopy(name.empty() ? std::string("default") : name);

                                Profile &profile = profiles[key];

                                if (profile.key.empty())
                                        profile.key = key;

                                if (profile.displayName.empty())
                                        profile.displayName = name.empty() ? "default" : name;

                                activeProfile = &profile;
                                continue;
                        }

                        std::string key;
                        std::string value;
                        const std::string::size_type equals = trimmed.find('=');

                        if (equals == std::string::npos)
                        {
                                key = "path";
                                value = trimmed;
                        }
                        else
                        {
                                key = trimCopy(trimmed.substr(0, equals));
                                value = trimCopy(trimmed.substr(equals + 1));
                        }

                        const std::string loweredKey = toLowerCopy(key);

                        if (loweredKey == "path" || loweredKey == "add" || loweredKey == "directory" || loweredKey == "dir")
                        {
                                ConfiguredPath entry;
                                entry.raw = value;
                                entry.baseDirectory = baseDirectory;

                                std::ostringstream origin;
                                origin << fileName << ':' << lineNumber;
                                entry.origin = origin.str();

                                activeProfile->paths.push_back(entry);
                        }
                        else if (loweredKey == "extends" || loweredKey == "inherit" || loweredKey == "inherits")
                        {
                                const std::vector<std::string> tokens = splitList(value);

                                for (std::vector<std::string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
                                {
                                        const std::string cleaned = toLowerCopy(trimCopy(*iter));

                                        if (!cleaned.empty())
                                                activeProfile->extends.push_back(cleaned);
                                }
                        }
                        else if (loweredKey == "include")
                        {
                                const std::vector<std::string> tokens = splitList(value);

                                for (std::vector<std::string>::const_iterator iter = tokens.begin(); iter != tokens.end(); ++iter)
                                {
                                        std::string includePath = stripQuotes(trimCopy(*iter));
                                        includePath = expandUserDirectory(includePath);
                                        includePath = expandEnvironmentVariables(includePath);

                                        if (!isAbsolutePath(includePath))
                                                includePath = joinPath(baseDirectory, includePath);

                                        includes.push_back(includePath);
                                }
                        }
                        else
                        {
#ifdef _DEBUG
                                TRACE("UIBuilderV2: unknown directive '%s' ignored in %s:%d.\n", key.c_str(), fileName.c_str(), lineNumber);
#endif
                        }
                }

                for (std::vector<std::string>::const_iterator includeIter = includes.begin(); includeIter != includes.end(); ++includeIter)
                        parsePathsConfig(*includeIter, profiles, visitedFiles, depth + 1);
        }

        // -----------------------------------------------------------------
        inline void collectProfilePaths(const std::string &profileKey, const std::map<std::string, Profile> &profiles, std::set<std::string> &resolutionStack, std::vector<CandidatePath> &output)
        {
                if (!resolutionStack.insert(profileKey).second)
                {
#ifdef _DEBUG
                        std::map<std::string, Profile>::const_iterator profileIter = profiles.find(profileKey);
                        const std::string &name = profileIter != profiles.end() && !profileIter->second.displayName.empty() ? profileIter->second.displayName : profileKey;
                        TRACE("UIBuilderV2: cyclic profile inheritance detected involving '%s'.\n", name.c_str());
#endif
                        return;
                }

                const std::map<std::string, Profile>::const_iterator profileIter = profiles.find(profileKey);

                if (profileIter == profiles.end())
                {
#ifdef _DEBUG
                        TRACE("UIBuilderV2: requested search path profile '%s' was not defined.\n", profileKey.c_str());
#endif
                        resolutionStack.erase(profileKey);
                        return;
                }

                const Profile &profile = profileIter->second;

                for (std::vector<std::string>::const_iterator extendIter = profile.extends.begin(); extendIter != profile.extends.end(); ++extendIter)
                        collectProfilePaths(*extendIter, profiles, resolutionStack, output);

                for (std::vector<ConfiguredPath>::const_iterator pathIter = profile.paths.begin(); pathIter != profile.paths.end(); ++pathIter)
                {
                        CandidatePath candidate;
                        candidate.path = resolveConfiguredPath(*pathIter);

                        if (!profile.displayName.empty())
                                candidate.source = "profile/" + profile.displayName;
                        else
                                candidate.source = "profile/" + profile.key;

                        if (!pathIter->origin.empty())
                                candidate.source += " (" + pathIter->origin + ")";

                        output.push_back(candidate);
                }

                resolutionStack.erase(profileKey);
        }

        // -----------------------------------------------------------------
        inline ResolvedSearchPaths resolveSearchPaths()
        {
                ResolvedSearchPaths resolved;
                resolved.orderedPaths.push_back(CandidatePath());
                resolved.orderedPaths.back().path = "./";
                resolved.orderedPaths.back().source = "working-directory";

                std::map<std::string, Profile> profiles;
                std::set<std::string> visitedFiles;
                parsePathsConfig("uibuilder_paths.cfg", profiles, visitedFiles);

                const char * profileOverride = std::getenv("UIBUILDER_PROFILE");
                const std::string requestedProfile = trimCopy(profileOverride ? profileOverride : "");
                std::string profileKey = toLowerCopy(requestedProfile);

                if (profiles.empty())
                {
                        resolved.activeProfile = requestedProfile;
                }
                else
                {
                        if (profileKey.empty())
                                profileKey = "default";

                        if (profiles.find(profileKey) == profiles.end())
                        {
                                const std::map<std::string, Profile>::const_iterator defaultIter = profiles.find("default");

                                if (defaultIter != profiles.end())
                                        profileKey = defaultIter->first;
                                else
                                        profileKey = profiles.begin()->first;
                        }

                        const Profile &activeProfile = profiles[profileKey];
                        resolved.activeProfile = !activeProfile.displayName.empty() ? activeProfile.displayName : activeProfile.key;

                        std::set<std::string> stack;
                        collectProfilePaths(profileKey, profiles, stack, resolved.orderedPaths);
                }

                if (collectEnvironmentPaths("UIBUILDER_SEARCH_PATHS", resolved.orderedPaths))
                        resolved.consumedEnvironment = true;

                if (collectEnvironmentPaths("UIBUILDER_EXTRA_PATHS", resolved.orderedPaths))
                        resolved.consumedEnvironment = true;

                if (loadLegacyPaths("uibuilder_searchpaths.cfg", resolved.orderedPaths))
                        resolved.consumedLegacyFile = true;

                return resolved;
        }

        // -----------------------------------------------------------------
        inline bool registerSearchPath(FileLocator &locator, const CandidatePath &candidate)
        {
                std::string cleaned = stripQuotes(trimCopy(candidate.path));

                if (cleaned.empty())
                        return false;

                cleaned = expandUserDirectory(cleaned);
                cleaned = expandEnvironmentVariables(cleaned);

                if (!directoryExists(cleaned))
                {
#ifdef _DEBUG
                        TRACE("UIBuilderV2: skipping search path '%s' from %s because the directory does not exist.\n", cleaned.c_str(), candidate.source.c_str());
#endif
                        return false;
                }

                locator.addPath(cleaned.c_str());

#ifdef _DEBUG
                TRACE("UIBuilderV2: registered search path '%s' from %s.\n", cleaned.c_str(), candidate.source.c_str());
#endif

                return true;
        }

        // -----------------------------------------------------------------
        inline void configureFileLocator(FileLocator &locator)
        {
                const ResolvedSearchPaths resolved = resolveSearchPaths();

                locator.clearPaths();

                for (std::vector<CandidatePath>::const_iterator iter = resolved.orderedPaths.begin(); iter != resolved.orderedPaths.end(); ++iter)
                {
                        registerSearchPath(locator, *iter);
                }

#ifdef _DEBUG
                TRACE("UIBuilderV2: active search path profile '%s' (env=%s, legacy=%s).\n",
                        resolved.activeProfile.empty() ? "default" : resolved.activeProfile.c_str(),
                        resolved.consumedEnvironment ? "yes" : "no",
                        resolved.consumedLegacyFile ? "yes" : "no");

                const std::vector<UINarrowString> finalPaths = locator.getPaths();
                TRACE("UIBuilderV2: %u search path(s) ready.\n", static_cast<unsigned>(finalPaths.size()));

                for (std::vector<UINarrowString>::const_iterator iter = finalPaths.begin(); iter != finalPaths.end(); ++iter)
                        TRACE("   -> %s\n", iter->c_str());
#endif
        }

        static const TCHAR * const sAppearanceSection             = _T("UIBuilderV2\\Appearance");
        static const TCHAR * const sOverlaySection                = _T("UIBuilderV2\\Overlays");
        static const TCHAR * const sWorkspaceSection              = _T("UIBuilderV2\\Workspace");
        static const TCHAR * const sThemeValueName                = _T("Theme");
        static const TCHAR * const sLayoutGuidesValueName         = _T("LayoutGuides");
        static const TCHAR * const sSafeZonesValueName            = _T("SafeZones");
        static const TCHAR * const sSafeZonePadXValueName         = _T("SafeZonePadX");
        static const TCHAR * const sSafeZonePadYValueName         = _T("SafeZonePadY");
        static const TCHAR * const sStatusMetricsValueName        = _T("StatusMetrics");
        static const TCHAR * const sAssistantValueName            = _T("AssistantOverlays");
        static const TCHAR * const sFocusModeValueName            = _T("FocusMode");
        static const int           s_safeZonePaddingScale         = 100;
}

const long               LineWidth             = 1;
const long               HandleSize            = 4;
const char               *gApplicationName            = "UIBuilderV2";
const char               *gDefaultExtension           = "ui";
UILowerString             gVisualEditLockPropertyName("VisualEditLock");
const char               *gFileFilter                 = "User Interface Files (*.ui)|ui_*.ui||";
const char               *gIncludeFileFilter          = "User Interface Include Files (.inc)\0ui_*.inc\0\0";

UIDirect3DPrimaryCanvas  *gPrimaryDisplay      = 0;

UINT CMainFrame::m_clipboardFormat=RegisterClipboardFormat("UIBuilderV2SerializedObjectTree");

UIColor CMainFrame::m_highlightOutlineColor          (  0, 220, 180, 255);
UIColor CMainFrame::m_selectionOutlineColor          ( 72, 163, 255, 255);
UIColor CMainFrame::m_highlightSelectionOutlineColor (255, 214,  10, 255);
UIColor CMainFrame::m_selectionFillColor             ( 72, 163, 255,  32);
UIColor CMainFrame::m_selectionBoxOutlineColor       ( 96, 164, 255, 192);
UIColor CMainFrame::m_selectionDragBoxOutlineColor   (  0, 196, 255, 255);

/////////////////////////////////////////////////////////////////////////////

void RecursiveSetProperty( UIBaseObject *root, const UILowerString & PropertyName, const char *OldValue, const char *NewValue )
{
	UIString Value;

	if( root->GetProperty( PropertyName, Value ) )
	{
		if( !_stricmp( UIUnicode::wideToNarrow (Value).c_str(), OldValue ) )
			root->SetProperty( PropertyName, UIUnicode::narrowToWide (NewValue) );

		UIBaseObject::UIObjectList Children;

		root->GetChildren( Children );

		for( UIBaseObject::UIObjectList::iterator i = Children.begin(); i != Children.end(); ++i )
		{
			RecursiveSetProperty( *i, PropertyName, OldValue, NewValue );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_UPDATE_COMMAND_UI(ID_FILE_NEW, OnUpdateFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN, OnUpdateFileOpen)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE, OnUpdateFileClose)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVEAS, OnUpdateFileSaveas)
	ON_WM_SIZE()
	ON_WM_DROPFILES()
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_VIEW_DEFAULT_PROPERTIES, OnViewDefaultProperties)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DEFAULT_PROPERTIES, OnUpdateViewDefaultProperties)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_COMMAND(ID_SELECTION_BURROW, OnSelectionBurrow)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_BURROW, OnUpdateSelectionBurrow)
	ON_COMMAND(ID_SELECTION_CLEARALL, OnSelectionClearall)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_CLEARALL, OnUpdateSelectionClearall)
	ON_COMMAND(ID_SELECTION_DESCENDANTS, OnSelectionDescendants)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_DESCENDANTS, OnUpdateSelectionDescendants)
	ON_COMMAND(ID_SELECTION_ANCESTORS, OnSelectionAncestors)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ANCESTORS, OnUpdateSelectionAncestors)
	ON_COMMAND(ID_EDIT_CANCELDRAG, OnEditCanceldrag)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CANCELDRAG, OnUpdateEditCanceldrag)
	ON_COMMAND(ID_SELECTION_DELETE, OnSelectionDelete)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_DELETE, OnUpdateSelectionDelete)
	ON_WM_ACTIVATEAPP()
	ON_COMMAND(ID_TOGGLE_GRID, OnToggleGrid)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_GRID, OnUpdateToggleGrid)
	ON_COMMAND(ID_SELECTION_ALIGNBOTTOM, OnSelectionAlignbottom)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNBOTTOM, OnUpdateSelectionAlignbottom)
	ON_COMMAND(ID_SELECTION_ALIGNLEFT, OnSelectionAlignleft)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNLEFT, OnUpdateSelectionAlignleft)
	ON_COMMAND(ID_SELECTION_ALIGNRIGHT, OnSelectionAlignright)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNRIGHT, OnUpdateSelectionAlignright)
	ON_COMMAND(ID_SELECTION_ALIGNTOP, OnSelectionAligntop)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNTOP, OnUpdateSelectionAligntop)
	ON_COMMAND(ID_SELECTION_ALIGNWIDTH, OnSelectionAlignwidth)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNWIDTH, OnUpdateSelectionAlignwidth)
	ON_COMMAND(ID_SELECTION_ALIGNHEIGHT, OnSelectionAlignheight)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNHEIGHT, OnUpdateSelectionAlignheight)
	ON_COMMAND(ID_SELECTION_ALIGNCENTERX, OnSelectionAligncenterx)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNCENTERX, OnUpdateSelectionAligncenterx)
	ON_COMMAND(ID_SELECTION_ALIGNCENTERY, OnSelectionAligncentery)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_ALIGNCENTERY, OnUpdateSelectionAligncentery)
	ON_COMMAND(ID_SELECTION_SMARTUPSCALE, OnSelectionSmartupscale)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_SMARTUPSCALE, OnUpdateSelectionSmartupscale)
	ON_COMMAND(ID_SELECTION_AUTOUPSCALE, OnSelectionAutoupscale)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_AUTOUPSCALE, OnUpdateSelectionAutoupscale)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_COMMAND(IDC_CHECKOUT, OnCheckout)
	ON_UPDATE_COMMAND_UI(IDC_CHECKOUT, OnUpdateCheckout)
        ON_COMMAND(ID_VIEW_OBJECTBROWSER, OnViewObjectbrowser)
        ON_UPDATE_COMMAND_UI(ID_VIEW_OBJECTBROWSER, OnUpdateViewObjectbrowser)
        ON_COMMAND(ID_VIEW_SELECTIONPROPERTIES, OnViewSelectionproperties)
        ON_UPDATE_COMMAND_UI(ID_VIEW_SELECTIONPROPERTIES, OnUpdateViewSelectionproperties)
        ON_COMMAND(ID_VIEW_THEME_DARK, OnViewThemeDark)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_DARK, OnUpdateViewThemeDark)
        ON_COMMAND(ID_VIEW_THEME_LIGHT, OnViewThemeLight)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_LIGHT, OnUpdateViewThemeLight)
        ON_COMMAND(ID_VIEW_THEME_BLUEPRINT, OnViewThemeBlueprint)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_BLUEPRINT, OnUpdateViewThemeBlueprint)
        ON_COMMAND(ID_VIEW_THEME_AURORA, OnViewThemeAurora)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_AURORA, OnUpdateViewThemeAurora)
        ON_COMMAND(ID_VIEW_THEME_SOLAR, OnViewThemeSolar)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_SOLAR, OnUpdateViewThemeSolar)
        ON_COMMAND(ID_VIEW_THEME_CARBON, OnViewThemeCarbon)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_CARBON, OnUpdateViewThemeCarbon)
        ON_COMMAND(ID_VIEW_THEME_NORDIC, OnViewThemeNordic)
        ON_UPDATE_COMMAND_UI(ID_VIEW_THEME_NORDIC, OnUpdateViewThemeNordic)
        ON_COMMAND(ID_VIEW_TOGGLE_LAYOUTGUIDES, OnViewToggleLayoutguides)
        ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLE_LAYOUTGUIDES, OnUpdateViewToggleLayoutguides)
        ON_COMMAND(ID_VIEW_TOGGLE_SAFEZONES, OnViewToggleSafezones)
        ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLE_SAFEZONES, OnUpdateViewToggleSafezones)
        ON_COMMAND(ID_VIEW_TOGGLE_STATUSMETRICS, OnViewToggleStatusmetrics)
        ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLE_STATUSMETRICS, OnUpdateViewToggleStatusmetrics)
        ON_COMMAND(ID_VIEW_TOGGLE_ASSISTANT, OnViewToggleAssistant)
        ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLE_ASSISTANT, OnUpdateViewToggleAssistant)
        ON_COMMAND(ID_VIEW_TOGGLE_FOCUSMODE, OnViewToggleFocusmode)
        ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLE_FOCUSMODE, OnUpdateViewToggleFocusmode)
        ON_COMMAND(ID_TOOLS_AUTOLAYOUT_RESPONSIVE, OnToolsAutolayoutResponsive)
        ON_UPDATE_COMMAND_UI(ID_TOOLS_AUTOLAYOUT_RESPONSIVE, OnUpdateToolsAutolayoutResponsive)
        ON_COMMAND(ID_TOOLS_BALANCEPADDING, OnToolsBalancepadding)
        ON_UPDATE_COMMAND_UI(ID_TOOLS_BALANCEPADDING, OnUpdateToolsBalancepadding)
        //}}AFX_MSG_MAP
	// Global help commands
	ON_MESSAGE(WM_paintChild, OnPaintChild)
	ON_MESSAGE(WM_closePropertiesDialog, closePropertiesDialog)
	ON_MESSAGE(WM_closeObjectBrowserDialog, closeObjectBrowserDialog)
	ON_COMMAND_RANGE(ID_INSERT_NAMESPACE, ID_INSERT_DEFORMER_ROTATE, OnInsertObject)
	ON_UPDATE_COMMAND_UI_RANGE(ID_INSERT_NAMESPACE, ID_INSERT_DEFORMER_ROTATE, OnUpdateInsertObject)
	ON_COMMAND(ID_HELP_FINDER, CFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CFrameWnd::OnHelpFinder)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
:	m_editor(0),
	m_browserDialog(0),
	m_propertiesDialog(0),
	m_factory(0),
	m_defaultsManager(0),
	gVersionFilename(false),
	g_showShaders(false),
	m_inVisualEditingMode(true),
	gDrawCursor(true),
	gDrawGrid(false),
	m_drawSelectionRect(true),
	m_showActive(false),
	m_browserDialogOpen(false),
	m_propertiesDialogOpen(false),
        m_refreshTimer(0),
        gGridColor(72,112,160,56),
	gGridMajorTicks(10),
	gTriangleCount(0),
	gFlushCount(0),
        gFrameCount(0),
        m_showLayoutGuides(true),
        m_showSafeZones(false),
        m_showStatusMetrics(true),
        m_showAssistantOverlay(true),
        m_focusMode(false),
        m_activeTheme(ThemeStyleNeoDark),
        m_canvasBackgroundColor(12, 16, 24, 255),
        m_layoutGuideColor(64, 196, 255, 180),
        m_safeZoneColor(255, 196,   0,  48),
        m_focusOverlayColor(0, 0, 0, 140),
        m_safeZoneHorizontalPadding(0.05f),
        m_safeZoneVerticalPadding(0.05f)
{
	m_cursors[CT_Normal]    = LoadCursor(0, IDC_ARROW);
	m_cursors[CT_Crosshair] = LoadCursor(0, IDC_CROSS);
	m_cursors[CT_Hand]      = LoadCursor(0, MAKEINTRESOURCE(IDC_GRABHAND));
	m_cursors[CT_SizeAll]   = LoadCursor(0, IDC_SIZEALL);
	m_cursors[CT_SizeNESW]  = LoadCursor(0, IDC_SIZENESW);
	m_cursors[CT_SizeNS]    = LoadCursor(0, IDC_SIZENS);
	m_cursors[CT_SizeNWSE]  = LoadCursor(0, IDC_SIZENWSE);
	m_cursors[CT_SizeWE]    = LoadCursor(0, IDC_SIZEWE);
	m_cursors[CT_Wait]      = LoadCursor(0, IDC_WAIT);
}

CMainFrame::~CMainFrame()
{
	_destroyEditingObjects();
}

bool CMainFrame::openFile(const char *i_fileName)
{
	return _openWorkspaceFile(i_fileName);
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	CRect rc;
	rc.left=0;
	rc.right=1024;
	rc.top=0;
	rc.bottom=768;
	AdjustWindowRectEx(&rc, lpCreateStruct->style, lpCreateStruct->hMenu!=0, lpCreateStruct->dwExStyle);
	lpCreateStruct->cx = rc.Width();
	lpCreateStruct->cy = rc.Height();

	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_wndView.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	
//AFX_IDW_TOOLBAR
	if (!m_wndToolBar.CreateEx(this) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

//AFX_IDW_DIALOGBAR
	if (!m_wndDlgBar.Create(this, IDR_MAINFRAME, 
		CBRS_ALIGN_TOP, AFX_IDW_DIALOGBAR))
	{
		TRACE0("Failed to create dialogbar\n");
		return -1;		// fail to create
	}

	if (!m_wndReBar.Create(this) ||
		!m_wndReBar.AddBar(&m_wndToolBar) ||
		!m_wndReBar.AddBar(&m_wndDlgBar))
	{
		TRACE0("Failed to create rebar\n");
		return -1;      // fail to create
	}

//AFX_IDW_STATUS_BAR
	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// TODO: Remove this if you don't want tool tips
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY);

	// Reuse existing toolbar imagery for the new upscale commands so they have
	// meaningful icons without requiring new artwork resources.
	int widthIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNWIDTH);
	if (widthIndex >= 0)
	{
		UINT existingId = 0;
		UINT existingStyle = 0;
		int existingImage = 0;
		m_wndToolBar.GetButtonInfo(widthIndex, existingId, existingStyle, existingImage);

		int smartIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_SMARTUPSCALE);
		if (smartIndex >= 0)
		{
			m_wndToolBar.SetButtonInfo(smartIndex, ID_SELECTION_SMARTUPSCALE, existingStyle, existingImage);
		}
	}

        int heightIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNHEIGHT);
        if (heightIndex >= 0)
        {
                UINT existingId = 0;
                UINT existingStyle = 0;
                int existingImage = 0;
                m_wndToolBar.GetButtonInfo(heightIndex, existingId, existingStyle, existingImage);

                int autoIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_AUTOUPSCALE);
                if (autoIndex >= 0)
                {
                        m_wndToolBar.SetButtonInfo(autoIndex, ID_SELECTION_AUTOUPSCALE, existingStyle, existingImage);
                }
        }

        int centerXIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNCENTERX);
        if (centerXIndex >= 0)
        {
                UINT existingId = 0;
                UINT existingStyle = 0;
                int existingImage = 0;
                m_wndToolBar.GetButtonInfo(centerXIndex, existingId, existingStyle, existingImage);

                const int responsiveIndex = m_wndToolBar.CommandToIndex(ID_TOOLS_AUTOLAYOUT_RESPONSIVE);
                if (responsiveIndex >= 0)
                {
                        m_wndToolBar.SetButtonInfo(responsiveIndex, ID_TOOLS_AUTOLAYOUT_RESPONSIVE, existingStyle, existingImage);
                }
        }

        int centerYIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNCENTERY);
        if (centerYIndex >= 0)
        {
                UINT existingId = 0;
                UINT existingStyle = 0;
                int existingImage = 0;
                m_wndToolBar.GetButtonInfo(centerYIndex, existingId, existingStyle, existingImage);

                const int balanceIndex = m_wndToolBar.CommandToIndex(ID_TOOLS_BALANCEPADDING);
                if (balanceIndex >= 0)
                {
                        m_wndToolBar.SetButtonInfo(balanceIndex, ID_TOOLS_BALANCEPADDING, existingStyle, existingImage);
                }
        }

        int alignLeftIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNLEFT);
        if (alignLeftIndex >= 0)
        {
                UINT existingId = 0;
                UINT existingStyle = 0;
                int existingImage = 0;
                m_wndToolBar.GetButtonInfo(alignLeftIndex, existingId, existingStyle, existingImage);

                const int assistantIndex = m_wndToolBar.CommandToIndex(ID_VIEW_TOGGLE_ASSISTANT);
                if (assistantIndex >= 0)
                {
                        m_wndToolBar.SetButtonInfo(assistantIndex, ID_VIEW_TOGGLE_ASSISTANT, existingStyle, existingImage);
                        m_wndToolBar.SetButtonStyle(assistantIndex, TBBS_CHECKBOX);
                }
        }

        int alignRightIndex = m_wndToolBar.CommandToIndex(ID_SELECTION_ALIGNRIGHT);
        if (alignRightIndex >= 0)
        {
                UINT existingId = 0;
                UINT existingStyle = 0;
                int existingImage = 0;
                m_wndToolBar.GetButtonInfo(alignRightIndex, existingId, existingStyle, existingImage);

                const int focusIndex = m_wndToolBar.CommandToIndex(ID_VIEW_TOGGLE_FOCUSMODE);
                if (focusIndex >= 0)
                {
                        m_wndToolBar.SetButtonInfo(focusIndex, ID_VIEW_TOGGLE_FOCUSMODE, existingStyle, existingImage);
                        m_wndToolBar.SetButtonStyle(focusIndex, TBBS_CHECKBOX);
                }
        }

        // -------------------------------------------------

        _resize(1024, 768);

	// -------------------------------------------------

	if (!InitializeCanvasSystem(m_wndView.m_hWnd))
	{
		::MessageBox(NULL, "Could not initialize canvas system", NULL, MB_OK );
		return -1;
	}

	// Create the primary display before we invoke the loader so that the global gPrimaryDisplay
	// is set up - so that we can make the textures with the correct pixelformat.
	m_wndView.GetClientRect(&rc);
	gPrimaryDisplay = new UIDirect3DPrimaryCanvas( UISize( rc.right, rc.bottom ), m_wndView.m_hWnd, false );
	gPrimaryDisplay->ShowShaders (g_showShaders);
	gPrimaryDisplay->Attach( 0 );

	UIManager::gUIManager().SetScriptEngine( new UIScriptEngine );


	///////////////////////////////////////////////////////////////////

	char DirBuff[_MAX_PATH + 1];
	GetCurrentDirectory( sizeof( DirBuff ), DirBuff );
	gInitialDirectory = DirBuff;
	
        // searchpaths for FileLocator - modernised initialisation

        FileLocator & loc = FileLocator::gFileLocator ();
        configureFileLocator(loc);

        ///////////////////////////////////////////////////////////////////

        m_refreshTimer = SetTimer(1, 100, 0);

        m_menuTipManager.Install(this);

        _loadModernPreferences();

        return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWnd::PreCreateWindow(cs))
	{
		return FALSE;
	}
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

//////////////////
// Calculate the size of the total frame, given a desired client (form) size.
// Start with client rect and add all the extra stuff.
//
void CMainFrame::CalcWindowRect(PRECT prcClient, UINT nAdjustType)
{
	const int desiredWidth = prcClient->right-prcClient->left;
	const int desiredHeight = prcClient->bottom-prcClient->top;

	CRect rect(0, 0, 32767, 32767);
	RepositionBars(0, 0xffff, AFX_IDW_PANE_FIRST, reposQuery,
		&rect, &rect, FALSE);

	CRect adjRect;
	adjRect.left=0;
	adjRect.top=0;
	adjRect.right=desiredWidth;
	adjRect.bottom=desiredHeight;
	::AdjustWindowRectEx(adjRect, GetStyle(), TRUE, GetExStyle());

	const int xAdjust = adjRect.Width()-desiredWidth;
	const int yAdjust = adjRect.Height()-desiredHeight;

	prcClient->right += xAdjust;
	prcClient->bottom += yAdjust + rect.Height();
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::setCursor(CursorType showType)
{
	SetCursor(m_cursors[showType]);
}

// ====================================================

void CMainFrame::setCapture()
{
	m_wndView.SetCapture();
}

// ====================================================

void CMainFrame::releaseCapture()
{
	ReleaseCapture();
}

// ====================================================

void CMainFrame::redrawViews(bool synchronous)
{
	m_wndView.redrawView(synchronous);
}

// ====================================================

void CMainFrame::copyObjectBuffersToClipboard(const std::list<SerializedObjectBuffer> &i_buffers)
{
	if (!m_hWnd || !m_clipboardFormat)
	{
		return;
	}

	// Open the clipboard, and empty it. 
	if (!OpenClipboard())
	{
		return; 
	}
	if (!EmptyClipboard())
	{
		return;
	}

	std::list<SerializedObjectBuffer>::const_iterator bi;

	//------------------------------------------------------
	// compute data size
	int size=0;
	for (bi=i_buffers.begin();bi!=i_buffers.end();++bi)
	{
		size+=4; // file size.
		size+=bi->size();
	}
	//------------------------------------------------------

	if (size)
	{
		//------------------------------------------------------
		// allocate and fill buffer
		HGLOBAL hglbCopy = GlobalAlloc(GMEM_DDESHARE, size); 
		if (hglbCopy)
		{
			char *const copy = static_cast<char *>(GlobalLock(hglbCopy));

			char *copyIter=copy;
			for (bi=i_buffers.begin();bi!=i_buffers.end();++bi)
			{
				const SerializedObjectBuffer::Buffer &buffer = bi->getBuffer();
				const int bufferSize = buffer.size();

				*reinterpret_cast<int *>(copyIter) = bufferSize;
				copyIter+=4;

				memcpy(copyIter, &buffer[0], bufferSize);
				copyIter+=bufferSize;
			}

			assert(copyIter == copy + size);

			GlobalUnlock(hglbCopy); 
			
			// Place the handle on the clipboard. 
			SetClipboardData(m_clipboardFormat, hglbCopy); 
		}
		//------------------------------------------------------
	}

	CloseClipboard(); 
}

// ====================================================

bool CMainFrame::pasteObjectBuffersFromClipboard(std::list<SerializedObjectBuffer> &o_buffers)
{
	if (!m_hWnd || !m_clipboardFormat)
	{
		return false;
	}

	if (!IsClipboardFormatAvailable(m_clipboardFormat)) 
	{
		return false;
	}

	bool success=false;

	if (OpenClipboard())
	{
		HANDLE hData = GetClipboardData(m_clipboardFormat);
		if (hData)
		{
			DWORD size = GlobalSize(hData);
			if (size)
			{
				const char *const data = static_cast<char *>(GlobalLock(hData));
				const char *const dataEnd = data + size;
				if (data)
				{
					const char *dataIter = data;

					while (dataIter<dataEnd)
					{
						int bufferSize = *reinterpret_cast<const int *>(dataIter);
						dataIter+=4;

						if (dataIter + bufferSize>dataEnd)
						{
							break;
						}

						o_buffers.push_back();
						SerializedObjectBuffer &newBuffer = o_buffers.back();
						newBuffer.setBuffer(bufferSize, dataIter);
						dataIter+=bufferSize;
					}

					success = dataIter==dataEnd && !o_buffers.empty();
					GlobalUnlock(hData);
				}
			}
		}
		CloseClipboard();
	}

	if (!success)
	{
		o_buffers.clear();
	}

	return success;
}

// ====================================================

void CMainFrame::onRootSize(int width, int height)
{
	if (width<16)
	{
		width=16;
	}
	else if (width>2048)
	{
		width=2048;
	}
	if (height<16)
	{
		height=16;
	}
	else if (height>2048)
	{
		height=2048;
	}
	_resize(width, height);
}

// ====================================================

LRESULT CMainFrame::closePropertiesDialog(WPARAM, LPARAM)
{
	if (m_propertiesDialog)
	{
		m_propertiesDialogOpen=false;
		m_propertiesDialog->ShowWindow(SW_HIDE);
	}
	return 0;
}

// ====================================================

LRESULT CMainFrame::closeObjectBrowserDialog(WPARAM, LPARAM)
{
	if (m_browserDialog)
	{
		m_browserDialogOpen=false;
		m_browserDialog->ShowWindow(SW_HIDE);
	}
	return 0;
}

// ====================================================

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_resize(int desiredWidth, int desiredHeight)
{
	CRect screenRect;
	screenRect.left=0;
	screenRect.right=desiredWidth;
	screenRect.top=0;
	screenRect.bottom=desiredHeight;
	::AdjustWindowRectEx(&screenRect, m_wndView.GetStyle(), FALSE, m_wndView.GetExStyle());

	const int viewWidth = screenRect.Width();
	const int viewHeight = screenRect.Height();

	GetWindowRect(screenRect);
	screenRect.right = screenRect.left + viewWidth;
	screenRect.bottom = screenRect.top + viewHeight;
	CalcWindowRect(screenRect, adjustBorder);
	MoveWindow(&screenRect);
}

// ====================================================

void CMainFrame::_createEditingObjects()
{
	m_defaultsManager = new DefaultObjectPropertiesManager;
	m_factory = new ObjectFactory(*m_defaultsManager);
	m_editor = new ObjectEditor(*this, *m_factory);

	m_designAssistant.install(*m_editor);

	const bool isActive = GetActiveWindow()==this;

	// -------------------------------------------------------------------------

	m_browserDialog = new ObjectBrowserDialog(*m_editor, NULL, this);
	m_browserDialog->setAcceleratorTable(m_hAccelTable);
	m_browserDialog->ShowWindow(SW_SHOW);
	m_browserDialogOpen=true;


	// -------------------------------------------------------------------------

	m_propertiesDialog = new ObjectPropertiesDialog(*m_editor, "Selection Properties");
	m_propertiesDialog->Create(
		this, 
		-1,
		WS_EX_TOOLWINDOW
		);
	m_propertiesDialog->setAcceleratorTable(m_hAccelTable);
	m_propertiesDialog->ShowWindow(SW_SHOW);
	m_propertiesDialogOpen=true;

	// -------------------------------------------------------------------------

	_setActiveAppearance(isActive);
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_destroyEditingObjects()
{
	if (m_browserDialog)
	{
		m_browserDialog->DestroyWindow();
		m_browserDialogOpen=false;
		m_browserDialog=0;
	}

	if (m_propertiesDialog)
	{
		m_propertiesDialog->destroy();
		m_propertiesDialogOpen=false;
		m_propertiesDialog=0;
	}

	if (m_editor)
	{
		m_designAssistant.remove(*m_editor);
	}

	delete m_editor;
	m_editor=0;

	delete m_factory;
	m_factory=0;

	delete m_defaultsManager;
	m_defaultsManager=0;

	m_wndStatusBar.SetPaneText(0, _T("Assistant: Ready"));
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_createNewWorkspace()
{
	// ------------------------------------------

	if (UIBuilderHistory::isInstalled())
	{
		UIBuilderHistory::remove();
	}
	UIBuilderHistory::install();

	// ------------------------------------------

	_unloadObjects();

	// ------------------------------------------

	UIPage *NewRoot = new UIPage;
	NewRoot->SetName( "root" );
	NewRoot->SetVisible( true );
	NewRoot->SetSize( UISize(800,600) );
	NewRoot->SetEnabled( true );
	UIManager::gUIManager().SetRootPage(NewRoot);

	_createEditingObjects();

	// TODO SizeWindowToCurrentPageSelection();

	m_fileName = "";
	_setMainWindowTitle();

	redrawViews(false);
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_openWorkspaceFile(const char *Filename)
{
	if (!Filename || !*Filename)
	{
		return false;
	}

	char res[_MAX_PATH+1] = "";

	if (!_closeWorkspaceFile())
	{
		return false;
	}

	strcpy( res, Filename );

	UIBuilderLoader             Loader;
	UIBaseObject::UIObjectList	 TopLevelObjects;

	char *pLastDelimitor = strrchr( res, '\\' );

	if( pLastDelimitor )
		*pLastDelimitor = '\0';

	SetCurrentDirectory( res );

	if( pLastDelimitor )
		*pLastDelimitor = '\\';

	UIBuilderLoader::s_fileTimes.clear ();

	if( !Loader.LoadFromResource( res, TopLevelObjects, true ) )
	{
		GetUIOutputStream()->flush ();
		MessageBox("The file could not be opened, check ui.log for more information", gApplicationName, MB_OK );
		return false;
	}

	if( TopLevelObjects.size() > 1 )
	{
		MessageBox("Error: The file contains more than one root level object", gApplicationName, MB_OK );
		return false;
	}
	
	UIBaseObject * const o = TopLevelObjects.front();

	UITextStyleManager::GetInstance()->Initialize(static_cast<UIPage *>(o), Loader);
	
	o->Link ();

	Loader.Lint ();

	if( o->IsA( TUIPage ) )
		UIManager::gUIManager().SetRootPage( static_cast< UIPage * >( o ) );
	else
	{
		MessageBox("Error: The root level object in the file is not a page", gApplicationName, MB_OK );
		return false;
	}

	UIBuilderHistory::install ();

//	if( Loader.ReadOnlyFilesInInput )
//		MessageBox( NULL, "Warning, one or more of the files making up this script are read only.", "UIBuilderV2", MB_OK );

	int version = 0;
	if (o->GetPropertyInteger (UIVersion::PropertyVersion, version) && version > UIVersion::ms_version)
	{		
           MessageBox("WARNING: the data you are editing is a newer version than this UIBuilderV2.", "UIBuilderV2", MB_OK | MB_ICONWARNING);
	}
	else
		o->SetPropertyInteger (UIVersion::PropertyVersion, UIVersion::ms_version);

	_createEditingObjects();
	
	m_fileName = Filename;
	theApp.AddToRecentFileList(Filename);
	_setMainWindowTitle();

	redrawViews(false);

	return true;
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_saveWorkspaceFile(const char *fileName)
{
	if (!fileName || !*fileName)
	{
		return false;
	}

	char  res[_MAX_PATH+1];
	FILE *fp = 0;

	UIPage * const RootPage = UIManager::gUIManager().GetRootPage();

	if (RootPage)
	{
		int version = 0;
		if (  RootPage->GetPropertyInteger(UIVersion::PropertyVersion, version) 
			&& version > UIVersion::ms_version
			)
		{
                   MessageBox("WARNING: the data you are editing is a newer version than this UIBuilderV2.", "UIBuilderV2", MB_OK  | MB_ICONWARNING);

			if (IDYES != MessageBox("Are you sure you want to overrite the newer data?", gApplicationName, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING ))
			{
				return false;
			}
		}
	}

	strncpy( res, fileName, sizeof(res) );
	res[sizeof(res)-1] = '\0';

	if (RootPage)
	{
		//-- force packing
		RootPage->ForcePackChildren();

		UISaver			Saver;
		typedef std::map<UINarrowString, UINarrowString> NarrowStringMap;
		NarrowStringMap Output;

		RecursiveSetProperty( RootPage, UIBaseObject::PropertyName::SourceFile, m_fileName.c_str(), res );
		Saver.SaveToStringSet( Output, Output.end(), *RootPage );

		std::vector<std::string> messages;

		std::string TmpStr;
		UIBuilderLoader Loader;

		for (NarrowStringMap::iterator i = Output.begin(); i != Output.end();)
		{
			const std::string & FileName   = i->first.empty () ? res : i->first;
			const std::string & outputData = i->second;
			
			++i;

			{
				struct _stat statbuf;
				const int result = _stat (FileName.c_str (), &statbuf);
				if (!result)
				{
					const int t = statbuf.st_mtime;
					const int oldTime = UIBuilderLoader::s_fileTimes [FileName];
					if (t > oldTime)
					{
						char buf [1024];
						_snprintf (buf, sizeof (buf), "The file [%s] has changed on disk.", FileName.c_str ());
						MessageBox(buf, gApplicationName, MB_OK | MB_ICONWARNING);
						_snprintf (buf, sizeof (buf), "Are you sure you want to save [%s]?\nThis will almost certainly clobber someone else's changes.", FileName.c_str ());
						if (IDYES != MessageBox(buf, gApplicationName, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING ))
						{
							continue;
						}
					}
				}
			}
			
			{
				TmpStr.clear ();
				Loader.LoadStringFromResource (FileName, TmpStr);
				
				//-- don't even attempt to write unmodified files
				if (TmpStr == outputData)
					continue;
			}

			FILE * fp = fopen( FileName.c_str (), "wb" );
			
			if( !fp )
			{
				messages.push_back (FileName);
			}
			else
			{
				fwrite( outputData.data (), outputData.size(), 1, fp );
				fclose( fp );
				
				struct _stat statbuf;
				const int result = _stat (FileName.c_str (), &statbuf);
				if (!result)
				{
					const int t = statbuf.st_mtime;
					UIBuilderLoader::s_fileTimes[FileName] = t;
				}
			}
		}

		if (!messages.empty ())
		{
			const std::string header ("Could not save objects to files:\n");
			int currentNumberOfFiles = 0;				

			std::string fileNames;

			for (size_t i = 0; i < messages.size ();)
			{
				const std::string & FileName = messages [i];

				++i;

				fileNames += FileName;
				fileNames.push_back ('\n');

				if (++currentNumberOfFiles >= 25 || i >= messages.size ())
				{
					const std::string msg = header + fileNames;
					MessageBox(msg.c_str (), gApplicationName, MB_OK ); 
					fileNames.clear ();
					currentNumberOfFiles = 0;
				}
			}
		}
	}	

	m_fileName = res;
	theApp.AddToRecentFileList(res);
	_setMainWindowTitle();	

	return true;
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_closeWorkspaceFile()
{	
	if (UIManager::gUIManager().GetRootPage())
	{
		switch(MessageBox("Would you like to save your workspace before closing it?", gApplicationName, MB_YESNOCANCEL | MB_ICONWARNING ) )
		{
		case IDYES:
			OnFileSave();
			break;
			
		case IDNO:
			break;
			
		case IDCANCEL:
			return false;
		}
		
		UIBuilderHistory::remove();
	}
	
	_unloadObjects();
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_unloadObjects()
{
	_destroyEditingObjects();
	UIManager::gUIManager().SetRootPage(0);
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_getSaveFileName(CString &o_filename)
{
	CFileDialog openDialog(
		FALSE,
		gDefaultExtension,
		NULL,
		OFN_EXPLORER,
		gFileFilter,
		this
	);
	openDialog.m_ofn.hInstance			= GetModuleHandle(0);
	openDialog.m_ofn.lpstrInitialDir = ".";
	openDialog.m_ofn.lpstrTitle		= "Save As...";		

	if (openDialog.DoModal()==IDOK)
	{
		o_filename = openDialog.GetPathName();
		return true;
	}
	else
	{
		return false;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_setMainWindowTitle()
{
        CString titleSegment;

        if (!m_fileName.empty())
        {
                const char *filename = strrchr(m_fileName.c_str(), '\\');
                if (!filename)
                {
                        filename = strrchr(m_fileName.c_str(), '/');
                }

                if (filename && *filename)
                {
                        ++filename;
                        titleSegment = filename;
                }
                else
                {
                        titleSegment = m_fileName.c_str();
                }
        }
        else
        {
                titleSegment = _T("Untitled");
        }

        CString composedTitle;
        composedTitle.Format(_T("UIBuilderV2 • %s"), titleSegment.GetString());

        if (m_showLayoutGuides || m_showSafeZones)
        {
                composedTitle += _T(" [");

                if (m_showLayoutGuides)
                {
                        composedTitle += _T("Guides");
                }

                if (m_showSafeZones)
                {
                        if (m_showLayoutGuides)
                        {
                                composedTitle += _T(" + ");
                        }
                        composedTitle += _T("Safe Zones");
                }

                composedTitle += _T("]");
        }

        composedTitle += _T(" — ");
        composedTitle += _getThemeDisplayName();

        SetWindowText(composedTitle);
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_setActiveAppearance(bool i_showActive)
{
        // ----------------------------------------------------------------

        ObjectBrowserDialog *browser = _getObjectBrowserDialog();
	if (browser)
	{
		browser->setActiveAppearance(i_showActive);
	}

	// ----------------------------------------------------------------

	ObjectPropertiesEditor *properties = _getObjectPropertiesDialog();
	if (properties)
	{
		properties->setActiveAppearance(i_showActive);
	}

	// ----------------------------------------------------------------

	if (i_showActive!=m_showActive)
	{
		m_showActive=i_showActive;
                PostMessage(WM_NCACTIVATE, m_showActive);
        }
}

/////////////////////////////////////////////////////////////////////////////

CString CMainFrame::_getThemeDisplayName() const
{
        switch (m_activeTheme)
        {
        case ThemeStyleNeoDark:
                return _T("Obsidian Pulse");
        case ThemeStyleLuminousLight:
                return _T("Glacier Glass");
        case ThemeStyleBlueprint:
                return _T("Hologrid Indigo");
        case ThemeStyleAurora:
                return _T("Verdant Bloom");
        case ThemeStyleSolar:
                return _T("Amber Drift");
        case ThemeStyleCarbon:
                return _T("Synthwave Vapor");
        case ThemeStyleNordic:
                return _T("Arctic Mist");
        default:
                break;
        }

        return _T("Custom");
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_applyTheme(ThemeStyle style, bool persist)
{
	m_activeTheme = style;

	switch (style)
	{
	case ThemeStyleNeoDark:
		m_canvasBackgroundColor.Set(14, 18, 32, 255);
		gGridColor.Set(120, 90, 255, 56);
		m_layoutGuideColor.Set(0, 255, 214, 210);
		m_safeZoneColor.Set(255, 115, 34, 48);
		m_focusOverlayColor.Set(4, 6, 18, 185);
		m_highlightOutlineColor.Set(0, 255, 214, 255);
		m_selectionOutlineColor.Set(120, 214, 255, 255);
		m_highlightSelectionOutlineColor.Set(255, 82, 167, 255);
		m_selectionFillColor.Set(120, 214, 255, 36);
		m_selectionBoxOutlineColor.Set(120, 214, 255, 210);
		m_selectionDragBoxOutlineColor.Set(255, 82, 167, 255);
		break;
	case ThemeStyleLuminousLight:
		m_canvasBackgroundColor.Set(242, 246, 252, 255);
		gGridColor.Set(90, 140, 180, 56);
		m_layoutGuideColor.Set(46, 160, 255, 200);
		m_safeZoneColor.Set(66, 202, 255, 40);
		m_focusOverlayColor.Set(255, 255, 255, 150);
		m_highlightOutlineColor.Set(24, 126, 214, 255);
		m_selectionOutlineColor.Set(45, 56, 78, 255);
		m_highlightSelectionOutlineColor.Set(242, 140, 19, 255);
		m_selectionFillColor.Set(45, 56, 78, 28);
		m_selectionBoxOutlineColor.Set(88, 170, 255, 190);
		m_selectionDragBoxOutlineColor.Set(24, 126, 214, 255);
		break;
	case ThemeStyleBlueprint:
		m_canvasBackgroundColor.Set(18, 10, 40, 255);
		gGridColor.Set(160, 120, 255, 80);
		m_layoutGuideColor.Set(255, 255, 255, 200);
		m_safeZoneColor.Set(120, 70, 255, 52);
		m_focusOverlayColor.Set(12, 6, 26, 170);
		m_highlightOutlineColor.Set(255, 255, 255, 255);
		m_selectionOutlineColor.Set(90, 255, 220, 255);
		m_highlightSelectionOutlineColor.Set(255, 255, 255, 255);
		m_selectionFillColor.Set(90, 255, 220, 38);
		m_selectionBoxOutlineColor.Set(120, 255, 230, 210);
		m_selectionDragBoxOutlineColor.Set(255, 255, 255, 255);
		break;
	case ThemeStyleAurora:
		m_canvasBackgroundColor.Set(24, 26, 12, 255);
		gGridColor.Set(120, 180, 60, 70);
		m_layoutGuideColor.Set(196, 255, 140, 210);
		m_safeZoneColor.Set(255, 196, 64, 44);
		m_focusOverlayColor.Set(0, 0, 0, 170);
		m_highlightOutlineColor.Set(120, 255, 140, 255);
		m_selectionOutlineColor.Set(188, 240, 120, 255);
		m_highlightSelectionOutlineColor.Set(255, 230, 120, 255);
		m_selectionFillColor.Set(188, 240, 120, 40);
		m_selectionBoxOutlineColor.Set(210, 245, 150, 205);
		m_selectionDragBoxOutlineColor.Set(255, 210, 88, 255);
		break;
	case ThemeStyleSolar:
		m_canvasBackgroundColor.Set(30, 22, 14, 255);
		gGridColor.Set(255, 170, 80, 72);
		m_layoutGuideColor.Set(255, 206, 128, 210);
		m_safeZoneColor.Set(255, 132, 64, 48);
		m_focusOverlayColor.Set(12, 6, 0, 180);
		m_highlightOutlineColor.Set(255, 214, 150, 255);
		m_selectionOutlineColor.Set(255, 186, 96, 255);
		m_highlightSelectionOutlineColor.Set(255, 234, 190, 255);
		m_selectionFillColor.Set(255, 186, 96, 44);
		m_selectionBoxOutlineColor.Set(255, 204, 144, 210);
		m_selectionDragBoxOutlineColor.Set(255, 142, 82, 255);
		break;
	case ThemeStyleCarbon:
		m_canvasBackgroundColor.Set(18, 16, 34, 255);
		gGridColor.Set(120, 110, 150, 76);
		m_layoutGuideColor.Set(255, 109, 212, 215);
		m_safeZoneColor.Set(72, 206, 255, 50);
		m_focusOverlayColor.Set(6, 4, 18, 190);
		m_highlightOutlineColor.Set(255, 255, 255, 255);
		m_selectionOutlineColor.Set(120, 240, 255, 255);
		m_highlightSelectionOutlineColor.Set(255, 255, 255, 255);
		m_selectionFillColor.Set(120, 240, 255, 38);
		m_selectionBoxOutlineColor.Set(140, 248, 255, 215);
		m_selectionDragBoxOutlineColor.Set(255, 109, 212, 255);
		break;
	case ThemeStyleNordic:
		m_canvasBackgroundColor.Set(234, 240, 244, 255);
		gGridColor.Set(120, 176, 206, 68);
		m_layoutGuideColor.Set(54, 146, 196, 220);
		m_safeZoneColor.Set(122, 210, 255, 46);
		m_focusOverlayColor.Set(255, 255, 255, 150);
		m_highlightOutlineColor.Set(0, 118, 154, 255);
		m_selectionOutlineColor.Set(46, 82, 118, 255);
		m_highlightSelectionOutlineColor.Set(94, 178, 212, 255);
		m_selectionFillColor.Set(46, 82, 118, 30);
		m_selectionBoxOutlineColor.Set(94, 178, 212, 210);
		m_selectionDragBoxOutlineColor.Set(0, 170, 210, 255);
		break;
	default:
		break;
	}

	if (persist)
	{
		theApp.WriteProfileInt(sAppearanceSection, sThemeValueName, static_cast<int>(style));
	}

	if (gPrimaryDisplay)
	{
		redrawViews(false);
	}

	_setMainWindowTitle();
}

void CMainFrame::_loadModernPreferences()
{
        const int storedTheme = theApp.GetProfileInt(sAppearanceSection, sThemeValueName, static_cast<int>(ThemeStyleNeoDark));

        ThemeStyle themeToApply = ThemeStyleNeoDark;
        if (storedTheme >= ThemeStyleNeoDark && storedTheme <= ThemeStyleNordic)
        {
                themeToApply = static_cast<ThemeStyle>(storedTheme);
        }

        _applyTheme(themeToApply, false);

        m_showLayoutGuides = theApp.GetProfileInt(sOverlaySection, sLayoutGuidesValueName, 1) != 0;
        m_showSafeZones = theApp.GetProfileInt(sOverlaySection, sSafeZonesValueName, 0) != 0;
        m_showStatusMetrics = theApp.GetProfileInt(sWorkspaceSection, sStatusMetricsValueName, 1) != 0;
        m_showAssistantOverlay = theApp.GetProfileInt(sWorkspaceSection, sAssistantValueName, 1) != 0;
        m_focusMode = theApp.GetProfileInt(sWorkspaceSection, sFocusModeValueName, 0) != 0;

        const int defaultPad = static_cast<int>(m_safeZoneHorizontalPadding * static_cast<float>(s_safeZonePaddingScale));
        const int storedPadX = theApp.GetProfileInt(sOverlaySection, sSafeZonePadXValueName, defaultPad);
        const int storedPadY = theApp.GetProfileInt(sOverlaySection, sSafeZonePadYValueName, defaultPad);

        m_safeZoneHorizontalPadding = static_cast<float>(std::max(0, storedPadX)) / static_cast<float>(s_safeZonePaddingScale);
        m_safeZoneVerticalPadding = static_cast<float>(std::max(0, storedPadY)) / static_cast<float>(s_safeZonePaddingScale);

        _setMainWindowTitle();
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_drawSafeZonesOverlay(long viewportWidth, long viewportHeight)
{
        if (!gPrimaryDisplay || viewportWidth <= 0 || viewportHeight <= 0)
        {
                return;
        }

        const long horizontalMargin = static_cast<long>(viewportWidth * m_safeZoneHorizontalPadding);
        const long verticalMargin = static_cast<long>(viewportHeight * m_safeZoneVerticalPadding);

        if (horizontalMargin * 2 >= viewportWidth || verticalMargin * 2 >= viewportHeight)
        {
                return;
        }

        const float overlayOpacity = m_safeZoneColor.a / 255.0f;
        gPrimaryDisplay->SetOpacity(overlayOpacity * 0.4f);

        gPrimaryDisplay->ClearTo(m_safeZoneColor, UIRect(0, 0, viewportWidth, verticalMargin));
        gPrimaryDisplay->ClearTo(m_safeZoneColor, UIRect(0, viewportHeight - verticalMargin, viewportWidth, viewportHeight));
        gPrimaryDisplay->ClearTo(m_safeZoneColor, UIRect(0, verticalMargin, horizontalMargin, viewportHeight - verticalMargin));
        gPrimaryDisplay->ClearTo(m_safeZoneColor, UIRect(viewportWidth - horizontalMargin, verticalMargin, viewportWidth, viewportHeight - verticalMargin));

        const UIRect safeRect(horizontalMargin, verticalMargin, viewportWidth - horizontalMargin, viewportHeight - verticalMargin);

        gPrimaryDisplay->SetOpacity(overlayOpacity);
        _drawBoxOutline(safeRect, m_safeZoneColor);
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_drawLayoutGuidesOverlay(const UIRect &bounds, const UIPoint &canvasTranslation, long canvasWidth, long canvasHeight)
{
        if (!gPrimaryDisplay)
        {
                return;
        }

        if (bounds.Width() <= 0 || bounds.Height() <= 0)
        {
                return;
        }

        const float guideOpacity = m_layoutGuideColor.a / 255.0f;
        gPrimaryDisplay->SetOpacity(guideOpacity);

        const long centerX = (bounds.left + bounds.right) / 2;
        const long centerY = (bounds.top + bounds.bottom) / 2;

        gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(centerX, canvasTranslation.y, centerX + 1, canvasTranslation.y + canvasHeight));
        gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(canvasTranslation.x, centerY, canvasTranslation.x + canvasWidth, centerY + 1));

        const long thirdWidth = bounds.Width() / 3;
        const long thirdHeight = bounds.Height() / 3;

        if (thirdWidth > 0)
        {
                const long thirdX1 = bounds.left + thirdWidth;
                const long thirdX2 = bounds.right - thirdWidth;
                gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(thirdX1, bounds.top, thirdX1 + 1, bounds.bottom));
                gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(thirdX2, bounds.top, thirdX2 + 1, bounds.bottom));
        }

        if (thirdHeight > 0)
        {
                const long thirdY1 = bounds.top + thirdHeight;
                const long thirdY2 = bounds.bottom - thirdHeight;
                gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(bounds.left, thirdY1, bounds.right, thirdY1 + 1));
                gPrimaryDisplay->ClearTo(m_layoutGuideColor, UIRect(bounds.left, thirdY2, bounds.right, thirdY2 + 1));
        }

        _drawBoxOutline(bounds, m_layoutGuideColor);
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_renderFocusModeOverlay(const UIRect &selectionBounds, bool hasSelection, const UIPoint &canvasTranslation, long canvasWidth, long canvasHeight)
{
        if (!m_focusMode || !gPrimaryDisplay)
        {
                return;
        }

        const UIRect overlayRect(canvasTranslation.x, canvasTranslation.y, canvasTranslation.x + canvasWidth, canvasTranslation.y + canvasHeight);
        const float overlayOpacity = static_cast<float>(m_focusOverlayColor.a) / 255.0f;

        gPrimaryDisplay->SetOpacity(overlayOpacity);
        gPrimaryDisplay->ClearTo(m_focusOverlayColor, overlayRect);

        if (hasSelection)
        {
                UIColor spotlight = m_focusOverlayColor;
                spotlight.a = static_cast<unsigned char>(std::max(0, static_cast<int>(m_focusOverlayColor.a) - 90));

                const float spotlightOpacity = static_cast<float>(spotlight.a) / 255.0f;
                gPrimaryDisplay->SetOpacity(spotlightOpacity);
                gPrimaryDisplay->ClearTo(spotlight, selectionBounds);

                gPrimaryDisplay->SetOpacity(1.0f);
                _drawBoxOutline(selectionBounds, m_highlightSelectionOutlineColor);
        }

        gPrimaryDisplay->SetOpacity(1.0f);
}

/////////////////////////////////////////////////////////////////////////////

CString CMainFrame::_buildStatusMetrics()
{
        CString result;
        result.Format(_T("Theme: %s"), _getThemeDisplayName().GetString());

        if (m_editor)
        {
                result.AppendFormat(_T("  |  Grid: %s"), m_editor->getSnapToGrid() ? _T("Snap") : _T("Free"));

                const int selectionCount = m_editor->getSelectionCount();
                if (selectionCount > 0)
                {
                        UIRect selectionBounds;
                        if (m_editor->getSelectionBox(selectionBounds))
                        {
                                result.AppendFormat(_T("  |  Selection: %ld×%ld (%d)"), selectionBounds.Width(), selectionBounds.Height(), selectionCount);
                        }
                        else
                        {
                                result.AppendFormat(_T("  |  Selection: %d"), selectionCount);
                        }
                }
        }

        if (m_showLayoutGuides)
        {
                result.Append(_T("  |  Guides"));
        }

        if (m_showSafeZones)
        {
                result.Append(_T("  |  Safe Zones"));
        }

        result.AppendFormat(_T("  |  Assistant: %s"), m_showAssistantOverlay ? _T("On") : _T("Hidden"));
        result.AppendFormat(_T("  |  Focus: %s"), m_focusMode ? _T("Active") : _T("Off"));

        if (m_designAssistant.hasAutoLayoutRecommendation())
        {
                result.AppendFormat(_T("  |  Suggested Grid %d x %d"),
                        m_designAssistant.getRecommendedLayoutColumns(),
                        m_designAssistant.getRecommendedLayoutRows());
        }

        const float densityScore = m_designAssistant.getLayoutDensityScore();
        if (densityScore > 0.0f)
        {
                result.AppendFormat(_T("  |  Density %d%%"), static_cast<int>(densityScore * 100.0f + 0.5f));

                if (m_designAssistant.hasLayoutDensityWarning())
                {
                        result.Append(_T(" (tight)"));
                }
        }

        return result;
}

/////////////////////////////////////////////////////////////////////////////

ObjectBrowserDialog *CMainFrame::_getObjectBrowserDialog()
{
        if (!isEditing())
        {
		return 0;
	}
	return m_browserDialog;
}

/////////////////////////////////////////////////////////////////////////////

CMainFrame::PropertiesDialog *CMainFrame::_getObjectPropertiesDialog()
{
	if (!isEditing())
	{
		return 0;
	}
	return m_propertiesDialog;
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_objectBrowserIsFocus()
{
	ObjectBrowserDialog *browser = _getObjectBrowserDialog();

	for (CWnd *f = GetFocus();f;f = f->GetParent())
	{
		if (f==browser)
		{
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

bool CMainFrame::_objectPropertiesIsFocus()
{
	PropertiesDialog *properties = _getObjectPropertiesDialog();

	for (CWnd *f = GetFocus();f;f = f->GetParent())
	{
		if (f==properties)
		{
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_renderUI()
{
	if (!m_editor)
	{
		return;
	}

	UIManager &GUIManager = UIManager::gUIManager();

	if (!m_inVisualEditingMode)
	{
		GUIManager.SendHeartbeats();
	}

	UIPage *RootPage = GUIManager.GetRootPage();
	if (!RootPage)
	{
		return;
	}

	m_designAssistant.update(*m_editor, *RootPage);

	if (!gPrimaryDisplay->BeginRendering())
	{
		return;
	}

	UIPoint CanvasTranslation(0,0);
	
        gPrimaryDisplay->ClearTo(m_canvasBackgroundColor, UIRect(0, 0, gPrimaryDisplay->GetWidth(), gPrimaryDisplay->GetHeight()));
        gPrimaryDisplay->PushState();

        UIPage *currentlySelectedPage = m_editor->getCurrentlyVisiblePage();
        if (currentlySelectedPage)
        {
                CanvasTranslation = -currentlySelectedPage->GetLocation();
                gPrimaryDisplay->Translate(CanvasTranslation);
        }

        GUIManager.DrawCursor(gDrawCursor);
        GUIManager.Render(*gPrimaryDisplay);

        if (m_showSafeZones)
        {
                gPrimaryDisplay->PushState();
                gPrimaryDisplay->Translate(-CanvasTranslation);
                _drawSafeZonesOverlay(gPrimaryDisplay->GetWidth(), gPrimaryDisplay->GetHeight());
                gPrimaryDisplay->PopState();
        }

        if (m_editor->getSnapToGrid())
        {
		int x;
		int y;
		int linecount;
		int width  = RootPage->GetWidth();
		int height = RootPage->GetHeight();
		
		float LineOpacity = gGridColor.a / 255.0f;
		float ThickLineOpacity = 2.0f * LineOpacity;
		
		gPrimaryDisplay->SetOpacity( LineOpacity );

		UIPoint gridSteps = m_editor->getGridSteps();
		
		for( x = 0, linecount = 0; x <= width; x += gridSteps.x, ++linecount )
		{
			if( (linecount % gGridMajorTicks) == 0 )
			{
				gPrimaryDisplay->SetOpacity( ThickLineOpacity );
				gPrimaryDisplay->ClearTo( gGridColor, UIRect( x, CanvasTranslation.y, x+1, height ) );
				gPrimaryDisplay->SetOpacity( LineOpacity );
			}
			else
				gPrimaryDisplay->ClearTo( gGridColor, UIRect( x, CanvasTranslation.y, x+1, height ) );
		}
		
		for( y = 0, linecount = 0; y <= height; y += gridSteps.y, ++linecount )
		{
			if( (linecount % gGridMajorTicks) == 0 )
			{
				gPrimaryDisplay->SetOpacity( ThickLineOpacity );
				gPrimaryDisplay->ClearTo( gGridColor, UIRect( CanvasTranslation.x, y, width, y+1 ) );
				gPrimaryDisplay->SetOpacity( LineOpacity );
			}
			else
				gPrimaryDisplay->ClearTo( gGridColor, UIRect( CanvasTranslation.x, y, width, y+1 ) );
		}
	}
	
        UIRect selectionBox;
        const bool hasSelectionBox = m_editor->getSelectionBox(selectionBox);

        if (m_drawSelectionRect)
        {
                const UIObjectSet &sels = m_editor->getSelections();
                const UIObjectSet &hilt = m_editor->getHighlights();

		UIObjectSet::const_iterator osi;
		for (osi = sels.begin(); osi != sels.end(); ++osi)
		{
			UIBaseObject *const o = *osi;
			if (!o->IsA(TUIWidget))
			{
				continue;
			}
			const UIWidget * w = UI_ASOBJECT(UIWidget, o);

			UIRect WidgetRect;
			w->GetWorldRect(WidgetRect);

			const bool isSelectionHighlighted = hilt.contains(o);

			UIColor selectionFillColor    = m_selectionFillColor;
			UIColor selectionOutlineColor = (isSelectionHighlighted) ? m_highlightSelectionOutlineColor : m_selectionOutlineColor;

			// ------------------------------------------------------------------

			if (m_selectionFillColor.a>0)
			{
				gPrimaryDisplay->SetOpacity(selectionFillColor.a / 255.0f);
				gPrimaryDisplay->ClearTo(selectionFillColor, WidgetRect);
			}
			
			gPrimaryDisplay->SetOpacity(selectionOutlineColor.a / 255.0f);

			_drawBoxOutline(WidgetRect, selectionOutlineColor);

			// ------------------------------------------------------------------
		}

		for (osi = hilt.begin(); osi != hilt.end(); ++osi)
		{
			UIBaseObject *const o = *osi;
			if (!o->IsA(TUIWidget))
			{
				continue;
			}
			const UIWidget * w = UI_ASOBJECT(UIWidget, o);

			UIRect WidgetRect;
			w->GetWorldRect(WidgetRect);

			const bool isSelected = sels.contains(o);
			if (!isSelected)
			{
				gPrimaryDisplay->SetOpacity(m_highlightOutlineColor.a / 255.0f);
				_drawBoxOutline(WidgetRect, m_highlightOutlineColor);
			}
		}

		// ----------------------------------------------------
                if (hasSelectionBox)
                {
                        gPrimaryDisplay->SetOpacity(m_selectionBoxOutlineColor.a / 255.0f);
                        _drawBoxOutline(selectionBox, m_selectionBoxOutlineColor);
                }
		// ----------------------------------------------------

		UIRect selectionDragBox;
		if (m_editor->getSelectionDragBox(selectionDragBox))
		{
			const UIColor selectionDragBoxOutlineColor = m_selectionDragBoxOutlineColor;

			gPrimaryDisplay->SetOpacity(m_selectionDragBoxOutlineColor.a / 255.0f);

			_drawBoxOutline(selectionDragBox, m_selectionDragBoxOutlineColor);

			if (  m_inVisualEditingMode 
				&& selectionDragBox.Height() >= 8
				&& selectionDragBox.Width()  >= 8
				)
			{
				gPrimaryDisplay->ClearTo(
					selectionDragBoxOutlineColor, 
					UIRect(
						selectionDragBox.left + LineWidth,
						selectionDragBox.top  + LineWidth,
						selectionDragBox.left + HandleSize,
						selectionDragBox.top  + HandleSize
					) 
				);
				
				gPrimaryDisplay->ClearTo(
					selectionDragBoxOutlineColor,
					UIRect(
						selectionDragBox.right - HandleSize,
						selectionDragBox.top   + LineWidth,
						selectionDragBox.right - LineWidth,
						selectionDragBox.top   + HandleSize
					)
				);
				
				gPrimaryDisplay->ClearTo(
					selectionDragBoxOutlineColor,
					UIRect(
						selectionDragBox.left   + LineWidth,
						selectionDragBox.bottom - HandleSize,
						selectionDragBox.left   + HandleSize,
						selectionDragBox.bottom - LineWidth
					)
				);
				
				gPrimaryDisplay->ClearTo(
					selectionDragBoxOutlineColor,
					UIRect(
						selectionDragBox.right  - HandleSize,
						selectionDragBox.bottom - HandleSize,
						selectionDragBox.right  - LineWidth,
						selectionDragBox.bottom - LineWidth
					)
				);
				
				if (  selectionDragBox.Height()>=16 
					&& selectionDragBox.Width() >=16
					)
				{
					long HHandleLoc, VHandleLoc;
					
					HHandleLoc = (selectionDragBox.bottom + selectionDragBox.top - HandleSize) / 2;
					VHandleLoc = (selectionDragBox.right + selectionDragBox.left - HandleSize) / 2;
					
					gPrimaryDisplay->ClearTo(
						selectionDragBoxOutlineColor, 
						UIRect(
							selectionDragBox.left + LineWidth,
							HHandleLoc,
							selectionDragBox.left + HandleSize,
							HHandleLoc         + HandleSize
						)
					);
					
					gPrimaryDisplay->ClearTo(
						selectionDragBoxOutlineColor,
						UIRect(
							selectionDragBox.right - HandleSize,
							HHandleLoc,
							selectionDragBox.right - LineWidth,
							HHandleLoc          + HandleSize
						)
					);
					
					gPrimaryDisplay->ClearTo(
						selectionDragBoxOutlineColor,
						UIRect(
							VHandleLoc, 
							selectionDragBox.top + LineWidth,
							VHandleLoc        + HandleSize,
							selectionDragBox.top + HandleSize
						)
					);
					
					gPrimaryDisplay->ClearTo(
						selectionDragBoxOutlineColor,
						UIRect(
							VHandleLoc,
							selectionDragBox.bottom - HandleSize,
							VHandleLoc + HandleSize,
							selectionDragBox.bottom - LineWidth
						)
					);
				}
			}

		}
        }

        if (m_showLayoutGuides && hasSelectionBox)
        {
                _drawLayoutGuidesOverlay(selectionBox, CanvasTranslation, RootPage->GetWidth(), RootPage->GetHeight());
        }

        if (m_focusMode)
        {
                _renderFocusModeOverlay(selectionBox, hasSelectionBox, CanvasTranslation, RootPage->GetWidth(), RootPage->GetHeight());
        }

        if (m_showAssistantOverlay)
        {
                m_designAssistant.render(*gPrimaryDisplay);
        }

        gPrimaryDisplay->PopState();
        gPrimaryDisplay->EndRendering();
        gPrimaryDisplay->Flip();

        gTriangleCount = gPrimaryDisplay->GetTriangleCount();
        gFlushCount = gPrimaryDisplay->GetFlushCount();

        ++gFrameCount;

        CString assistantText(m_designAssistant.getStatusText().c_str());

        if (!m_showAssistantOverlay)
        {
                if (assistantText.IsEmpty())
                {
                        assistantText = _T("Assistant paused – press Ctrl+Shift+A to resume insights.");
                }
                else
                {
                        assistantText = _T("Assistant paused – press Ctrl+Shift+A to resume insights.   |   ") + assistantText;
                }
        }

        if (m_showStatusMetrics)
        {
                CString metrics = _buildStatusMetrics();
                if (!metrics.IsEmpty())
                {
                        if (!assistantText.IsEmpty())
                        {
                                assistantText += _T("   |   ");
                        }
                        assistantText += metrics;
                }
        }

        m_wndStatusBar.SetPaneText(0, assistantText);
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::_drawBoxOutline(UIRect box, UIColor color)
{
	gPrimaryDisplay->ClearTo(
		color,
		UIRect(
			box.left,
			box.top, 
			box.left + LineWidth, 
			box.bottom
		)
	);
	
	gPrimaryDisplay->ClearTo(
		color,
		UIRect(
			box.right - LineWidth,
			box.top,
			box.right,
			box.bottom
		)
	);
	
	gPrimaryDisplay->ClearTo(
		color,
		UIRect(
			box.left  + LineWidth,
			box.top,
			box.right - LineWidth,
			box.top   + LineWidth
		)
	);
	
	gPrimaryDisplay->ClearTo(
		color,
		UIRect(
			box.left   + LineWidth,
			box.bottom - LineWidth,
			box.right  - LineWidth,
			box.bottom
		)
	);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
	// forward focus to the view window
	m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}


void CMainFrame::OnDestroy() 
{
	if (m_refreshTimer)
	{
		KillTimer(m_refreshTimer);
		m_refreshTimer=0;
	}

	_destroyEditingObjects();

	if (gPrimaryDisplay)
	{
		// We better have closed all references to the display
//		if( gPrimaryDisplay->GetRefCount() != 1 )
//			MessageBox(0, "There is a handle leak: not all references to the primary display were closed.", gApplicationName, MB_OK );

		gPrimaryDisplay->Detach();
		gPrimaryDisplay = 0;
	}

	ShutdownCanvasSystem(m_wndView.m_hWnd);
	
	delete UIManager::gUIManager().GetScriptEngine();
	UIManager::gUIManager().SetScriptEngine(0);

	// --------------------------------------------

	SetCurrentDirectory( gInitialDirectory.c_str() );

	// --------------------------------------------

	CFrameWnd::OnDestroy();
}

void CMainFrame::OnFileNew() 
{
	_createNewWorkspace();
}

void CMainFrame::OnUpdateFileNew(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
}

LRESULT CMainFrame::OnPaintChild(WPARAM, LPARAM)
{
	_renderUI();

	return 0;
}

void CMainFrame::OnFileOpen() 
{
	CFileDialog openDialog(
		TRUE,
		gDefaultExtension,
		NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		gFileFilter,
		this
	);
	openDialog.m_ofn.lpstrInitialDir = ".";
	openDialog.m_ofn.lpstrTitle		= "Select a User Interface Workspace to Open";

	if (openDialog.DoModal()==IDOK)
	{
		CString fileName = openDialog.GetPathName();
		_openWorkspaceFile(fileName);
	}
}

void CMainFrame::OnUpdateFileOpen(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
}

void CMainFrame::OnFileClose() 
{
	_closeWorkspaceFile();
}

void CMainFrame::OnUpdateFileClose(CCmdUI* pCmdUI) 
{
	bool isLoaded = UIManager::gUIManager().GetRootPage()!=0;
	pCmdUI->Enable(isLoaded);
}

void CMainFrame::OnFileSave() 
{
	if (isEditing())
	{
		if (m_fileName.empty())
		{
			OnFileSaveas();
		}
		else
		{
			_saveWorkspaceFile(m_fileName.c_str());
		}
	}
}

void CMainFrame::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnFileSaveas() 
{
	if (isEditing())
	{
		CString fileName;

		if (_getSaveFileName(fileName))
		{
			_saveWorkspaceFile(fileName);
		}
	}
}

void CMainFrame::OnUpdateFileSaveas(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	CFrameWnd::OnSize(nType, cx, cy);
	
	if (gPrimaryDisplay)
	{
		gPrimaryDisplay->SetSize(UISize(cx, cy));
	}

	InvalidateRect(0, false);
}

void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
	if (_closeWorkspaceFile())
	{
		char Filename[_MAX_PATH + 1];

		DragQueryFile(hDropInfo, 0, Filename, sizeof(Filename));
		_openWorkspaceFile(Filename);
	}
}

void CMainFrame::OnEditCopy() 
{
	if (m_editor)
	{
		m_editor->copySelectionsToClipboard(false);
	}
}

void CMainFrame::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	const bool canCopy=m_editor && m_editor->hasSelections();
	pCmdUI->Enable(canCopy);
}

void CMainFrame::OnEditCut() 
{
	if (m_editor)
	{
		m_editor->copySelectionsToClipboard(true);
	}
}

void CMainFrame::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	const bool canCopy=m_editor && m_editor->hasSelections();
	pCmdUI->Enable(canCopy);
}

void CMainFrame::OnEditPaste() 
{
	if (m_editor)
	{
		m_editor->pasteFromClipboard();
	}
}

void CMainFrame::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	const bool canPaste=m_editor && m_clipboardFormat && IsClipboardFormatAvailable(m_clipboardFormat)!=0;
	pCmdUI->Enable(canPaste);
}

void CMainFrame::OnInsertObject(UINT nId)
{
	if (isEditing())
	{
		m_editor->insertNewObject(nId);
	}
}

void CMainFrame::OnUpdateInsertObject(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnViewDefaultProperties() 
{
	// TODO: Add your command handler code here
	DefaultObjectPropertiesDialog dlg(*m_defaultsManager);
	dlg.DoModal();
}

void CMainFrame::OnUpdateViewDefaultProperties(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_defaultsManager!=0);
}

void CMainFrame::OnTimer(UINT nIDEvent) 
{
	if (  nIDEvent==m_refreshTimer
		&& isEditing()
		&& IsWindowVisible()
		&& !IsIconic()
		)
	{
		redrawViews(false);
	}
	
	CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnClose() 
{
	if (m_editor)
	{
		if (m_browserDialog)
		{
			m_browserDialog->saveUserPreferences();
			m_browserDialog->DestroyWindow();
			m_browserDialog=0;
			m_browserDialogOpen=false;
		}

		if (m_propertiesDialog)
		{
			m_propertiesDialog->saveUserPreferences();
			m_propertiesDialog->DestroyWindow();
			m_propertiesDialog=0;
			m_propertiesDialogOpen=false;
		}
	}
	
	CFrameWnd::OnClose();
}

void CMainFrame::OnSelectionBurrow() 
{
	if (m_editor)
	{
		m_editor->burrow();
	}
}

void CMainFrame::OnUpdateSelectionBurrow(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnSelectionClearall() 
{
	if (m_editor)
	{
		m_editor->clearSelections();
	}
}

void CMainFrame::OnUpdateSelectionClearall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnSelectionDescendants() 
{
	if (m_editor)
	{
		m_editor->selectDescendants();
	}
}

void CMainFrame::OnUpdateSelectionDescendants(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnSelectionAncestors() 
{
	if (m_editor)
	{
		m_editor->selectAncestors();
	}
}

void CMainFrame::OnUpdateSelectionAncestors(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnEditCanceldrag() 
{
	if (m_editor)
	{
		m_editor->cancelDrag();
	}
}

void CMainFrame::OnUpdateEditCanceldrag(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
}

void CMainFrame::OnSelectionDelete() 
{
	if (m_editor)
	{
		UIBaseObject *nextSelection=0;
		if (  m_editor->getSelectionCount()==1
			&& _objectBrowserIsFocus()
			)
		{
			UIBaseObject *sel = m_editor->getSelections().front();
			UIBaseObject *selParent = sel->GetParent();
			if (selParent)
			{
				UIBaseObject::UIObjectList children;
				EditUtils::getChildren(children, *selParent);
				for (UIBaseObject::UIObjectList::iterator ci=children.begin();ci!=children.end();++ci)
				{
					if (*ci == sel)
					{
						UIBaseObject::UIObjectList::iterator ciNext=ci;
						++ciNext;
						if (ciNext!=children.end())
						{
							nextSelection=*ciNext;
						}
						else if (ci!=children.begin())
						{
							--ci;
							nextSelection=*ci;
						}
						else
						{
							nextSelection=selParent;
						}
						break;
					}
				}
			}
			assert(nextSelection!=sel);
		}

		if (m_editor->deleteSelections())
		{
			if (nextSelection)
			{
				m_editor->select(*nextSelection);
			}
		}
	}
}

void CMainFrame::OnUpdateSelectionDelete(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->hasSelections());
}

void CMainFrame::OnActivateApp(BOOL bActive, HTASK hTask) 
{
	CFrameWnd::OnActivateApp(bActive, hTask);

	_setActiveAppearance(bActive!=0);

	if (!bActive)
	{
		if (m_editor)
		{
			m_editor->onDeactivate();
		}
	}
}

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	switch (message)
	{
		case WM_NCACTIVATE:
			wParam=m_showActive;
			break;
	}
	return CFrameWnd::WindowProc(message, wParam, lParam);
}

void CMainFrame::OnToggleGrid() 
{
	if (isEditing())
	{
		m_editor->setSnapToGrid(!m_editor->getSnapToGrid());
	}
}

void CMainFrame::OnUpdateToggleGrid(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing());
	if (isEditing())
	{
		pCmdUI->SetCheck(m_editor->getSnapToGrid() ? 1 : 0);
	}
	else
	{
		pCmdUI->SetCheck(2);
	}
}

void CMainFrame::OnSelectionAlignbottom() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_BOTTOM);
	}
}

void CMainFrame::OnUpdateSelectionAlignbottom(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
	
}

void CMainFrame::OnSelectionAlignleft() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_LEFT);
	}
}

void CMainFrame::OnUpdateSelectionAlignleft(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAlignright() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_RIGHT);
	}
}

void CMainFrame::OnUpdateSelectionAlignright(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAligntop() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_TOP);
	}
}

void CMainFrame::OnUpdateSelectionAligntop(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAlignwidth() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_WIDTH);
	}
}

void CMainFrame::OnUpdateSelectionAlignwidth(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAlignheight() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_HEIGHT);
	}
}

void CMainFrame::OnUpdateSelectionAlignheight(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAligncenterx() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_CENTERX);
	}
}

void CMainFrame::OnUpdateSelectionAligncenterx(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionAligncentery() 
{
	if (m_editor)
	{
		m_editor->alignSelections(ObjectEditor::AD_CENTERY);
	}
}

void CMainFrame::OnUpdateSelectionAligncentery(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>1);
}

void CMainFrame::OnSelectionSmartupscale()
{
	if (m_editor && m_editor->upscaleSelectionsSmart())
	{
		SetMessageText(_T("Smart upscale applied"));
	}
}

void CMainFrame::OnUpdateSelectionSmartupscale(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>0);
}

void CMainFrame::OnSelectionAutoupscale()
{
	if (m_editor && m_editor->upscaleSelectionsAuto())
	{
		SetMessageText(_T("Auto upscale applied"));
	}
}

void CMainFrame::OnUpdateSelectionAutoupscale(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>0);
}

void CMainFrame::OnEditUndo() 
{
	if (m_editor)
	{
		m_editor->undo();
	}
}

void CMainFrame::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	const char *undoName=0;
	if (m_editor)
	{
		undoName = m_editor->getCurrentUndoOperation();
	}

	if (undoName)
	{
		char temp[256];
		sprintf(temp, "Undo %s \tCtrl-Z", undoName);

		pCmdUI->Enable(TRUE);
		pCmdUI->SetText(temp);
	}
	else
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetText("Undo \tCtrl-Z");
	}
}

void CMainFrame::OnEditRedo() 
{
	if (m_editor)
	{
		m_editor->redo();
	}
}

void CMainFrame::OnUpdateEditRedo(CCmdUI* pCmdUI) 
{
	const char *redoName=0;
	if (m_editor)
	{
		redoName = m_editor->getCurrentRedoOperation();
	}

	if (redoName)
	{
		char temp[256];
		sprintf(temp, "Redo %s \tCtrl-Y", redoName);

		pCmdUI->Enable(TRUE);
		pCmdUI->SetText(temp);
	}
	else
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetText("Redo \tCtrl-Y");
	}
}

void CMainFrame::OnCheckout() 
{
	if (!m_editor)
	{
		return;
	}

	std::set<std::string> fileNames;

	const UIObjectSet &sels = m_editor->getSelections();
	for (UIObjectSet::const_iterator oi = sels.begin();oi != sels.end(); ++oi)
	{
		UIBaseObject *const o = *oi;

		UIString sourcePath;
		if (UIManager::gUIManager().GetRootPage() == o) 
		{
			sourcePath = Unicode::narrowToWide("ui_root.ui");
		}
		else
		{
			o->GetProperty(UIBaseObject::PropertyName::SourceFile, sourcePath);
		}
		
		fileNames.insert(Unicode::wideToNarrow(sourcePath));
	}

	std::set<std::string>::iterator fi;
	for (fi=fileNames.begin();fi!=fileNames.end();++fi)
	{
		const std::string &fileName = *fi;;

		char command[1024];
		sprintf(command, "p4 edit %s\n\n", fileName.c_str());
		system(command);
	}
}

void CMainFrame::OnUpdateCheckout(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(isEditing() && m_editor->getSelectionCount()>0);
}

void CMainFrame::OnViewObjectbrowser() 
{
	if (m_browserDialog)
	{
		if (m_browserDialogOpen)
		{
			m_browserDialog->ShowWindow(SW_HIDE);
			m_browserDialogOpen=false;
		}
		else
		{
			m_browserDialog->ShowWindow(SW_SHOW);
			m_browserDialogOpen=true;
		}
	}
}

void CMainFrame::OnUpdateViewObjectbrowser(CCmdUI* pCmdUI) 
{
	if (m_browserDialog)
	{
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(m_browserDialogOpen ? 1 : 0);
	}
	else
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	}
}

void CMainFrame::OnViewSelectionproperties() 
{
	if (m_propertiesDialog)
	{
		if (m_propertiesDialogOpen)
		{
			m_propertiesDialog->ShowWindow(SW_HIDE);
			m_propertiesDialogOpen=false;
		}
		else
		{
			m_propertiesDialog->ShowWindow(SW_SHOW);
			m_propertiesDialogOpen=true;
		}
	}
}

void CMainFrame::OnUpdateViewSelectionproperties(CCmdUI* pCmdUI)
{
        if (m_propertiesDialog)
        {
                pCmdUI->Enable(TRUE);
                pCmdUI->SetCheck(m_propertiesDialogOpen ? 1 : 0);
        }
        else
        {
                pCmdUI->Enable(FALSE);
                pCmdUI->SetCheck(0);
        }
}

void CMainFrame::OnViewThemeDark()
{
        _applyTheme(ThemeStyleNeoDark, true);
}

void CMainFrame::OnUpdateViewThemeDark(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleNeoDark);
}

void CMainFrame::OnViewThemeLight()
{
        _applyTheme(ThemeStyleLuminousLight, true);
}

void CMainFrame::OnUpdateViewThemeLight(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleLuminousLight);
}

void CMainFrame::OnViewThemeBlueprint()
{
        _applyTheme(ThemeStyleBlueprint, true);
}

void CMainFrame::OnUpdateViewThemeBlueprint(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleBlueprint);
}

void CMainFrame::OnViewThemeAurora()
{
        _applyTheme(ThemeStyleAurora, true);
}

void CMainFrame::OnUpdateViewThemeAurora(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleAurora);
}

void CMainFrame::OnViewThemeSolar()
{
        _applyTheme(ThemeStyleSolar, true);
}

void CMainFrame::OnUpdateViewThemeSolar(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleSolar);
}

void CMainFrame::OnViewThemeCarbon()
{
        _applyTheme(ThemeStyleCarbon, true);
}

void CMainFrame::OnUpdateViewThemeCarbon(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleCarbon);
}

void CMainFrame::OnViewThemeNordic()
{
        _applyTheme(ThemeStyleNordic, true);
}

void CMainFrame::OnUpdateViewThemeNordic(CCmdUI* pCmdUI)
{
        pCmdUI->SetRadio(m_activeTheme == ThemeStyleNordic);
}

void CMainFrame::OnViewToggleLayoutguides()
{
        m_showLayoutGuides = !m_showLayoutGuides;
        theApp.WriteProfileInt(sOverlaySection, sLayoutGuidesValueName, m_showLayoutGuides ? 1 : 0);
        _setMainWindowTitle();

        if (gPrimaryDisplay)
        {
                redrawViews(false);
        }
}

void CMainFrame::OnUpdateViewToggleLayoutguides(CCmdUI* pCmdUI)
{
        pCmdUI->SetCheck(m_showLayoutGuides ? 1 : 0);
}

void CMainFrame::OnViewToggleSafezones()
{
        m_showSafeZones = !m_showSafeZones;
        theApp.WriteProfileInt(sOverlaySection, sSafeZonesValueName, m_showSafeZones ? 1 : 0);
        _setMainWindowTitle();

        if (gPrimaryDisplay)
        {
                redrawViews(false);
        }
}

void CMainFrame::OnUpdateViewToggleSafezones(CCmdUI* pCmdUI)
{
        pCmdUI->SetCheck(m_showSafeZones ? 1 : 0);
}

void CMainFrame::OnViewToggleStatusmetrics()
{
        m_showStatusMetrics = !m_showStatusMetrics;
        theApp.WriteProfileInt(sWorkspaceSection, sStatusMetricsValueName, m_showStatusMetrics ? 1 : 0);

        if (gPrimaryDisplay)
        {
                redrawViews(false);
        }
        else
        {
                CString assistantText(m_designAssistant.getStatusText().c_str());
                if (m_showStatusMetrics)
                {
                        CString metrics = _buildStatusMetrics();
                        if (!metrics.IsEmpty())
                        {
                                if (!assistantText.IsEmpty())
                                {
                                        assistantText += _T("   |   ");
                                }
                                assistantText += metrics;
                        }
                }
                m_wndStatusBar.SetPaneText(0, assistantText);
        }
}

void CMainFrame::OnUpdateViewToggleStatusmetrics(CCmdUI* pCmdUI)
{
        pCmdUI->SetCheck(m_showStatusMetrics ? 1 : 0);
}

void CMainFrame::OnViewToggleAssistant()
{
        m_showAssistantOverlay = !m_showAssistantOverlay;
        theApp.WriteProfileInt(sWorkspaceSection, sAssistantValueName, m_showAssistantOverlay ? 1 : 0);

        if (gPrimaryDisplay)
        {
                redrawViews(false);
        }

        SetMessageText(m_showAssistantOverlay ? _T("Assistant overlays enabled") : _T("Assistant overlays hidden"));
}

void CMainFrame::OnUpdateViewToggleAssistant(CCmdUI* pCmdUI)
{
        pCmdUI->SetCheck(m_showAssistantOverlay ? 1 : 0);
}

void CMainFrame::OnViewToggleFocusmode()
{
        m_focusMode = !m_focusMode;
        theApp.WriteProfileInt(sWorkspaceSection, sFocusModeValueName, m_focusMode ? 1 : 0);

        if (gPrimaryDisplay)
        {
                redrawViews(false);
        }

        SetMessageText(m_focusMode ? _T("Focus mode enabled") : _T("Focus mode disabled"));
}

void CMainFrame::OnUpdateViewToggleFocusmode(CCmdUI* pCmdUI)
{
        pCmdUI->SetCheck(m_focusMode ? 1 : 0);
}

void CMainFrame::OnToolsAutolayoutResponsive()
{
        if (!m_editor)
        {
                return;
        }

        const int preferredColumns = m_designAssistant.hasAutoLayoutRecommendation() ? m_designAssistant.getRecommendedLayoutColumns() : 0;

        if (m_editor->autoLayoutSelectionsResponsive(preferredColumns, false))
        {
                SetMessageText(_T("Responsive auto layout applied"));
        }
}

void CMainFrame::OnUpdateToolsAutolayoutResponsive(CCmdUI* pCmdUI)
{
        pCmdUI->Enable(isEditing() && m_editor->getSelectionCount() > 1);
}

void CMainFrame::OnToolsBalancepadding()
{
        if (m_editor && m_editor->centerSelectionInParent())
        {
                SetMessageText(_T("Selection centered within parent"));
        }
}

void CMainFrame::OnUpdateToolsBalancepadding(CCmdUI* pCmdUI)
{
        pCmdUI->Enable(isEditing() && m_editor->getSelectionCount() == 1);
}
