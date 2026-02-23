// ======================================================================
//
// SwgPlusIntegration.cpp
//
// Reports client startup metrics and friend online presence updates to the
// SWGPlus web service.
//
// ======================================================================

#include "FirstSwgClient.h"
#include "SwgPlusIntegration.h"

#if defined(WIN32)

#include "clientGame/CommunityManager.h"
#include "clientGame/CommunityManager_FriendData.h"
#include "clientGame/Game.h"
#include "sharedFoundation/ApplicationVersion.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedMemoryManager/MemoryManager.h"
#include "sharedMessageDispatch/Emitter.h"
#include "sharedMessageDispatch/Message.h"
#include "sharedMessageDispatch/Receiver.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "UnicodeUtils.h"

#include <windows.h>
#include <winhttp.h>
#include <dxgi.h>
#include <map>
#include <ctime>
#include <cstdio>
#include "sharedSynchronization/Guard.h"
#include "sharedSynchronization/RecursiveMutex.h"
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "dxgi.lib")

namespace
{
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 0x00000200
#endif

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif

        struct HttpHandle
        {
                explicit HttpHandle(HINTERNET value = nullptr) : handle(value) {}
                ~HttpHandle()
                {
                        if (handle)
                                WinHttpCloseHandle(handle);
                }

                HttpHandle(HttpHandle const &) = delete;
                HttpHandle &operator =(HttpHandle const &) = delete;

                HINTERNET handle;
        };

        std::wstring getEndpoint(wchar_t const *environmentVariableName, wchar_t const *defaultValue)
        {
                if (!environmentVariableName || !defaultValue)
                        return std::wstring();

                wchar_t buffer[512] = { 0 };
                DWORD const length = GetEnvironmentVariableW(environmentVariableName, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));

                if (length > 0 && length < sizeof(buffer) / sizeof(buffer[0]))
                        return std::wstring(buffer, length);

                return std::wstring(defaultValue);
        }

        std::string escapeJson(std::string const &value)
        {
                std::string result;
                result.reserve(value.size());

                for (char c : value)
                {
                        switch (c)
                        {
                                case '\\': result += "\\\\"; break;
                                case '"': result += "\\\""; break;
                                case '\b': result += "\\b"; break;
                                case '\f': result += "\\f"; break;
                                case '\n': result += "\\n"; break;
                                case '\r': result += "\\r"; break;
                                case '\t': result += "\\t"; break;
                                default:
                                        if (static_cast<unsigned char>(c) < 0x20)
                                        {
                                                char buffer[7];
                                                _snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                                                result.append(buffer);
                                        }
                                        else
                                        {
                                                result += c;
                                        }
                                        break;
                        }
                }

                return result;
        }

        std::string wstringToUtf8(std::wstring const &value)
        {
                if (value.empty())
                        return std::string();

                int const required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), nullptr, 0, nullptr, nullptr);
                if (required <= 0)
                        return std::string();

                std::string result(static_cast<size_t>(required), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), &result[0], required, nullptr, nullptr);
                return result;
        }

        std::string unicodeToUtf8(Unicode::String const &value)
        {
                Unicode::UTF8String utf8(Unicode::wideToUTF8(value));
                return std::string(utf8.begin(), utf8.end());
        }

        std::string getUtcTimestamp()
        {
                std::time_t const now = std::time(nullptr);
                std::tm utcTime = {};

#if defined(_WIN32)
                if (gmtime_s(&utcTime, &now) != 0)
                        return std::string();
#else
                if (!gmtime_r(&now, &utcTime))
                        return std::string();
#endif

                char buffer[32];
                if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime) == 0)
                        return std::string();

                return buffer;
        }

        std::string getOsVersionString()
        {
                OSVERSIONINFOEXW versionInfo = {};
                versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
                bool const gotVersion = (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&versionInfo)) != 0);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

                if (!gotVersion)
                        return "unknown";

                std::ostringstream stream;
                stream << versionInfo.dwMajorVersion << '.' << versionInfo.dwMinorVersion << '.' << versionInfo.dwBuildNumber;

                if (versionInfo.szCSDVersion[0] != L'\0')
                        stream << ' ' << wstringToUtf8(std::wstring(versionInfo.szCSDVersion));

                return stream.str();
        }

        std::string getProcessorArchitecture()
        {
                SYSTEM_INFO systemInfo = {};
#if defined(_WIN64)
                GetNativeSystemInfo(&systemInfo);
#else
                GetSystemInfo(&systemInfo);
#endif

                switch (systemInfo.wProcessorArchitecture)
                {
                        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
#ifdef PROCESSOR_ARCHITECTURE_ARM64
                        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM
                        case PROCESSOR_ARCHITECTURE_ARM: return "arm";
#endif
                        case PROCESSOR_ARCHITECTURE_IA64: return "ia64";
                        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
                        default: break;
                }

                return "unknown";
        }

        unsigned long long getTotalSystemMemoryMb()
        {
                MEMORYSTATUSEX memoryStatus = { sizeof(memoryStatus) };
                if (!GlobalMemoryStatusEx(&memoryStatus) || memoryStatus.ullTotalPhys == 0)
                        return 0;

                return memoryStatus.ullTotalPhys / (1024ULL * 1024ULL);
        }

        struct GpuInfo
        {
                std::string name;
                unsigned long long vramMb;
                bool valid;
        };

        GpuInfo getPrimaryGpuInfo()
        {
                GpuInfo info = {};
                IDXGIFactory1 *factory1 = nullptr;
                IDXGIFactory *factory = nullptr;

                HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory1));
                if (SUCCEEDED(hr) && factory1)
                {
                        factory = factory1;
                }
                else
                {
                        hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory));
                        if (FAILED(hr) || !factory)
                                return info;
                }

                IDXGIAdapter *adapter = nullptr;
                hr = factory->EnumAdapters(0, &adapter);
                if (SUCCEEDED(hr) && adapter)
                {
                        DXGI_ADAPTER_DESC adapterDesc = {};
                        if (SUCCEEDED(adapter->GetDesc(&adapterDesc)))
                        {
                                info.name = wstringToUtf8(std::wstring(adapterDesc.Description));
                                info.vramMb = static_cast<unsigned long long>(adapterDesc.DedicatedVideoMemory / (1024ULL * 1024ULL));
                                info.valid = true;
                        }

                        adapter->Release();
                }

                if (factory1)
                        factory1->Release();
                else if (factory)
                        factory->Release();

                return info;
        }

        struct ReporterInfo
        {
                std::string loginId;
                std::string clusterName;
                std::string playerName;
                std::string playerId;
                bool        valid;
        };

        ReporterInfo getReporterInfo()
        {
                ReporterInfo info = {};
                Unicode::String playerName;
                NetworkId playerId;

                if (Game::getPlayerPath(info.loginId, info.clusterName, playerName, playerId))
                {
                        info.playerName = unicodeToUtf8(playerName);
                        info.playerId = playerId.getValueString();
                        info.valid = true;
                }
                else
                {
                        info.valid = false;
                }

                return info;
        }

        static bool s_hasSentStartupMetrics = false;

        bool configureSecureProtocols(HINTERNET sessionHandle)
        {
                if (!sessionHandle)
                        return false;

                DWORD secureProtocols =
                        WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
                        WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;

#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
                secureProtocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif

                return WinHttpSetOption(sessionHandle, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols)) == TRUE;
        }

        bool sendJsonToEndpoint(std::wstring const &endpoint, std::string const &payload)
        {
                if (endpoint.empty())
                        return false;

                URL_COMPONENTS components = {};
                components.dwStructSize = sizeof(components);
                components.dwSchemeLength = static_cast<DWORD>(-1);
                components.dwHostNameLength = static_cast<DWORD>(-1);
                components.dwUrlPathLength = static_cast<DWORD>(-1);
                components.dwExtraInfoLength = static_cast<DWORD>(-1);

                if (!WinHttpCrackUrl(endpoint.c_str(), 0, 0, &components))
                        return false;

                std::wstring host;
                if (components.dwHostNameLength > 0 && components.lpszHostName)
                        host.assign(components.lpszHostName, components.dwHostNameLength);
                if (host.empty())
                        return false;

                std::wstring path;
                if (components.dwUrlPathLength > 0 && components.lpszUrlPath)
                        path.assign(components.lpszUrlPath, components.dwUrlPathLength);
                if (components.dwExtraInfoLength > 0 && components.lpszExtraInfo)
                        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
                if (path.empty())
                        path = L"/";

                INTERNET_PORT port = components.nPort;
                if (port == 0)
                        port = (components.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

                DWORD const requestFlags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

                static RecursiveMutex s_httpMutex;
                Guard guard(s_httpMutex);

                HttpHandle session(WinHttpOpen(L"SWGClient/SwgPlus", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
                if (!session.handle)
                        return false;

                WinHttpSetTimeouts(session.handle, 5000, 5000, 5000, 5000);

                IGNORE_RETURN(configureSecureProtocols(session.handle));

                HttpHandle connection(WinHttpConnect(session.handle, host.c_str(), port, 0));
                if (!connection.handle)
                        return false;

                HttpHandle request(WinHttpOpenRequest(connection.handle, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags));
                if (!request.handle)
                        return false;

                BOOL const sendResult = WinHttpSendRequest(
                        request.handle,
                        L"Content-Type: application/json\r\n",
                        static_cast<DWORD>(-1),
                        payload.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(payload.data()),
                        payload.empty() ? 0 : static_cast<DWORD>(payload.size()),
                        payload.empty() ? 0 : static_cast<DWORD>(payload.size()),
                        0);

                if (!sendResult)
                        return false;

                return WinHttpReceiveResponse(request.handle, nullptr) == TRUE;
        }

        void appendReporterClientObject(std::ostringstream &payload, ReporterInfo const &reporter)
        {
                std::string const login = escapeJson(reporter.loginId);
                std::string const cluster = escapeJson(reporter.clusterName);
                std::string const playerName = escapeJson(reporter.playerName);
                std::string const playerId = escapeJson(reporter.playerId);

                payload << "\"client\":{";
                payload << "\"login_id\":\"" << login << "\",";
                payload << "\"cluster\":\"" << cluster << "\",";
                payload << "\"player_name\":\"" << playerName << "\",";
                payload << "\"player_id\":\"" << playerId << "\"";
                payload << "},";

                payload << "\"login_id\":\"" << login << "\",";
                payload << "\"cluster\":\"" << cluster << "\",";
                payload << "\"player_name\":\"" << playerName << "\",";
                payload << "\"player_id\":\"" << playerId << "\",";
        }

        void appendReporterObject(std::ostringstream &payload, ReporterInfo const &reporter)
        {
                std::string const login = escapeJson(reporter.loginId);
                std::string const cluster = escapeJson(reporter.clusterName);
                std::string const playerName = escapeJson(reporter.playerName);
                std::string const playerId = escapeJson(reporter.playerId);

                payload << "\"reporter\":{";
                payload << "\"login_id\":\"" << login << "\",";
                payload << "\"cluster\":\"" << cluster << "\",";
                payload << "\"player_name\":\"" << playerName << "\",";
                payload << "\"player_id\":\"" << playerId << "\"";
                payload << "},";

                payload << "\"login_id\":\"" << login << "\",";
                payload << "\"cluster\":\"" << cluster << "\",";
                payload << "\"player_name\":\"" << playerName << "\",";
                payload << "\"player_id\":\"" << playerId << "\",";
        }

        std::string buildStartupPayload()
        {
                std::ostringstream payload;
                payload << '{';
                payload << "\"schema_version\":1,";
                payload << "\"event\":\"startupMetrics\",";
                payload << "\"event_type\":\"startup_metrics\",";

                std::string const internalVersion = escapeJson(ApplicationVersion::getInternalVersion());
                std::string const publicVersion = escapeJson(ApplicationVersion::getPublicVersion());
                std::string const osVersion = escapeJson(getOsVersionString());
                std::string const architecture = escapeJson(getProcessorArchitecture());
                unsigned long const memoryLimitMb = MemoryManager::getLimit();
                unsigned long long const totalSystemMemoryMb = getTotalSystemMemoryMb();
                GpuInfo const gpuInfo = getPrimaryGpuInfo();
                ReporterInfo const reporter = getReporterInfo();
                if (!reporter.valid)
                        return std::string();

                payload << "\"client_version_internal\":\"" << internalVersion << "\",";
                payload << "\"client_version_public\":\"" << publicVersion << "\",";
                payload << "\"memory_limit_mb\":" << memoryLimitMb << ',';
                payload << "\"total_system_memory_mb\":" << totalSystemMemoryMb << ',';
                payload << "\"os_version\":\"" << osVersion << "\",";
                payload << "\"system_architecture\":\"" << architecture << "\",";

                if (gpuInfo.valid)
                {
                        std::string const gpuName = escapeJson(gpuInfo.name);
                        payload << "\"gpu_name\":\"" << gpuName << "\",";
                        payload << "\"gpu_vram_mb\":" << gpuInfo.vramMb << ',';
                }

                appendReporterClientObject(payload, reporter);

                std::string const timestamp = escapeJson(getUtcTimestamp());
                payload << "\"timestamp_utc\":\"" << timestamp << "\"";
                payload << '}';
                return payload.str();
        }

        std::string buildClientPresencePayload(bool const online)
        {
                ReporterInfo const reporter = getReporterInfo();
                if (!reporter.valid)
                        return std::string();

                std::ostringstream payload;
                payload << '{';
                payload << "\"schema_version\":1,";
                payload << "\"event\":\"clientPresence\",";
                payload << "\"event_type\":\"client_presence\",";
                appendReporterClientObject(payload, reporter);
                payload << "\"online\":" << (online ? "true" : "false") << ',';
                payload << "\"online_status\":" << (online ? "true" : "false") << ',';
                std::string const timestamp = escapeJson(getUtcTimestamp());
                payload << "\"timestamp_utc\":\"" << timestamp << "\"";
                payload << '}';
                return payload.str();
        }

        std::string buildFriendPresencePayload(Unicode::String const &friendName, bool const online)
        {
                ReporterInfo const reporter = getReporterInfo();
                if (!reporter.valid)
                        return std::string();

                std::string const friendNameUtf8 = unicodeToUtf8(friendName);
                if (friendNameUtf8.empty())
                        return std::string();

                std::string const escapedFriendName = escapeJson(friendNameUtf8);
                std::ostringstream payload;
                payload << '{';
                payload << "\"schema_version\":1,";
                payload << "\"event\":\"friendPresence\",";
                payload << "\"event_type\":\"friend_presence\",";
                appendReporterObject(payload, reporter);
                payload << "\"friend\":{";
                payload << "\"name\":\"" << escapedFriendName << "\",";
                payload << "\"display_name\":\"" << escapedFriendName << "\"";
                payload << "},";
                payload << "\"friend_name\":\"" << escapedFriendName << "\",";
                payload << "\"online\":" << (online ? "true" : "false") << ',';
                payload << "\"online_status\":" << (online ? "true" : "false") << ',';
                std::string const timestamp = escapeJson(getUtcTimestamp());
                payload << "\"timestamp_utc\":\"" << timestamp << "\"";
                payload << '}';
                return payload.str();
        }

        bool sendStartupMetrics()
        {
                if (s_hasSentStartupMetrics)
                        return true;

                std::wstring const endpoint = getEndpoint(L"SWGPLUS_METRIC_ENDPOINT", L"https://swgplus.com/api/swgplusintegration/metrics/");
                if (endpoint.empty())
                        return false;

                std::string const payload = buildStartupPayload();
                if (payload.empty())
                        return false;

                if (!sendJsonToEndpoint(endpoint, payload))
                        return false;

                s_hasSentStartupMetrics = true;
                return true;
        }

        bool sendClientPresence(bool const online)
        {
                std::wstring const endpoint = getEndpoint(L"SWGPLUS_PRESENCE_ENDPOINT", L"https://swgplus.com/api/swgplusintegration/presence/");
                if (endpoint.empty())
                        return false;

                std::string const payload = buildClientPresencePayload(online);
                if (payload.empty())
                        return false;

                return sendJsonToEndpoint(endpoint, payload);
        }

        void sendFriendPresence(Unicode::String const &friendName, bool const online)
        {
                std::wstring const endpoint = getEndpoint(L"SWGPLUS_PRESENCE_ENDPOINT", L"https://swgplus.com/api/swgplusintegration/presence/");
                if (endpoint.empty())
                        return;

                std::string const payload = buildFriendPresencePayload(friendName, online);
                if (payload.empty())
                        return;

                IGNORE_RETURN(sendJsonToEndpoint(endpoint, payload));
        }

        void sendFriendSnapshot()
        {
                CommunityManager::FriendList const &friendList = CommunityManager::getFriendList();
                for (CommunityManager::FriendList::const_iterator iter = friendList.begin(); iter != friendList.end(); ++iter)
                {
                        CommunityManager::FriendData const &friendData = iter->second;
                        sendFriendPresence(friendData.getName(), friendData.isOnline());
                }
        }

        class PresenceReporter : public MessageDispatch::Receiver
        {
        public:
                PresenceReporter()
                : MessageDispatch::Receiver()
                , m_callback(new MessageDispatch::Callback)
                , m_hasSentOnlinePresence(false)
                {
                        if (m_callback)
                        {
                                m_callback->connect(*this, &PresenceReporter::onFriendListChanged, static_cast<CommunityManager::Messages::FriendListChanged *>(0));
                                m_callback->connect(*this, &PresenceReporter::onFriendOnlineStatusChanged, static_cast<CommunityManager::Messages::FriendOnlineStatusChanged *>(0));
                        }

                        connectToMessage(Game::Messages::SCENE_CHANGED);

                        sendFriendSnapshot();
                }

                ~PresenceReporter()
                {
                        disconnectFromMessage(Game::Messages::SCENE_CHANGED);
                        delete m_callback;
                        m_callback = nullptr;
                }

        private:
                void trySendInitialPresence()
                {
                        if (!m_hasSentOnlinePresence)
                        {
                                if (sendClientPresence(true))
                                        m_hasSentOnlinePresence = true;
                        }

                        IGNORE_RETURN(sendStartupMetrics());
                }

                void onFriendListChanged(CommunityManager::Messages::FriendListChanged::Status const &)
                {
                        trySendInitialPresence();
                        sendFriendSnapshot();
                }

                void onFriendOnlineStatusChanged(CommunityManager::Messages::FriendOnlineStatusChanged::Name const &name)
                {
                        trySendInitialPresence();
                        bool const isOnline = CommunityManager::isFriendOnline(name);
                        sendFriendPresence(name, isOnline);
                }

                virtual void receiveMessage(MessageDispatch::Emitter const &source, MessageDispatch::MessageBase const &message)
                {
                        UNREF(source);
                        if (message.isType(Game::Messages::SCENE_CHANGED))
                                trySendInitialPresence();
                }

        private:
                MessageDispatch::Callback *m_callback;
                bool                        m_hasSentOnlinePresence;
        };

        PresenceReporter &getPresenceReporter()
        {
                static PresenceReporter reporter;
                return reporter;
        }
}

#endif // WIN32

void SwgPlusIntegration::install()
{
#if defined(WIN32)
        static bool s_installed = false;
        if (s_installed)
                return;

        s_installed = true;

        sendStartupMetrics();
        IGNORE_RETURN(sendClientPresence(true));
        getPresenceReporter();
#else
        // No-op on non-Windows platforms.
#endif
}
