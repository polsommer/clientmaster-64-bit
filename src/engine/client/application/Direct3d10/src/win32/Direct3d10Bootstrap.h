// ============================================================================
//
// Direct3d10Bootstrap.h
// Copyright 2024
//
// Lightweight runtime probes used to verify that the Direct3D 10 dependency
// chain is available on the executing system.  This is an interim helper while
// the full Direct3D 10 renderer is being brought online.
//
// ============================================================================

#ifndef INCLUDED_Direct3d10Bootstrap_H
#define INCLUDED_Direct3d10Bootstrap_H

#include <cstdint>
#include <string>
#include <vector>

struct Direct3d10Bootstrap
{
#ifdef _WIN32
        struct RuntimeProbe
        {
                bool dxgiAvailable = false;
                bool d3d10Available = false;
                bool d3d10_1Available = false;
                bool deviceCreated = false;
                std::uint32_t featureLevel = 0;
                std::uint64_t dedicatedVideoMemory = 0;
                std::string adapterDescription;
                std::uint32_t vendorId = 0;
                std::uint32_t deviceId = 0;
                bool isAmdAdapter = false;
                bool isNvidiaAdapter = false;
                bool isIntelAdapter = false;
                std::vector<std::string> missingDependencies;
                std::vector<std::string> missingFeatures;
                std::vector<std::string> warnings;

                bool isReady() const
                {
                        return dxgiAvailable
                                && (d3d10Available || d3d10_1Available)
                                && deviceCreated
                                && missingDependencies.empty()
                                && missingFeatures.empty();
                }
        };

        // Attempt to load the Direct3D 10 runtime components.  The function does
        // not keep the modules resident; it only verifies that they can be loaded
        // so the caller can surface actionable diagnostics to the user.
        static RuntimeProbe probe();
#endif // _WIN32
};

#endif
