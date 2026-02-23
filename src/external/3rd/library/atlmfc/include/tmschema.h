#pragma once

// This project relies on the legacy Windows header "tmschema.h" which was
// removed from modern Windows SDK releases.  The modern replacement header
// "vssym32.h" still provides the property identifiers and theme constants
// that the legacy ATL/MFC headers expect.  Including it here satisfies the
// old include without requiring changes to upstream code.
#include <vssym32.h>
