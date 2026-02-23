// ======================================================================
//
// SwgPlusIntegration.h
//
// Integrates the SWGPlus telemetry and presence reporting hooks that are
// used by the SwgClient application.
//
// ======================================================================

#ifndef INCLUDED_SwgPlusIntegration_H
#define INCLUDED_SwgPlusIntegration_H

namespace SwgPlusIntegration
{
        // Installs the SWGPlus reporting hooks. This is safe to call multiple
        // times; subsequent calls are ignored.
        void install();
}

#endif // INCLUDED_SwgPlusIntegration_H
