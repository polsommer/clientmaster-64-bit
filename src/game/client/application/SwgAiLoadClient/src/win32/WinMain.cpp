// ======================================================================
//
// WinMain.cpp
//
// Purpose:
//   Standalone AI-driven load generator that spawns multiple headless SWG
//   clients, assigns deterministic test credentials, and nudges each bot
//   to move around like a real player.  The tool is intentionally
//   self-contained so it can be launched from automation or by hand
//   without a UI, relying only on Win32 process management.
//
// ======================================================================

#include "FirstSwgAiLoadClient.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <vector>

#ifndef IGNORE_RETURN
#define IGNORE_RETURN(x) static_cast<void>((x))
#endif

namespace
{
        bool extractJsonString(std::string const &source, std::string const &key, size_t startPos, size_t &outPos, std::string &outValue)
        {
                std::string const token = std::string("\"") + key + "\"";
                size_t const keyPos = source.find(token, startPos);
                if (keyPos == std::string::npos)
                        return false;

                size_t const colonPos = source.find(':', keyPos + token.size());
                if (colonPos == std::string::npos)
                        return false;

                size_t const valueStart = source.find('"', colonPos);
                if (valueStart == std::string::npos)
                        return false;

                size_t const valueEnd = source.find('"', valueStart + 1);
                if (valueEnd == std::string::npos)
                        return false;

                outValue.assign(source.begin() + valueStart + 1, source.begin() + valueEnd);
                outPos = valueEnd + 1;
                return true;
        }
        struct AiLoadConfig
        {
                AiLoadConfig()
                : startIndex(1)
                , endIndex(20)
                , accountPrefix("test1")
                , password("Oliver123")
                , loginServerAddress("login.swgplus.com")
                , loginServerPort(44453)
                , galaxy()
                , server()
                , clientExecutable()
                , forceHeadlessClient(false)
                , forceUiClient(false)
                , avatarNameTemplate()
                , launchSpacingMs(500)
                , wanderKickoffMs(8000)
                , verbose(false)
                , spacingOverridden(false)
                , wanderDelayOverridden(false)
                , clientExecutableOverridden(false)
                {
                }

                struct Account
                {
                        std::string username;
                        std::string password;
                        std::string character;
                };

                int startIndex;
                int endIndex;
                std::string accountPrefix;
                std::string password;
                std::string loginServerAddress;
                uint16_t loginServerPort;
                std::string galaxy; // optional shard/cluster name
                std::string server; // optional login server or address
                std::wstring clientExecutable; // overridden via command line
                bool forceHeadlessClient;
                bool forceUiClient;
                std::string avatarNameTemplate; // optional override for character name
                DWORD launchSpacingMs; // delay between process launches
                DWORD wanderKickoffMs; // how long to wait before telling bots to wander
                bool verbose;
                bool spacingOverridden;
                bool wanderDelayOverridden;
                bool clientExecutableOverridden;
                std::vector<Account> accounts;
        };

        struct BotProcess
        {
                BotProcess()
                : processInfo()
                {
                        ZeroMemory(&processInfo, sizeof(processInfo));
                }

                std::wstring commandLine;
                std::wstring executable;
                std::wstring workingDirectory;
                PROCESS_INFORMATION processInfo;
        };

        DWORD calculateSmartSpacing(AiLoadConfig const &config);

        std::wstring toWide(std::string const &source)
        {
                if (source.empty())
                        return std::wstring();

                int const required = MultiByteToWideChar(CP_UTF8, 0, source.c_str(), static_cast<int>(source.size()), nullptr, 0);
                std::wstring buffer(static_cast<size_t>(required), L'\0');
                IGNORE_RETURN(MultiByteToWideChar(CP_UTF8, 0, source.c_str(), static_cast<int>(source.size()), &buffer[0], required));
                return buffer;
        }

        std::string toNarrow(std::wstring const &source)
        {
                if (source.empty())
                        return std::string();

                int const required = WideCharToMultiByte(CP_UTF8, 0, source.c_str(), static_cast<int>(source.size()), nullptr, 0, nullptr, nullptr);
                std::string buffer(static_cast<size_t>(required), '\0');
                IGNORE_RETURN(WideCharToMultiByte(CP_UTF8, 0, source.c_str(), static_cast<int>(source.size()), &buffer[0], required, nullptr, nullptr));
                return buffer;
        }

        void logMessage(std::string const &text)
        {
                OutputDebugStringA(("[SwgAiLoadClient] " + text + "\n").c_str());
        }

        std::wstring readEnvironmentVariable(std::wstring const &name)
        {
                DWORD const required = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
                if (required == 0)
                        return std::wstring();

                std::wstring buffer(required, L'\0');
                DWORD const result = GetEnvironmentVariableW(name.c_str(), &buffer[0], required);
                if (result == 0 || result >= required)
                        return std::wstring();

                buffer.resize(result);
                return buffer;
        }

        bool isAffirmative(std::wstring const &value)
        {
                return _wcsicmp(value.c_str(), L"1") == 0
                        || _wcsicmp(value.c_str(), L"true") == 0
                        || _wcsicmp(value.c_str(), L"yes") == 0
                        || _wcsicmp(value.c_str(), L"on") == 0;
        }

        std::wstring getExecutableDirectory()
        {
                wchar_t modulePath[MAX_PATH] = {};
                DWORD const length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
                if (length == 0)
                        return std::wstring();

                std::wstring path(modulePath, modulePath + length);
                size_t const lastSlash = path.find_last_of(L"/\\");
                if (lastSlash == std::wstring::npos)
                        return std::wstring();

                return path.substr(0, lastSlash + 1);
        }

		bool fileExists(std::wstring const &path)
		{
			DWORD const attributes = GetFileAttributesW(path.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}

		std::wstring getDirectoryName(std::wstring const &path)
		{
			size_t const lastSlash = path.find_last_of(L"/\\");
			if (lastSlash == std::wstring::npos)
				return std::wstring();

			return path.substr(0, lastSlash + 1);
		}

		std::vector<std::wstring> getPreferredClientExecutableNames()
		{
			return {
				L"SwgHeadlessClient.exe",
				L"Swg+Client_r.exe",
				L"SwgClient_r.exe",
				L"Swg+Client.exe",
				L"SwgClient.exe",
			};
		}

		std::wstring discoverDefaultClientExecutable()
		{
			// Prefer the headless build because it starts quickly and
			// consumes fewer resources.  The loader is intended to run
			// without user interaction, so avoid launching the full UI
			// client unless an explicit override is provided.
			std::wstring const folder = getExecutableDirectory();
			std::wstring const envOverride = readEnvironmentVariable(L"SWG_AI_CLIENT");

			std::vector<std::wstring> candidates;
			candidates.reserve(16);

			if (!envOverride.empty())
				candidates.push_back(envOverride);

			if (!folder.empty())
			{
				std::vector<std::wstring> const basePaths = {
					folder,
					folder + L"..\\",
					folder + L"..\\bin\\",
				};

				for (std::wstring const &basePath : basePaths)
				{
					for (std::wstring const &name : getPreferredClientExecutableNames())
						candidates.push_back(basePath + name);
				}
			}

			for (auto const &candidate : candidates)
			{
				if (fileExists(candidate))
					return candidate;
			}

			return std::wstring();
		}

        std::wstring quote(std::wstring const &value)
        {
                return L"\"" + value + L"\"";
        }

        bool containsCaseInsensitive(std::wstring const &text, std::wstring const &needle)
        {
                std::wstring haystack(text);
                std::wstring target(needle);

                std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::towlower);
                std::transform(target.begin(), target.end(), target.begin(), ::towlower);

                return haystack.find(target) != std::wstring::npos;
        }

        bool shouldUseHeadlessProfile(AiLoadConfig const &config)
        {
                if (config.forceHeadlessClient)
                        return true;

                if (config.forceUiClient)
                        return false;

                if (config.clientExecutable.empty())
                        return true;

                return containsCaseInsensitive(config.clientExecutable, L"headless");
        }

        int wideToInt(std::wstring const &text)
        {
                return _wtoi(text.c_str());
        }

        void applyEnvironmentOverrides(AiLoadConfig &config)
        {
                std::wstring const envStart = readEnvironmentVariable(L"SWG_AI_START");
                if (!envStart.empty())
                        config.startIndex = wideToInt(envStart);

                std::wstring const envEnd = readEnvironmentVariable(L"SWG_AI_END");
                if (!envEnd.empty())
                        config.endIndex = wideToInt(envEnd);

                std::wstring const envPrefix = readEnvironmentVariable(L"SWG_AI_PREFIX");
                if (!envPrefix.empty())
                        config.accountPrefix = toNarrow(envPrefix);

                std::wstring const envPassword = readEnvironmentVariable(L"SWG_AI_PASSWORD");
                if (!envPassword.empty())
                        config.password = toNarrow(envPassword);

                std::wstring const envGalaxy = readEnvironmentVariable(L"SWG_AI_GALAXY");
                if (!envGalaxy.empty())
                        config.galaxy = toNarrow(envGalaxy);

                std::wstring const envServer = readEnvironmentVariable(L"SWG_AI_SERVER");
                if (!envServer.empty())
                {
                        config.server = toNarrow(envServer);
                        config.loginServerAddress = config.server;
                }

                std::wstring const envLoginAddress = readEnvironmentVariable(L"SWG_AI_LOGIN_ADDRESS");
                if (!envLoginAddress.empty())
                        config.loginServerAddress = toNarrow(envLoginAddress);

                std::wstring const envLoginPort = readEnvironmentVariable(L"SWG_AI_LOGIN_PORT");
                if (!envLoginPort.empty())
                        config.loginServerPort = static_cast<uint16_t>(wideToInt(envLoginPort));

                std::wstring const envSpacing = readEnvironmentVariable(L"SWG_AI_SPACING_MS");
                if (!envSpacing.empty())
                {
                        config.launchSpacingMs = static_cast<DWORD>(wideToInt(envSpacing));
                        config.spacingOverridden = true;
                }

                std::wstring const envWander = readEnvironmentVariable(L"SWG_AI_WANDER_DELAY_MS");
                if (!envWander.empty())
                {
                        config.wanderKickoffMs = static_cast<DWORD>(wideToInt(envWander));
                        config.wanderDelayOverridden = true;
                }

                std::wstring const envAvatar = readEnvironmentVariable(L"SWG_AI_AVATAR");
                if (!envAvatar.empty())
                        config.avatarNameTemplate = toNarrow(envAvatar);

                std::wstring const envVerbose = readEnvironmentVariable(L"SWG_AI_VERBOSE");
                if (!envVerbose.empty())
                        config.verbose = isAffirmative(envVerbose);

                std::wstring const envClient = readEnvironmentVariable(L"SWG_AI_CLIENT");
                if (!envClient.empty())
                {
                        config.clientExecutable = envClient;
                        config.clientExecutableOverridden = true;
                }
        }

        void parseCommandLine(AiLoadConfig &config)
        {
                int argc = 0;
                LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
                if (!argv)
                        return;

                for (int i = 1; i < argc; ++i)
                {
                        std::wstring const argument = argv[i];
                        auto equals = argument.find(L'=');
                        std::wstring key = argument;
                        std::wstring value;
                        if (equals != std::wstring::npos)
                        {
                                key = argument.substr(0, equals);
                                value = argument.substr(equals + 1);
                        }

                        if (key == L"--start" || key == L"-s")
                        {
                                if (i + 1 < argc)
                                        config.startIndex = _wtoi(argv[++i]);
                                else if (!value.empty())
                                        config.startIndex = wideToInt(value);
                        }
                        else if (key == L"--end" || key == L"-e")
                        {
                                if (i + 1 < argc)
                                        config.endIndex = _wtoi(argv[++i]);
                                else if (!value.empty())
                                        config.endIndex = wideToInt(value);
                        }
                        else if (key == L"--prefix" || key == L"-p")
                        {
                                if (i + 1 < argc)
                                        config.accountPrefix = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.accountPrefix = toNarrow(value);
                        }
                        else if (key == L"--password")
                        {
                                if (i + 1 < argc)
                                        config.password = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.password = toNarrow(value);
                        }
                        else if (key == L"--galaxy")
                        {
                                if (i + 1 < argc)
                                        config.galaxy = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.galaxy = toNarrow(value);
                        }
                        else if (key == L"--server")
                        {
                                if (i + 1 < argc)
                                        config.server = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.server = toNarrow(value);

                                if (!config.server.empty())
                                        config.loginServerAddress = config.server;
                        }
                        else if (key == L"--login-address")
                        {
                                if (i + 1 < argc)
                                        config.loginServerAddress = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.loginServerAddress = toNarrow(value);
                        }
                        else if (key == L"--login-port")
                        {
                                if (i + 1 < argc)
                                        config.loginServerPort = static_cast<uint16_t>(_wtoi(argv[++i]));
                                else if (!value.empty())
                                        config.loginServerPort = static_cast<uint16_t>(wideToInt(value));
                        }
                        else if (key == L"--client")
                        {
                                if (i + 1 < argc)
                                        config.clientExecutable = argv[++i];
                                else if (!value.empty())
                                        config.clientExecutable = value;

                                config.clientExecutableOverridden = true;
                        }
                        else if (key == L"--avatar")
                        {
                                if (i + 1 < argc)
                                        config.avatarNameTemplate = toNarrow(argv[++i]);
                                else if (!value.empty())
                                        config.avatarNameTemplate = toNarrow(value);
                        }
                        else if (key == L"--spacing")
                        {
                                if (i + 1 < argc)
                                        config.launchSpacingMs = static_cast<DWORD>(_wtoi(argv[++i]));
                                else if (!value.empty())
                                        config.launchSpacingMs = static_cast<DWORD>(wideToInt(value));

                                config.spacingOverridden = true;
                        }
                        else if (key == L"--wander-delay")
                        {
                                if (i + 1 < argc)
                                        config.wanderKickoffMs = static_cast<DWORD>(_wtoi(argv[++i]));
                                else if (!value.empty())
                                        config.wanderKickoffMs = static_cast<DWORD>(wideToInt(value));

                                config.wanderDelayOverridden = true;
                        }
                        else if (key == L"--verbose" || key == L"-v")
                        {
                                config.verbose = true;
                        }
                        else if (key == L"--ui-client" || key == L"--no-headless")
                        {
                                config.forceUiClient = true;
                        }
                        else if (key == L"--headless-client")
                        {
                                config.forceHeadlessClient = true;
                        }
                }

                LocalFree(argv);

                if (config.startIndex < 1)
                        config.startIndex = 1;

                if (config.endIndex < config.startIndex)
                        config.endIndex = config.startIndex;

                if (config.clientExecutable.empty())
                        config.clientExecutable = discoverDefaultClientExecutable();
        }

        std::wstring findAiConfig(std::wstring const &clientExecutable)
        {
                std::wstring const clientDirectory = getDirectoryName(clientExecutable);
                std::wstring const loaderDirectory = getExecutableDirectory();

                std::vector<std::wstring> candidates;

                if (!clientDirectory.empty())
                {
                        candidates.push_back(clientDirectory + L"Swg+ai.cfg");
                        candidates.push_back(clientDirectory + L"plugin\\ai_load_tester\\Swg+ai.cfg");
                }

                if (!loaderDirectory.empty())
                {
                        candidates.push_back(loaderDirectory + L"Swg+ai.cfg");
                        candidates.push_back(loaderDirectory + L"plugin\\ai_load_tester\\Swg+ai.cfg");
                }

                for (auto const &candidate : candidates)
                {
                        if (fileExists(candidate))
                                return candidate;
                }

                return std::wstring();
        }

        std::wstring discoverPreferredConfig(std::wstring const &clientExecutable)
        {
                std::wstring const envConfig = readEnvironmentVariable(L"SWG_CONFIG_FILE");
                if (!envConfig.empty())
                        return envConfig;

                std::wstring const aiConfig = findAiConfig(clientExecutable);
                if (!aiConfig.empty())
                        return aiConfig;

                std::wstring const clientDirectory = getDirectoryName(clientExecutable);
                std::wstring const loaderDirectory = getExecutableDirectory();

                std::vector<std::wstring> candidates;
                std::vector<std::wstring> const configNames = {
                        L"Swg+ai.cfg",
                        L"Swg+Setup.cfg",
                        L"Swg+Setup_d.cfg",
                        L"SwgAi.cfg",
                        L"ai.cfg",
                        L"ai_load.cfg",
                };

                if (!clientDirectory.empty())
                {
                        for (auto const &name : configNames)
                                candidates.push_back(clientDirectory + name);

                        candidates.push_back(clientDirectory + L"plugin\\ai_load_tester\\Swg+ai.cfg");
                        candidates.push_back(clientDirectory + L"plugin\\ai_load_tester\\ai.cfg");
                }

                if (!loaderDirectory.empty())
                {
                        candidates.push_back(loaderDirectory + L"plugin\\ai_load_tester\\Swg+ai.cfg");
                        candidates.push_back(loaderDirectory + L"plugin\\ai_load_tester\\ai.cfg");

                        for (auto const &name : configNames)
                                candidates.push_back(loaderDirectory + name);
                }

                for (auto const &candidate : candidates)
                {
                        if (fileExists(candidate))
                                return candidate;
                }

                return std::wstring();
        }

        std::wstring discoverScenarioPath(std::wstring const &clientExecutable)
        {
                std::wstring const envScenario = readEnvironmentVariable(L"SWG_AI_LOAD_SCENARIO");
                if (!envScenario.empty())
                        return envScenario;

                std::wstring const directory = getDirectoryName(clientExecutable);
                std::wstring const loaderDirectory = getExecutableDirectory();

                std::vector<std::wstring> candidates;
                std::vector<std::wstring> const scenarioNames = {
                        L"Swg+ai.cfg",
                        L"ai.cfg",
                        L"ai_load_scenario.json",
                        L"ai_load_accounts.json",
                };

                for (auto const &name : scenarioNames)
                        candidates.push_back(directory + L"plugin\\ai_load_tester\\" + name);

                for (auto const &name : scenarioNames)
                        candidates.push_back(directory + name);

                for (auto const &name : scenarioNames)
                        candidates.push_back(directory + L"..\\plugin\\ai_load_tester\\" + name);

                if (!loaderDirectory.empty())
                {
                        for (auto const &name : scenarioNames)
                                candidates.push_back(loaderDirectory + L"plugin\\ai_load_tester\\" + name);

                        for (auto const &name : scenarioNames)
                                candidates.push_back(loaderDirectory + name);
                }

                for (auto const &candidate : candidates)
                {
                        if (fileExists(candidate))
                                return candidate;
                }

                return std::wstring();
        }


        bool loadScenarioAccounts(std::wstring const &path, AiLoadConfig &config)
        {
                if (path.empty())
                        return false;

                std::ifstream file(path.c_str(), std::ios::binary);
                if (!file)
                        return false;

                std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                config.accounts.clear();

                size_t searchOffset = 0;
                for (;;)
                {
                        AiLoadConfig::Account account;
                        size_t nextOffset = searchOffset;

                        if (!extractJsonString(contents, "username", nextOffset, nextOffset, account.username))
                                break;

                        if (!extractJsonString(contents, "password", nextOffset, nextOffset, account.password))
                                break;

                        if (!extractJsonString(contents, "character", nextOffset, nextOffset, account.character))
                                break;

                        config.accounts.push_back(std::move(account));
                        searchOffset = nextOffset;
                }

                if (config.accounts.empty())
                        return false;

                config.startIndex = 1;
                config.endIndex = static_cast<int>(config.accounts.size());

                if (!config.spacingOverridden)
                        config.launchSpacingMs = calculateSmartSpacing(config);

                std::ostringstream oss;
                oss << "Loaded " << config.accounts.size() << " account" << (config.accounts.size() == 1 ? "" : "s")
                    << " from scenario file: " << toNarrow(path);
                logMessage(oss.str());

                return true;
        }

        void applyPreferredConfig(AiLoadConfig const &config)
        {
                std::wstring const preferredConfig = discoverPreferredConfig(config.clientExecutable);
                if (preferredConfig.empty())
                        return;

                if (SetEnvironmentVariableW(L"SWG_CONFIG_FILE", preferredConfig.c_str()))
                {
                        std::ostringstream oss;
                        oss << "Applying config file for child clients: " << toNarrow(preferredConfig);
                        logMessage(oss.str());
                }
                else
                {
                        DWORD const error = GetLastError();
                        std::ostringstream oss;
                        oss << "Unable to set SWG_CONFIG_FILE to " << toNarrow(preferredConfig)
                            << " (error " << error << ")";
                        logMessage(oss.str());
                }
        }

        DWORD calculateSmartSpacing(AiLoadConfig const &config)
        {
                SYSTEM_INFO systemInfo = {};
                GetSystemInfo(&systemInfo);

                DWORD const logicalProcessors = systemInfo.dwNumberOfProcessors > 0 ? systemInfo.dwNumberOfProcessors : 1;
                int const botCount = config.endIndex - config.startIndex + 1;
                DWORD const baseSpacing = config.launchSpacingMs > 0 ? config.launchSpacingMs : 250;

                if (botCount <= static_cast<int>(logicalProcessors))
                        return baseSpacing;

                // Space launches relative to available cores so we do not clobber the login server or
                // the local machine.  A gentle multiplier spreads out bursts while keeping the total
                // runtime bounded for CI jobs.
                DWORD const multiplier = static_cast<DWORD>((botCount + logicalProcessors - 1) / logicalProcessors);
                unsigned long long const scaled = static_cast<unsigned long long>(baseSpacing) * multiplier;
                DWORD const capped = scaled > 60000ULL ? 60000U : static_cast<DWORD>(scaled);

                // Never slow things down excessively; cap the auto spacing to a soft limit that keeps
                // batches manageable on CI hardware while still avoiding login storms.
                DWORD const softLimit = baseSpacing + 5000U;
                return std::min<DWORD>(capped, softLimit);
        }

        void applySmartDefaults(AiLoadConfig &config)
        {
                if (config.startIndex < 1)
                        config.startIndex = 1;

                if (config.endIndex < config.startIndex)
                        config.endIndex = config.startIndex;

                if (config.clientExecutable.empty())
                        config.clientExecutable = discoverDefaultClientExecutable();

                if (!config.spacingOverridden)
                        config.launchSpacingMs = calculateSmartSpacing(config);

                if (!config.wanderDelayOverridden && config.wanderKickoffMs < 1000)
                        config.wanderKickoffMs = 1000;
        }

        void applyScenarioAccounts(AiLoadConfig &config)
        {
                if (!config.accounts.empty())
                        return;

                std::wstring const scenarioPath = discoverScenarioPath(config.clientExecutable);
                if (!scenarioPath.empty())
                {
                        if (loadScenarioAccounts(scenarioPath, config))
                                return;

                        std::ostringstream oss;
                        oss << "Scenario file present but no accounts could be read: " << toNarrow(scenarioPath);
                        logMessage(oss.str());
                }

                // Fall back to a baked-in list when we cannot read Swg+ai.cfg.
                config.accounts = {
                        {"test1", "Oliver123", "Oala Labacold"},
                        {"test2", "Oliver123", "Hace Istecha"},
                        {"test3", "Oliver123", "Iscoo"},
                        {"test4", "Oliver123", "Odam Owosic"},
                        {"test5", "Oliver123", "Itheti Osaid"},
                        {"test6", "Oliver123", "Imiak So'Ekovi"},
                };

                config.startIndex = 1;
                config.endIndex = static_cast<int>(config.accounts.size());

                if (!config.spacingOverridden)
                        config.launchSpacingMs = calculateSmartSpacing(config);

                logMessage("Using built-in fallback accounts because Swg+ai.cfg could not be loaded.");
        }

        void logResolvedConfiguration(AiLoadConfig const &config)
        {
                bool const headlessProfile = shouldUseHeadlessProfile(config);
                std::ostringstream oss;
                oss << "Configured bots " << config.startIndex << " through " << config.endIndex
                        << " using prefix '" << config.accountPrefix << "'"
                        << " against " << config.loginServerAddress << ':' << config.loginServerPort
                        << " (" << (headlessProfile ? "headless" : "ui") << " profile)";

                if (!config.galaxy.empty())
                        oss << " on galaxy '" << config.galaxy << "'";

                oss << ", spacing=" << config.launchSpacingMs << "ms"
                        << ", wanderDelay=" << config.wanderKickoffMs << "ms"
                        << ", client=" << toNarrow(config.clientExecutable)
                        << ", avatar=" << (config.avatarNameTemplate.empty() ? "accountName" : config.avatarNameTemplate)
                        << (config.verbose ? ", verbose" : "");

                if (!config.accounts.empty())
                        oss << ", accountsLoaded=" << config.accounts.size();

                logMessage(oss.str());
        }

        std::string::size_type replaceAll(std::string &source, std::string const &from, std::string const &to)
        {
                std::string::size_type count = 0;
                std::string::size_type position = 0;
                while ((position = source.find(from, position)) != std::string::npos)
                {
                        source.replace(position, from.length(), to);
                        position += to.length();
                        ++count;
                }

                return count;
        }

        std::string buildAvatarName(AiLoadConfig const &config, std::string const &accountName, int accountNumber)
        {
                std::string avatarName = config.avatarNameTemplate.empty() ? accountName : config.avatarNameTemplate;

                // Allow callers to customize names while still keeping them unique and predictable.
                IGNORE_RETURN(replaceAll(avatarName, "{account}", accountName));
                IGNORE_RETURN(replaceAll(avatarName, "{index}", std::to_string(accountNumber)));

                return avatarName;
        }

        std::wstring buildBotCommandLine(AiLoadConfig const &config, int accountNumber)
        {
                std::string accountName;
                std::string password = config.password;
                std::string avatarName;
                bool const headlessProfile = shouldUseHeadlessProfile(config);

                if (!config.accounts.empty())
                {
                        size_t const index = static_cast<size_t>(accountNumber - 1);
                        if (index < config.accounts.size())
                        {
                                AiLoadConfig::Account const &account = config.accounts[index];
                                if (!account.username.empty())
                                        accountName = account.username;

                                password = account.password.empty() ? password : account.password;
                                avatarName = account.character;
                        }
                }

                if (accountName.empty())
                {
                        std::ostringstream botNameBuilder;
                        botNameBuilder << config.accountPrefix << accountNumber;
                        accountName = botNameBuilder.str();
                }

                if (!config.avatarNameTemplate.empty())
                        avatarName = buildAvatarName(config, accountName, accountNumber);
                else if (avatarName.empty())
                        avatarName = accountName;

                std::wostringstream command;
                command << quote(config.clientExecutable)
                        << L" --loginId " << quote(toWide(accountName))
                        << L" --password " << quote(toWide(password))
                        << L" --auto-select-first-character"
                        << L" autoConnectToLoginServer=true"
                        << L" loginClientID=" << quote(toWide(accountName))
                        << L" loginClientPassword=" << quote(toWide(password))
                        << L" AvatarName=" << quote(toWide(avatarName));

                if (headlessProfile)
                        command << L" --script ai_wander" // consumed by automation scripts baked into SwgHeadlessClient
                                << L" --headless --disable-ui --noaudio";

                if (!config.loginServerAddress.empty())
                {
                        command << L" --server " << quote(toWide(config.loginServerAddress))
                                << L" -s ClientGame"
                                << L" loginServerAddress0=" << toWide(config.loginServerAddress)
                                << L" loginServerPort0=" << config.loginServerPort
                                << L" loginServerAddress=" << toWide(config.loginServerAddress)
                                << L" loginServerPort=" << config.loginServerPort;
                }

                if (!config.galaxy.empty())
                        command << L" --galaxy " << quote(toWide(config.galaxy));

                if (config.verbose)
                        command << L" --verbose";

                return command.str();
        }

        bool launchBotProcess(BotProcess &bot)
        {
                STARTUPINFOW startupInfo = {};
                startupInfo.cb = sizeof(startupInfo);

                // CreateProcessW modifies the command line buffer in-place
                std::wstring commandLine = bot.commandLine;
                std::wstring workingDirectory = bot.workingDirectory;
                BOOL created = CreateProcessW(
                        bot.executable.c_str(),
                        &commandLine[0],
                        nullptr,
                        nullptr,
                        FALSE,
                        DETACHED_PROCESS,
                        nullptr,
                        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
                        &startupInfo,
                        &bot.processInfo);

                if (!created)
                {
                        DWORD const error = GetLastError();
                        std::ostringstream oss;
                        oss << "Failed to spawn bot with command line: " << toNarrow(bot.commandLine)
                            << " (error " << error << ")";
                        logMessage(oss.str());
                }

                return created == TRUE;
        }

        struct WanderPingContext
        {
                DWORD processId = 0;
                UINT message = WM_APP + 100;
        };

        BOOL CALLBACK sendWanderPingToWindow(HWND hwnd, LPARAM lParam)
        {
                WanderPingContext *context = reinterpret_cast<WanderPingContext *>(lParam);
                if (!context)
                        return TRUE;

                DWORD windowProcessId = 0;
                GetWindowThreadProcessId(hwnd, &windowProcessId);
                if (windowProcessId == context->processId)
                        PostMessage(hwnd, context->message, 0, 0);

                return TRUE;
        }

        void issueWanderPing(BotProcess const &bot)
        {
                // Gentle nudge for automation-aware clients: send a custom
                // message that interested windows can interpret as a signal
                // to start scripted wandering.  Clients that do not listen
                // will simply ignore this broadcast.
                WanderPingContext context;
                context.processId = bot.processInfo.dwProcessId;
                EnumWindows(&sendWanderPingToWindow, reinterpret_cast<LPARAM>(&context));
        }

        void waitForBots(std::vector<BotProcess> const &bots)
        {
                if (bots.empty())
                        return;

                std::vector<HANDLE> handles;
                handles.reserve(bots.size());
                for (BotProcess const &bot : bots)
                        handles.push_back(bot.processInfo.hProcess);

                // Wait for all children to complete; this keeps the loader
                // process alive so automation can scrape logs.
                WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, INFINITE);
        }
}

// ======================================================================
// Entry point for the application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
        UNUSED(hInstance);
        UNUSED(hPrevInstance);
        UNUSED(lpCmdLine);
        UNUSED(nCmdShow);

        AiLoadConfig config;
        applyEnvironmentOverrides(config);
        parseCommandLine(config);
        applySmartDefaults(config);

        applyPreferredConfig(config);
        applyScenarioAccounts(config);

        logResolvedConfiguration(config);

	if (config.clientExecutable.empty())
	{
		std::vector<std::wstring> const executableNames = getPreferredClientExecutableNames();
		std::wostringstream error;
		error << L"Unable to locate any of the following client executables next to SwgAiLoadClient: ";
		for (size_t i = 0; i < executableNames.size(); ++i)
		{
			if (i > 0)
				error << L", ";

			error << executableNames[i];
		}
		error << L". Provide --client or set SWG_AI_CLIENT to override.";

		MessageBoxW(nullptr,
			error.str().c_str(),
			L"SwgAiLoadClient",
			MB_OK | MB_ICONERROR);
		return 1;
	}

        std::vector<BotProcess> bots;
        for (int account = config.startIndex; account <= config.endIndex; ++account)
        {
                BotProcess bot;
                bot.commandLine = buildBotCommandLine(config, account);
                bot.executable = config.clientExecutable;
                bot.workingDirectory = getDirectoryName(bot.executable);

                std::ostringstream oss;
                oss << "Launching bot " << account << " with command line: " << toNarrow(bot.commandLine);
                logMessage(oss.str());

                if (launchBotProcess(bot))
                {
                        bots.push_back(bot);
                        Sleep(config.launchSpacingMs);
                }
        }

        // Give clients a moment to authenticate before asking them to move
        Sleep(config.wanderKickoffMs);
        for (BotProcess const &bot : bots)
                issueWanderPing(bot);

        waitForBots(bots);

        return 0;
}
// ======================================================================
