#pragma once

#include <cstdint>
#include <cstddef>

// Provide lightweight fallbacks for noexcept/constexpr on older MSVC toolsets
#if defined(_MSC_VER) && _MSC_VER < 1900
#define SWG_LEGACY_MSVC 1
#define SWG_NOEXCEPT
#define SWG_CONSTEXPR
#else
#define SWG_NOEXCEPT noexcept
#define SWG_CONSTEXPR constexpr
#endif

namespace swg
{
namespace plugin
{
    /**
     * Version triple describing the semantic version of a plugin or API surface.
     */
    struct Version
    {
#if defined(SWG_LEGACY_MSVC)
        Version() : major(0), minor(0), patch(0) {}
        Version(std::uint16_t majorValue, std::uint16_t minorValue, std::uint16_t patchValue)
            : major(majorValue), minor(minorValue), patch(patchValue)
        {
        }

        std::uint16_t major;
        std::uint16_t minor;
        std::uint16_t patch;
#else
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t patch = 0;
#endif
    };

    /**
     * Logging levels that plugins can emit through the host dispatch table.
     */
    enum class LogLevel : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    /**
     * Narrow string view utility that avoids depending on C++20's std::string_view.
     */
    struct StringView
    {
#if defined(SWG_LEGACY_MSVC)
        StringView() : data(nullptr), length(0) {}
        StringView(const char *dataValue, std::size_t lengthValue) : data(dataValue), length(lengthValue) {}

        const char *data;
        std::size_t length;
#else
        const char *data = nullptr;
        std::size_t length = 0;
#endif
    };

    /**
     * Host-provided services that plugins can call into. Function pointers are preferred so that the ABI remains C-compatible.
     */
    struct HostDispatch
    {
#if defined(SWG_LEGACY_MSVC)
        HostDispatch()
            : log(nullptr), registerCommand(nullptr), enqueueTask(nullptr), acquireService(nullptr), releaseService(nullptr)
        {
        }

        void (*log)(LogLevel level, StringView message);
        bool (*registerCommand)(StringView name, void (*callback)(void *userData), void *userData);
        void (*enqueueTask)(void (*task)(void *userData), void *userData);
        void *(*acquireService)(StringView serviceName);
        void (*releaseService)(StringView serviceName, void *service);
#else
        void (*log)(LogLevel level, StringView message) = nullptr;
        bool (*registerCommand)(StringView name, void (*callback)(void *userData), void *userData) = nullptr;
        void (*enqueueTask)(void (*task)(void *userData), void *userData) = nullptr;
        void *(*acquireService)(StringView serviceName) = nullptr;
        void (*releaseService)(StringView serviceName, void *service) = nullptr;
#endif
    };

    /**
     * Context passed to plugins during initialisation.
     */
    struct HostContext
    {
#if defined(SWG_LEGACY_MSVC)
        HostContext() : apiVersion(), dispatch() {}
        HostContext(Version version, HostDispatch hostDispatch) : apiVersion(version), dispatch(hostDispatch) {}

        Version apiVersion;
        HostDispatch dispatch;
#else
        SWG_CONSTEXPR HostContext() SWG_NOEXCEPT = default;
        SWG_CONSTEXPR HostContext(Version version, HostDispatch hostDispatch) SWG_NOEXCEPT
            : apiVersion(version), dispatch(hostDispatch)
        {
        }

        Version apiVersion{};
        HostDispatch dispatch{};
#endif
    };

    /**
     * Metadata describing a plugin so that the host can expose information in diagnostics and UI.
     */
    struct PluginDescriptor
    {
#if defined(SWG_LEGACY_MSVC)
        PluginDescriptor()
            : name(), description(), pluginVersion(), compatibleApiMin(1, 0, 0), compatibleApiMax(1, 0, 0)
        {
        }

        StringView name;
        StringView description;
        Version pluginVersion;
        Version compatibleApiMin;
        Version compatibleApiMax;
#else
        StringView name{};
        StringView description{};
        Version pluginVersion{};
        Version compatibleApiMin{1, 0, 0};
        Version compatibleApiMax{1, 0, 0};
#endif
    };

    /**
     * Collection of lifecycle callbacks that the plugin exposes.
     */
    struct Lifecycle
    {
#if defined(SWG_LEGACY_MSVC)
        Lifecycle() : onLoad(nullptr), onUnload(nullptr), onTick(nullptr) {}

        bool (*onLoad)(const HostContext &context);
        void (*onUnload)();
        void (*onTick)(double deltaSeconds);
#else
        bool (*onLoad)(const HostContext &context) = nullptr;
        void (*onUnload)() = nullptr;
        void (*onTick)(double deltaSeconds) = nullptr;
#endif
    };

    /**
     * The signature every plugin entry point must implement. Returning false indicates the plugin declined to load.
     */
    using EntryPoint = bool (*)(const HostContext &context, PluginDescriptor &descriptor, Lifecycle &lifecycle);

    SWG_CONSTEXPR Version makeVersion(std::uint16_t major, std::uint16_t minor, std::uint16_t patch) SWG_NOEXCEPT
    {
        return Version{major, minor, patch};
    }

    SWG_CONSTEXPR bool operator==(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
    }

    SWG_CONSTEXPR bool operator!=(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        return !(lhs == rhs);
    }

    SWG_CONSTEXPR bool operator<(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        if (lhs.major != rhs.major)
            return lhs.major < rhs.major;
        if (lhs.minor != rhs.minor)
            return lhs.minor < rhs.minor;
        return lhs.patch < rhs.patch;
    }

    SWG_CONSTEXPR bool operator>(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        return rhs < lhs;
    }

    SWG_CONSTEXPR bool operator<=(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        return !(rhs < lhs);
    }

    SWG_CONSTEXPR bool operator>=(const Version &lhs, const Version &rhs) SWG_NOEXCEPT
    {
        return !(lhs < rhs);
    }
}
}

#undef SWG_CONSTEXPR
#undef SWG_NOEXCEPT

extern "C" {
    /**
     * Plugins must export a function named `SwgRegisterPlugin` with this signature. The implementation should fill in the
     * descriptor and lifecycle structures before returning true.
     */
    bool SwgRegisterPlugin(const swg::plugin::HostContext &context,
                           swg::plugin::PluginDescriptor &descriptor,
                           swg::plugin::Lifecycle &lifecycle);
}

