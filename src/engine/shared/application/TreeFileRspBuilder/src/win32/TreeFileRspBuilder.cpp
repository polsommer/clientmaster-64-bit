//===================================================================
//
// TreeFileRspBuilder.cpp
// asommers
//
// copyright 2001, sony online entertainment
//
//===================================================================

#include "FirstTreeFileRspBuilder.h"

#include <commdlg.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "User32.lib")
#endif

namespace
{
	using StringMap = std::multimap<std::string, std::string>;
	StringMap ms_stringMap;

	using DataMap = std::map<std::string, std::string>;
	DataMap ms_uncompressedMusicMap;
	DataMap ms_uncompressedSampleMap;
	DataMap ms_compressedTextureMap;
	DataMap ms_compressedAnimationMap;
	DataMap ms_compressedMeshSkeletalMap;
	DataMap ms_compressedMeshStaticMap;
	DataMap ms_compressedOtherMap;

	struct Bucket
	{
	public:
		bool     m_ext;
		std::string  m_key;
		int      m_keySize;
		DataMap* m_dataMap;

	public:
		Bucket(const bool ext, const char* const key, DataMap* const dataMap) :
			m_ext(ext),
			m_key(key),
			m_keySize(static_cast<int>(m_key.size())),
			m_dataMap(dataMap)
		{
		}
	};

using TreeFileList = std::vector<Bucket>;
TreeFileList ms_treeFileList;

std::vector<std::string> ms_bufferedMessages;

        struct DirectoryIdentifier
        {
                DWORD volumeSerialNumber;
                DWORD fileIndexHigh;
                DWORD fileIndexLow;
        };

        struct DirectoryIdentifierLess
        {
                bool operator()(const DirectoryIdentifier &lhs, const DirectoryIdentifier &rhs) const
                {
                        if (lhs.volumeSerialNumber != rhs.volumeSerialNumber)
                                return lhs.volumeSerialNumber < rhs.volumeSerialNumber;

                        if (lhs.fileIndexHigh != rhs.fileIndexHigh)
                                return lhs.fileIndexHigh < rhs.fileIndexHigh;

                        return lhs.fileIndexLow < rhs.fileIndexLow;
                }
        };

using VisitedDirectorySet = std::set<DirectoryIdentifier, DirectoryIdentifierLess>;
VisitedDirectorySet ms_visitedDirectories;
}

namespace
{
inline bool hasConsoleWindow()
{
return GetConsoleWindow() != nullptr;
}

inline void appendMessage(const std::string &text)
{
if (hasConsoleWindow())
{
if (!text.empty())
{
std::fwrite(text.c_str(), 1, text.size(), stdout);
if (text[text.size() - 1] == '\n')
{
std::fflush(stdout);
}
}
}
else
{
ms_bufferedMessages.push_back(text);
}
}

inline void logMessage(const char *format, ...)
{
va_list args;
va_start(args, format);
#if defined(_WIN32)
int length = _vscprintf(format, args);
va_end(args);
if (length < 0)
{
return;
}
std::vector<char> buffer(static_cast<size_t>(length) + 1u, '\0');
va_start(args, format);
std::vsnprintf(&buffer[0], buffer.size(), format, args);
va_end(args);
#else
int length = std::vsnprintf(nullptr, 0, format, args);
va_end(args);
if (length < 0)
{
return;
}
std::vector<char> buffer(static_cast<size_t>(length) + 1u, '\0');
va_start(args, format);
std::vsnprintf(&buffer[0], buffer.size(), format, args);
va_end(args);
#endif

appendMessage(std::string(&buffer[0], &buffer[0] + static_cast<size_t>(length)));
}

inline void flushMessages(bool success, const std::string &statusLine)
{
if (hasConsoleWindow())
{
if (!statusLine.empty())
{
std::printf("%s\n", statusLine.c_str());
}
return;
}

std::ostringstream stream;
for (std::size_t i = 0; i < ms_bufferedMessages.size(); ++i)
{
stream << ms_bufferedMessages[i];
}

std::string message = stream.str();
if (!statusLine.empty())
{
if (!message.empty() && message[message.size() - 1] != '\n')
{
message += '\n';
}
message += statusLine;
}

if (message.empty())
{
message = success ? "TreeFileRspBuilder completed successfully." : "TreeFileRspBuilder failed.";
}

const std::size_t maxDialogLength = 8192;
if (message.size() > maxDialogLength)
{
message.erase(0, message.size() - maxDialogLength);
}

MessageBoxA(nullptr, message.c_str(), "TreeFileRspBuilder", MB_OK | (success ? MB_ICONINFORMATION : MB_ICONERROR));
}

inline std::string getExecutableDirectory()
{
char buffer[MAX_PATH] = { 0 };
const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
if (length == 0 || length >= static_cast<DWORD>(sizeof(buffer)))
{
return std::string();
}

char *lastSlash = std::strrchr(buffer, '\\');
if (lastSlash == nullptr)
{
lastSlash = std::strrchr(buffer, '/');
}

if (lastSlash != nullptr)
{
*lastSlash = '\0';
}

return std::string(buffer);
}

inline bool tryDefaultConfig(std::string &outPath)
{
const std::string directory = getExecutableDirectory();
if (directory.empty())
{
return false;
}

const std::string candidate = directory + "\\TreeFileRspBuilder.cfg";
const DWORD attributes = GetFileAttributesA(candidate.c_str());
if (attributes == INVALID_FILE_ATTRIBUTES)
{
return false;
}

if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
{
return false;
}

outPath = candidate;
return true;
}

inline bool promptForConfig(std::string &outPath)
{
char fileBuffer[MAX_PATH] = { 0 };
OPENFILENAMEA ofn;
std::memset(&ofn, 0, sizeof(ofn));
ofn.lStructSize = sizeof(ofn);
ofn.hwndOwner = hasConsoleWindow() ? GetConsoleWindow() : nullptr;
ofn.lpstrFilter = "Configuration Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
ofn.lpstrFile = fileBuffer;
ofn.nMaxFile = static_cast<DWORD>(sizeof(fileBuffer));
ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
ofn.lpstrTitle = "Select TreeFileRspBuilder configuration file";

const std::string initialDirectory = getExecutableDirectory();
if (!initialDirectory.empty())
{
ofn.lpstrInitialDir = initialDirectory.c_str();
}

if (GetOpenFileNameA(&ofn) != FALSE)
{
outPath = fileBuffer;
return true;
}

const DWORD extendedError = CommDlgExtendedError();
if (extendedError != 0)
{
logMessage("File selection dialog failed (error %lu)\n", static_cast<unsigned long>(extendedError));
}

return false;
}

	inline void trimLeft(std::string &value)
	{
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch)
		{
			return !std::isspace(ch);
		}));
	}

	inline void trimRight(std::string &value)
	{
		value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch)
		{
			return !std::isspace(ch);
		}).base(), value.end());
	}

	inline void trim(std::string &value)
	{
		trimRight(value);
		trimLeft(value);
	}

        inline std::string joinPath(const std::string &base, const std::string &entry)
        {
                if (base.empty())
                        return entry;

		const char lastCharacter = base[base.size() - 1];
		if (lastCharacter == '/' || lastCharacter == '\\')
			return base + entry;

		return base + "/" + entry;
	}

	inline bool beginsWithI(const std::string &text, const std::string &prefix)
	{
		if (prefix.size() > text.size())
			return false;

		for (size_t i = 0; i < prefix.size(); ++i)
		{
			const unsigned char left = static_cast<unsigned char>(text[i]);
			const unsigned char right = static_cast<unsigned char>(prefix[i]);

			if (std::tolower(left) != std::tolower(right))
				return false;
		}

                return true;
        }

        inline bool tryEnterDirectory(const std::string &path)
        {
                HANDLE directoryHandle = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                if (directoryHandle == INVALID_HANDLE_VALUE)
                        return true;

                BY_HANDLE_FILE_INFORMATION information;
                const BOOL gotInformation = GetFileInformationByHandle(directoryHandle, &information);
                CloseHandle(directoryHandle);

                if (!gotInformation)
                        return true;

                const DirectoryIdentifier identifier =
                {
                        information.dwVolumeSerialNumber,
                        information.nFileIndexHigh,
                        information.nFileIndexLow
                };

                const auto insertResult = ms_visitedDirectories.insert(identifier);
                return insertResult.second;
        }
}

//===================================================================

static bool parseCommonCfg(const std::string &name)
{
std::ifstream infile(name.c_str());
if (!infile.is_open())
{
logMessage("Failed to open config file: %s\n", name.c_str());
return false;
}

	std::string line;
	while (std::getline(infile, line))
	{
		trim(line);
		if (line.empty())
			continue;

		std::size_t index = line.find('#');
		if (index == 0)
			continue;

		index = line.find(';');
		if (index == 0)
			continue;

		index = line.find('=');
		if (index == std::string::npos)
			continue;

		std::string left = line.substr(0, index);
		std::string right = line.substr(index + 1);
		trim(left);
		trim(right);

		if (left.find("searchPath") == std::string::npos)
			continue;

		auto iterator = ms_stringMap.begin();
		for (; iterator != ms_stringMap.end(); ++iterator)
		if (iterator->second == right)
			break;

if (iterator == ms_stringMap.end())
ms_stringMap.insert(StringMap::value_type(left, right));
}

if (ms_stringMap.empty())
logMessage("Warning: no searchPath entries found in %s\n", name.c_str());

return true;
}

//===================================================================

static void generateFiles(const std::string &explicitDirectory, const std::string &entryDirectory)
{
        if (explicitDirectory.empty())
                return;

if (!tryEnterDirectory(explicitDirectory))
{
logMessage("Skipping already visited directory: %s\n", explicitDirectory.c_str());
return;
}

	const std::string searchMask = joinPath(explicitDirectory, "*.*");

	WIN32_FIND_DATAA findData;
	const HANDLE handle = FindFirstFileA(searchMask.c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		const std::string fileName = findData.cFileName;
		if (fileName == "." || fileName == "..")
			continue;

		const bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		const std::string explicitName = joinPath(explicitDirectory, fileName);
		const std::string entryName = joinPath(entryDirectory, fileName);

                if (isDirectory)
                {
const bool isReparsePoint = (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
if (isReparsePoint)
{
logMessage("Skipping reparse point: %s\n", explicitName.c_str());
continue;
}

                        generateFiles(explicitName, entryName);
                }
                else
                {
			for (std::size_t i = 0; i < ms_treeFileList.size(); ++i)
			{
				bool found = i == ms_treeFileList.size() - 1;
				if (!found)
				{
					if (ms_treeFileList[i].m_ext)
						found = entryName.find(ms_treeFileList[i].m_key) != std::string::npos;
					else
						found = beginsWithI(entryName, ms_treeFileList[i].m_key);
				}

				if (found)
				{
					DataMap &dataMap = *ms_treeFileList[i].m_dataMap;
					const auto iterator = dataMap.find(entryName);

if (iterator == dataMap.end())
dataMap[entryName] = explicitName;
else
logMessage("Duplicate found: %s\n", entryName.c_str());

					break;
				}
			}
		}
	} while (FindNextFileA(handle, &findData));

	FindClose(handle);
}

//===================================================================

static bool writeRsp(const DataMap &dataMap, const std::string &name)
{
std::ofstream outfile(name.c_str(), std::ios::out | std::ios::trunc);
if (!outfile.is_open())
{
logMessage("Failed to open output file: %s\n", name.c_str());
return false;
}

for (const auto &entry : dataMap)
{
outfile << entry.first << " @ " << entry.second << '\n';
}

return true;
}

//===================================================================

int main(int argc, char* argv[])
{
ms_bufferedMessages.clear();
ms_stringMap.clear();
ms_treeFileList.clear();
ms_uncompressedMusicMap.clear();
ms_uncompressedSampleMap.clear();
ms_compressedTextureMap.clear();
ms_compressedAnimationMap.clear();
ms_compressedMeshSkeletalMap.clear();
ms_compressedMeshStaticMap.clear();
ms_compressedOtherMap.clear();
ms_visitedDirectories.clear();

bool success = false;
std::string statusLine;
std::string configurationPath;

do
{
if (argc >= 2)
{
configurationPath = argv[1];
}
else
{
if (!tryDefaultConfig(configurationPath))
{
logMessage("TreeFileRspBuilder configName\n");
if (!promptForConfig(configurationPath))
{
statusLine = "No configuration file was selected.";
break;
}
}
else
{
logMessage("Using default configuration file: %s\n", configurationPath.c_str());
}
}

if (!parseCommonCfg(configurationPath))
{
statusLine = "Failed to read configuration file.";
break;
}

ms_treeFileList.push_back(Bucket(true, ".mp3", &ms_uncompressedMusicMap));
ms_treeFileList.push_back(Bucket(true, ".wav", &ms_uncompressedSampleMap));
ms_treeFileList.push_back(Bucket(true, ".dds", &ms_compressedTextureMap));
ms_treeFileList.push_back(Bucket(true, ".png", &ms_compressedTextureMap));
ms_treeFileList.push_back(Bucket(true, ".pgn", &ms_compressedTextureMap));
ms_treeFileList.push_back(Bucket(true, ".ans", &ms_compressedAnimationMap));
ms_treeFileList.push_back(Bucket(true, ".mgn", &ms_compressedMeshSkeletalMap));
ms_treeFileList.push_back(Bucket(true, ".msh", &ms_compressedMeshStaticMap));
ms_treeFileList.push_back(Bucket(true, "", &ms_compressedOtherMap));

for (StringMap::reverse_iterator i = ms_stringMap.rbegin(); i != ms_stringMap.rend(); ++i)
{
logMessage("%s -> %s\n", i->first.c_str(), i->second.c_str());
generateFiles(i->second, "");
}

bool wroteAllFiles = true;
wroteAllFiles &= writeRsp(ms_uncompressedMusicMap, "data_uncompressed_music.rsp");
wroteAllFiles &= writeRsp(ms_uncompressedSampleMap, "data_uncompressed_sample.rsp");
wroteAllFiles &= writeRsp(ms_compressedTextureMap, "data_compressed_texture.rsp");
wroteAllFiles &= writeRsp(ms_compressedAnimationMap, "data_compressed_animation.rsp");
wroteAllFiles &= writeRsp(ms_compressedMeshStaticMap, "data_compressed_mesh_static.rsp");
wroteAllFiles &= writeRsp(ms_compressedMeshSkeletalMap, "data_compressed_mesh_skeletal.rsp");
wroteAllFiles &= writeRsp(ms_compressedOtherMap, "data_compressed_other.rsp");

if (!wroteAllFiles)
{
statusLine = "One or more response files could not be written.";
break;
}

success = true;
statusLine = "TreeFileRspBuilder completed successfully.";
}
while (false);

flushMessages(success, statusLine);

return success ? 0 : 1;
}

//===================================================================

enum MemoryManagerNotALeak
{
	MM_notALeak
};

void *operator new(size_t size, MemoryManagerNotALeak)
{
	return ::operator new(size);
}