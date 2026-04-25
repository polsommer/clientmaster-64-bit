#include "PluginManifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "json.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace swg::plugin
{
    namespace
    {
        std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            return value;
        }

        PluginArchitecture architectureFromToken(const std::string &token)
        {
            const std::string lowered = toLower(token);
            if (lowered == "x86" || lowered == "win32")
            {
                return PluginArchitecture::x86;
            }

            if (lowered == "x64" || lowered == "win64")
            {
                return PluginArchitecture::x64;
            }

            return PluginArchitecture::unknown;
        }

        std::string pluginLibraryFilename(const PluginManifest &manifest)
        {
            if (manifest.library.find('.') != std::string::npos)
            {
                return manifest.library;
            }

#ifdef _WIN32
            return manifest.library + ".dll";
#else
            return manifest.library;
#endif
        }
    }

    PluginArchitecture getCurrentProcessArchitecture()
    {
        return sizeof(void *) == 4 ? PluginArchitecture::x86 : PluginArchitecture::x64;
    }

    const char *toManifestArchitectureToken(const PluginArchitecture architecture)
    {
        switch (architecture)
        {
        case PluginArchitecture::x86:
            return "x86";
        case PluginArchitecture::x64:
            return "x64";
        default:
            return "unknown";
        }
    }

    const char *toInstallArchitectureSegment(const PluginArchitecture architecture)
    {
        switch (architecture)
        {
        case PluginArchitecture::x86:
            return "win32";
        case PluginArchitecture::x64:
            return "win64";
        default:
            return "unknown";
        }
    }

    bool parsePluginManifestFile(const std::filesystem::path &manifestPath, PluginManifest &manifest, std::string &error)
    {
        std::ifstream in(manifestPath);
        if (!in)
        {
            error = "Unable to open plugin manifest: " + manifestPath.string();
            return false;
        }

        std::stringstream buffer;
        buffer << in.rdbuf();

        nlohmann::json jsonValue;
        try
        {
            jsonValue = nlohmann::json::parse(buffer.str());
        }
        catch (const std::exception &exception)
        {
            error = "Failed to parse plugin manifest JSON for '" + manifestPath.string() + "': " + exception.what();
            return false;
        }

        if (!jsonValue.is_object())
        {
            error = "Plugin manifest root must be a JSON object: " + manifestPath.string();
            return false;
        }

        const auto nameIt = jsonValue.find("name");
        const auto entryPointIt = jsonValue.find("entryPoint");
        const auto libraryIt = jsonValue.find("library");
        const auto supportedArchitecturesIt = jsonValue.find("supportedArchitectures");

        if (nameIt == jsonValue.end() || !nameIt->is_string() || nameIt->get<std::string>().empty())
        {
            error = "Plugin manifest is missing required non-empty string field 'name': " + manifestPath.string();
            return false;
        }

        if (entryPointIt == jsonValue.end() || !entryPointIt->is_string() || entryPointIt->get<std::string>().empty())
        {
            error = "Plugin manifest is missing required non-empty string field 'entryPoint': " + manifestPath.string();
            return false;
        }

        if (libraryIt == jsonValue.end() || !libraryIt->is_string() || libraryIt->get<std::string>().empty())
        {
            error = "Plugin manifest is missing required non-empty string field 'library': " + manifestPath.string();
            return false;
        }

        if (supportedArchitecturesIt == jsonValue.end() || !supportedArchitecturesIt->is_array() || supportedArchitecturesIt->empty())
        {
            error = "Plugin manifest requires non-empty 'supportedArchitectures' array: " + manifestPath.string();
            return false;
        }

        PluginManifest parsed;
        parsed.name = nameIt->get<std::string>();
        parsed.entryPoint = entryPointIt->get<std::string>();
        parsed.library = libraryIt->get<std::string>();

        for (const auto &item : *supportedArchitecturesIt)
        {
            if (!item.is_string())
            {
                error = "Plugin manifest 'supportedArchitectures' values must be strings: " + manifestPath.string();
                return false;
            }

            const PluginArchitecture architecture = architectureFromToken(item.get<std::string>());
            if (architecture == PluginArchitecture::unknown)
            {
                error = "Plugin manifest contains unsupported architecture token '" + item.get<std::string>() + "' in " + manifestPath.string();
                return false;
            }

            if (std::find(parsed.supportedArchitectures.begin(), parsed.supportedArchitectures.end(), architecture) == parsed.supportedArchitectures.end())
            {
                parsed.supportedArchitectures.push_back(architecture);
            }
        }

        manifest = std::move(parsed);
        return true;
    }

    bool validatePluginManifestForArchitecture(const PluginManifest &manifest, const PluginArchitecture architecture, std::string &error)
    {
        const auto it = std::find(manifest.supportedArchitectures.begin(), manifest.supportedArchitectures.end(), architecture);
        if (it == manifest.supportedArchitectures.end())
        {
            error = "Plugin '" + manifest.name + "' does not support architecture '" + toManifestArchitectureToken(architecture) + "'.";
            return false;
        }

        return true;
    }

#ifdef _WIN32
    void *loadPluginModuleFromManifest(const std::filesystem::path &manifestPath, std::string &error)
    {
        PluginManifest manifest;
        if (!parsePluginManifestFile(manifestPath, manifest, error))
        {
            return nullptr;
        }

        const PluginArchitecture processArchitecture = getCurrentProcessArchitecture();
        if (!validatePluginManifestForArchitecture(manifest, processArchitecture, error))
        {
            return nullptr;
        }

        const std::filesystem::path libraryPath = manifestPath.parent_path() /
            toInstallArchitectureSegment(processArchitecture) /
            pluginLibraryFilename(manifest);

        HMODULE module = LoadLibraryA(libraryPath.string().c_str());
        if (!module)
        {
            error = "LoadLibrary failed for plugin '" + manifest.name + "' at path '" + libraryPath.string() + "'.";
            return nullptr;
        }

        return reinterpret_cast<void *>(module);
    }
#endif
}
