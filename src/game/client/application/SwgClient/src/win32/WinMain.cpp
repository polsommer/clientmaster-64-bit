// ======================================================================
//
// WinMain.cpp
//
// Improved version with better memory management for 32-bit systems
//
// ======================================================================

#include "FirstSwgClient.h"
#include "ClientMain.h"
#include "../shared/SwgPlusIntegration.h"
#include "LocalizedString.h"
#include "StringId.h"
#include "clientGame/Game.h"
#include "sharedMemoryManager/MemoryManager.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"
#include "../../../../../../engine/shared/library/sharedGame/include/public/sharedGame/PlatformFeatureBits.h"

#include <shellapi.h>
#include <windows.h>
#include <VersionHelpers.h>

// Forward declaration of external command handler
extern void externalCommandHandler(const char*);

// ======================================================================
// Helper function to parse an integer from environment variable
// Ensures the string contains only numeric characters.

static bool ParseMemorySizeFromBuffer(const char* buffer, int& megabytes) {
	megabytes = 0;
	for (const char* b = buffer; *b; ++b) {
		if (!isdigit(*b)) // Check for valid numeric characters
			return false;
		megabytes = (megabytes * 10) + (*b - '0'); // Build integer value
	}
	return true;
}

// ======================================================================
// Set memory manager target based on environment variable "SWGCLIENT_MEMORY_SIZE_MB"
// If the environment variable is missing or invalid, returns false.

static bool SetUserSelectedMemoryManagerTarget() {
	char buffer[32]; // 32-byte buffer for environment variable
	DWORD result = GetEnvironmentVariable("SWGCLIENT_MEMORY_SIZE_MB", buffer, sizeof(buffer));

	// Check if the environment variable was successfully retrieved and within bounds
	if (result == 0 || result >= sizeof(buffer))
		return false;

	int megabytes = 0;
	if (!ParseMemorySizeFromBuffer(buffer, megabytes))
		return false;

	// Set the memory limit via MemoryManager
	MemoryManager::setLimit(megabytes, false, false);
	return true;
}

// ======================================================================
// Set default memory manager target size for 32-bit systems
// Optimizes usage for 3GB systems and ensures a minimum threshold of memory.

static void SetDefaultMemoryManagerTargetSize() {
	MEMORYSTATUSEX memoryStatus = { sizeof(memoryStatus) };

	if (GlobalMemoryStatusEx(&memoryStatus)) {
		int ramMB = static_cast<int>(memoryStatus.ullTotalPhys / 1048576); // Convert bytes to MB

		// Check if system is running with /3GB boot option and has more than 2 GB of RAM available
                const bool isLargeAddressAware = IsWindowsVersionOrGreater(5, 0, 0); // Check if large address aware mode is possible

		if (ramMB >= 3072 && isLargeAddressAware) {
			// Maximize memory usage up to 3GB if available and system supports it
			ramMB = 3072;
		}
		else if (ramMB >= 2048) {
			// Cap at 2048 MB (2GB) for systems without the /3GB flag
			ramMB = 2048;
		}
		else {
			// Use 75% of available RAM if less than 2GB
			ramMB = static_cast<int>(ramMB * 0.75);
		}

		// Ensure a minimum memory limit of 512 MB
		ramMB = (ramMB < 512) ? 512 : ramMB;

		// Set memory limit via MemoryManager
		MemoryManager::setLimit(ramMB, false, false);
	}
}

// ======================================================================
// Handle external commands, such as opening external URLs
// Launches an external URL if the trial or rental feature is active.

void externalCommandHandler(const char* command) {
        (void)command;
        const StringId trialNagId("client", "npe_nag_url_trial");
        const StringId rentalNagId("client", "npe_nag_url_rental");

	Unicode::String url;

	// Check if NPENagForTrial bit is set and localize the URL
	if ((Game::getSubscriptionFeatureBits() & ClientSubscriptionFeature::NPENagForTrial) != 0) {
		url = trialNagId.localize();
	}

	// If a valid URL is found, attempt to open it
	if (!url.empty()) {
		Unicode::NarrowString url8 = Unicode::wideToNarrow(url);

		HINSTANCE result = ShellExecute(nullptr, "open", url8.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

		// If an error occurs during execution, log a warning
		if (reinterpret_cast<int>(result) <= 32) {
			WARNING(true, ("Could not launch external application (%d)", reinterpret_cast<int>(result)));
		}
		else {
			Game::quit(); // Exit the game if the external app is launched successfully
		}
	}
}

// ======================================================================
// Entry point for the application
// This function sets up memory limits and invokes the main game loop.
//
// Parameters:
//   - hInstance: Handle to the current instance
//   - hPrevInstance: Handle to the previous instance (unused)
//   - lpCmdLine: Pointer to the command line arguments
//   - nCmdShow: Show state of the window (minimized, maximized, etc.)

int WINAPI WinMain(
	HINSTANCE hInstance,     // Handle to the current instance
	HINSTANCE hPrevInstance, // Handle to the previous instance
	LPSTR lpCmdLine,         // Command line arguments
	int nCmdShow             // Window show state
	) {
	// Try to set the memory limit based on user settings, fallback to default if not set
	if (!SetUserSelectedMemoryManagerTarget()) {
		SetDefaultMemoryManagerTargetSize();
	}

        // Report startup metrics and install SWGPlus integrations.
        SwgPlusIntegration::install();

        // Call the main function for the client
        return ClientMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

// ======================================================================

