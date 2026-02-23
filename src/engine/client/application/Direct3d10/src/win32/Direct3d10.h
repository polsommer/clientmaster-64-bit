// ============================================================================
//
// Direct3d10.h
// Copyright 2024
//
// High-level runtime scaffolding for the experimental Direct3D 10 renderer.
// The implementation mirrors the structure of the legacy Direct3d9 entry
// point so that higher level systems can be hard-wired against a familiar
// surface area while the new backend is brought online.
//
// ============================================================================

#ifndef INCLUDED_Direct3d10_H
#define INCLUDED_Direct3d10_H

#include "Direct3d10Bootstrap.h"

#include <string>
#include <vector>

struct Direct3d10
{
#ifdef _WIN32
        struct RuntimeState
        {
                Direct3d10Bootstrap::RuntimeProbe probe;
                bool preferDirect3d10 = false;
                bool runtimeReady = false;
                std::vector<std::string> issues;
        };
#else
        struct RuntimeState
        {
                bool preferDirect3d10 = false;
                bool runtimeReady = false;
                std::vector<std::string> issues;
        };
#endif // _WIN32

        // Initialise the bootstrap scaffolding.  When preferDirect3d10 is true
        // the runtime will aggressively attempt to switch to the Direct3D 10
        // path and will record diagnostics that can be surfaced to developers
        // when the probe fails.
        static void install(bool preferDirect3d10 = false);

        // Tear down the bootstrap state.
        static void remove();

        // Query the last recorded runtime state.
        static RuntimeState const &getRuntimeState();

        // Update the preferred runtime flag at runtime.
        static void setPreferDirect3d10(bool prefer);

        // Returns true when install() has been invoked and a probe has been
        // executed.
        static bool isInstalled();

        // Returns true if the last probe concluded that Direct3D 10 is
        // available and ready for use.
        static bool isRuntimeReady();

        // Convenience wrapper describing the current runtime status in a
        // human-readable format suitable for logs, crash reports, or tooling.
        static std::string describeRuntime();
};

#endif // INCLUDED_Direct3d10_H
