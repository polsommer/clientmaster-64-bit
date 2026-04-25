#ifndef INCLUDED_PluginManifest_H
#define INCLUDED_PluginManifest_H

#include <filesystem>
#include <string>
#include <vector>

namespace swg::plugin
{
    enum class PluginArchitecture
    {
        x86,
        x64,
        unknown
    };

    struct PluginManifest
    {
        std::string name;
        std::string entryPoint;
        std::string library;
        std::vector<PluginArchitecture> supportedArchitectures;
    };

    PluginArchitecture getCurrentProcessArchitecture();
    const char *toManifestArchitectureToken(PluginArchitecture architecture);
    const char *toInstallArchitectureSegment(PluginArchitecture architecture);

    bool parsePluginManifestFile(const std::filesystem::path &manifestPath, PluginManifest &manifest, std::string &error);
    bool validatePluginManifestForArchitecture(const PluginManifest &manifest, PluginArchitecture architecture, std::string &error);

#ifdef _WIN32
    void *loadPluginModuleFromManifest(const std::filesystem::path &manifestPath, std::string &error);
#endif
}

#endif
