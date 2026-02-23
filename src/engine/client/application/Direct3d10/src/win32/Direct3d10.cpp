// ============================================================================
//
// Direct3d10.cpp
// Copyright 2024
//
// Runtime scaffolding for the experimental Direct3D 10 renderer.  The helper
// mirrors the structure of the Direct3d9 entry point to simplify temporary
// hard-coded integrations while the full backend is constructed.
//
// ============================================================================

#include "FirstDirect3d10.h"
#include "Direct3d10.h"

#include <sstream>

namespace
{
#ifdef _WIN32
        bool s_installed = false;
        Direct3d10::RuntimeState s_state;

        void refreshProbe()
        {
                s_state.probe = Direct3d10Bootstrap::probe();
                s_state.runtimeReady = s_state.probe.isReady();

                s_state.issues.clear();

                if (!s_state.probe.dxgiAvailable)
                        s_state.issues.emplace_back("DXGI runtime could not be loaded.");
                if (!s_state.probe.d3d10Available && !s_state.probe.d3d10_1Available)
                        s_state.issues.emplace_back("Direct3D 10 core runtime is unavailable.");
                if (!s_state.probe.deviceCreated)
                        s_state.issues.emplace_back("Failed to create a Direct3D 10 device instance.");

                for (const std::string &dependency : s_state.probe.missingDependencies)
                        s_state.issues.emplace_back("Missing dependency: " + dependency);
                for (const std::string &feature : s_state.probe.missingFeatures)
                        s_state.issues.emplace_back("Missing feature: " + feature);
                for (const std::string &warning : s_state.probe.warnings)
                        s_state.issues.emplace_back("Warning: " + warning);
        }
#else
        bool s_installed = false;
        Direct3d10::RuntimeState s_state;
#endif // _WIN32
}

void Direct3d10::install(bool preferDirect3d10)
{
#ifdef _WIN32
        s_state.preferDirect3d10 = preferDirect3d10;
        refreshProbe();
        s_installed = true;
#else
        (void)preferDirect3d10;
        s_state.runtimeReady = false;
        s_state.issues.clear();
        s_installed = true;
#endif // _WIN32
}

void Direct3d10::remove()
{
        s_installed = false;
#ifdef _WIN32
        s_state = RuntimeState();
#else
        s_state = RuntimeState();
#endif // _WIN32
}

Direct3d10::RuntimeState const &Direct3d10::getRuntimeState()
{
        return s_state;
}

void Direct3d10::setPreferDirect3d10(bool prefer)
{
        s_state.preferDirect3d10 = prefer;
#ifdef _WIN32
        if (s_installed)
                refreshProbe();
#endif // _WIN32
}

bool Direct3d10::isInstalled()
{
        return s_installed;
}

bool Direct3d10::isRuntimeReady()
{
        return s_installed && s_state.runtimeReady;
}

std::string Direct3d10::describeRuntime()
{
        std::ostringstream description;
        description << "Direct3D10 preferred=" << (s_state.preferDirect3d10 ? "true" : "false");
        description << ", installed=" << (s_installed ? "true" : "false");
        description << ", ready=" << (s_state.runtimeReady ? "true" : "false");

#ifdef _WIN32
        if (s_installed)
        {
                const Direct3d10Bootstrap::RuntimeProbe &probe = s_state.probe;
                description << ", featureLevel=0x" << std::hex << probe.featureLevel << std::dec;
                description << ", dedicatedVideoMemory=" << probe.dedicatedVideoMemory;
                if (probe.vendorId != 0)
                {
                        description << ", vendorId=0x" << std::hex << probe.vendorId;
                        description << ", deviceId=0x" << probe.deviceId << std::dec;
                }
                if (!probe.adapterDescription.empty())
                        description << ", adapter='" << probe.adapterDescription << "'";
        }
        if (!s_state.issues.empty())
        {
                description << " | issues:";
                for (const std::string &issue : s_state.issues)
                        description << " " << issue;
        }
#else
        description << " | Direct3D10 runtime is unavailable on this platform.";
#endif // _WIN32

        return description.str();
}
