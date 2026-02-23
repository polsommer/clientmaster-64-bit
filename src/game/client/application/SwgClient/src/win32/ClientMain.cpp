// ======================================================================
//
// ClientMain.cpp
// copyright 1998 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgClient.h"
#include "ClientMain.h"

#include "clientAnimation/SetupClientAnimation.h"
#include "clientAudio/Audio.h"
#include "clientAudio/SetupClientAudio.h"
#include "clientBugReporting/SetupClientBugReporting.h"
#include "clientDirectInput/DirectInput.h"
#include "clientDirectInput/SetupClientDirectInput.h"
#include "clientGame/Game.h"
#include "clientGame/SetupClientGame.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ScreenShotHelper.h"
#include "clientGraphics/ShaderTemplate.h"
#include "clientGraphics/SetupClientGraphics.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/VideoList.h"
#include "clientObject/SetupClientObject.h"
#include "clientParticle/SetupClientParticle.h"
#include "clientSkeletalAnimation/SetupClientSkeletalAnimation.h"
#include "clientTerrain/SetupClientTerrain.h"
#include "clientTextureRenderer/SetupClientTextureRenderer.h"
#include "clientUserInterface/CuiChatHistory.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "clientGraphics/IndexedTriangleListAppearance.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ApplicationVersion.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Branch.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Binary.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigFile.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/CrashReportInformation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ExitChain.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation//Os.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/SetupSharedFoundation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigSharedFoundation.h"
#include "sharedGame/CommoditiesAdvancedSearchAttribute.h"
#include "sharedGame/SetupSharedGame.h"
#include "sharedImage/SetupSharedImage.h"
#include "sharedIoWin/SetupSharedIoWin.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedLog/LogManager.h"
#include "sharedMath/SetupSharedMath.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMemoryManager/MemoryManager.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedPathfinding/SetupSharedPathfinding.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedTerrain/TerrainAppearance.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "sharedUtility/SetupSharedUtility.h"
#include "sharedXml/SetupSharedXml.h"
#include "swgClientUserInterface/SetupSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAuctionFilter.h"
#include "swgClientUserInterface/SwgCuiChatWindow.h"
#include "swgClientUserInterface/SwgCuiG15Lcd.h"
#include "swgClientUserInterface/SwgCuiManager.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"
#include "Resource.h"

// ---- SWG+ Enhancements (DX9/VS2013-friendly) --------------------------------
// Borderless fullscreen + monitor selection + profile suffix support.
// Minimal-risk additions that don't disturb engine subsystems.

#include <vector>

namespace
{
	// Parsed options
	struct SwgPlusOptions
	{
		bool   borderless;
		int    monitorIndex; // 0=primary
		char   profile[64];
		SwgPlusOptions() : borderless(false), monitorIndex(0) { profile[0] = '\0'; }
	};

	SwgPlusOptions g_swgPlus;

	// Basic, dependency-free cmdline parse: looks for -profile=NAME, -borderless, -monitor=N
	void parseSwgPlusCommandLine(const char* cmdLine)
	{
		if (!cmdLine || !*cmdLine)
			return;

		const char* p = cmdLine;
		while (*p)
		{
			while (*p==' ' || *p=='\t') ++p;
			if (*p=='-')
			{
				++p;
				const char* keyStart = p;
				while (*p && *p!=' ' && *p!='\t' && *p!='=') ++p;
				std::string key(keyStart, p - keyStart);
				std::string val;
				if (*p=='=')
				{
					++p;
					const char* vstart = p;
					while (*p && *p!=' ' && *p!='\t') ++p;
					val.assign(vstart, p - vstart);
				}
				if (_stricmp(key.c_str(), "borderless")==0)
					g_swgPlus.borderless = true;
				else if (_stricmp(key.c_str(), "monitor")==0 && !val.empty())
					g_swgPlus.monitorIndex = atoi(val.c_str());
				else if (_stricmp(key.c_str(), "profile")==0 && !val.empty())
				{
					strncpy(g_swgPlus.profile, val.c_str(), sizeof(g_swgPlus.profile)-1);
					g_swgPlus.profile[sizeof(g_swgPlus.profile)-1] = '\0';
				}
			}
			while (*p && *p!=' ' && *p!='\t') ++p;
		}
	}

	// Win32 helpers for borderless-fullscreen on selected monitor
	BOOL getMonitorRectByIndex(int index, RECT &out)
	{
		// index==0 => primary
		int i = 0;
                BOOL found = FALSE;

		auto enumProc = [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam)->BOOL
		{
			struct Payload { int target; int* i; RECT* out; BOOL* found; }* payload = (Payload*)lParam;
			MONITORINFOEX mi = {};
			mi.cbSize = sizeof(mi);
			if (GetMonitorInfo(hMon, &mi))
			{
				if (*(payload->i) == payload->target)
				{
					*(payload->out) = mi.rcMonitor;
					*(payload->found) = TRUE;
					return FALSE; // stop
				}
				(*(payload->i))++;
			}
			return TRUE; // continue
		};
		struct Payload { int target; int* i; RECT* out; BOOL* found; } payload = { index, &i, &out, &found };
		EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)enumProc, (LPARAM)&payload);

		if (!found)
		{
			// fallback: primary
			HMONITOR hPrimary = MonitorFromPoint(POINT{0,0}, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO mi = { sizeof(mi) };
			if (GetMonitorInfo(hPrimary, &mi))
			{
				out = mi.rcMonitor;
				return TRUE;
			}
			return FALSE;
		}
		return TRUE;
	}

	void applyBorderlessToWindow(HWND hwnd, int monitorIndex)
	{
		if (!IsWindow(hwnd))
			return;

		RECT r;
		if (!getMonitorRectByIndex(monitorIndex, r))
			return;

		// Remove border styles
		LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
		LONG_PTR ex    = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
		ex    &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
		SetWindowLongPtr(hwnd, GWL_STYLE, style);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);

		// Resize to monitor bounds
		SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW);
	}

}

#include "sharedGame/PlatformFeatureBits.h"
#include <windows.h>
#include <shellapi.h>
#include <dinput.h>
#include <string>
#include <ctime>
#include <cstdio>
#include <typeinfo>

extern void externalCommandHandler(const char*);

namespace ClientMainNamespace
{
void installConfigFileOverride()
{
AbstractFile* abstractFile = TreeFile::open("misc/override.cfg", AbstractFile::PriorityData, true);
		if (abstractFile)
		{
			int length = abstractFile->length();
			byte* data = abstractFile->readEntireFileAndClose();
			if (data)
			{
				IGNORE_RETURN(ConfigFile::loadFromBuffer(reinterpret_cast<char const*>(data), length));
				delete[] data;
			}
delete abstractFile;
}
}

	bool isMultipleInstanceAllowed()
	{
		bool allowMultipleInstances = ConfigFile::getKeyBool("SwgClient", "allowMultipleInstances", true);
		allowMultipleInstances = ConfigFile::getKeyBool("Swg+Client", "allowMultipleInstances", allowMultipleInstances);
		allowMultipleInstances = ConfigFile::getKeyBool("HavelonClient", "allowMultipleInstances", allowMultipleInstances);
		return allowMultipleInstances;
	}
}


using namespace ClientMainNamespace;

// ======================================================================
// Entry point for the application
//
// Return Value:
//
//   Result code to return to the operating system
//
// Remarks:
//
//   This routine should set up the engine, invoke the main game loop,
//   and then tear down the engine.
int ClientMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow
	)
{
	UNREF(hPrevInstance);
	UNREF(nCmdShow);

	// Skip resolution setup prompt if options.cfg already exists
	DWORD fileAttr = GetFileAttributesA("options.cfg");
	bool optionsExists = (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));

	if (!optionsExists)
	{
		// Prompt the user to run SwgClientSetup_r.exe
		int setupPromptResult = MessageBox(
			NULL,
			"Would you like to configure your resolution settings before starting the game?",
			"Resolution Setup",
			MB_YESNO | MB_ICONQUESTION
			);

		if (setupPromptResult == IDYES)
		{
			// Launch SwgClientSetup_r.exe
			SHELLEXECUTEINFO shellExecInfo = { 0 };
			shellExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			shellExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			shellExecInfo.hwnd = NULL;
			shellExecInfo.lpVerb = "open";
			shellExecInfo.lpFile = "SwgClientSetup_r.exe";
			shellExecInfo.lpParameters = "";
			shellExecInfo.lpDirectory = NULL;
			shellExecInfo.nShow = SW_SHOWNORMAL;
			shellExecInfo.hInstApp = NULL;

			if (!ShellExecuteEx(&shellExecInfo))
			{
				MessageBox(
					NULL,
					"Failed to launch resolution setup (SwgClientSetup_r.exe). The game will proceed with default settings.",
					"Error",
					MB_OK | MB_ICONERROR
					);
			}
			else
			{
				WaitForSingleObject(shellExecInfo.hProcess, INFINITE);
				CloseHandle(shellExecInfo.hProcess);
			}
		}
	}
	// Proceed with the original setup
	//-- thread
	SetupSharedThread::install();

	//-- debug
	SetupSharedDebug::install(4096);

	InstallTimer rootInstallTimer("root");

	char clientWindowName[128] = "Star Wars Galaxies + Release";

#if PRODUCTION != 1
	snprintf(clientWindowName, sizeof(clientWindowName), "Swg+Client (%s.%s)", Branch().getBranchName().c_str(), ApplicationVersion::getPublicVersion());
	clientWindowName[sizeof(clientWindowName)-1] = '\0';
#endif

	//-- foundation
	SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_game);
	data.windowName = clientWindowName;
	if (g_swgPlus.profile[0]) { strcat_s(clientWindowName, 128, " ["); strcat_s(clientWindowName, 128, g_swgPlus.profile); strcat_s(clientWindowName, 128, "]"); data.windowName = clientWindowName; }
	data.windowNormalIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	data.windowSmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	data.hInstance = hInstance;
	data.commandLine = lpCmdLine;
	parseSwgPlusCommandLine(lpCmdLine);
#if PRODUCTION == 0
	data.configFile = "Swg+Setup_d.cfg";
#else
	data.configFile = "Swg+Setup.cfg";
#endif
	data.clockUsesSleep = true;
	data.minFrameRate = 1.f;
	data.frameRateLimit = 144.f;

#if PRODUCTION
	data.demoMode = true;
#endif
	data.writeMiniDumps = true; // SWG Havelon Change 

	SetupSharedFoundation::install(data);

	REPORT_LOG(true, ("ClientMain: Command Line = \"%s\"\n", lpCmdLine));
	REPORT_LOG(true, ("ClientMain: Memory size = %i MB\n", MemoryManager::getLimit()));

	// Check if a config file is specified
	if (ConfigFile::isEmpty())
		FATAL(true, ("Config file not specified"));

	InstallTimer::checkConfigFile();
	// Clamp potentially dangerous config values to prevent integer overflow
	float maxFps = ConfigSharedFoundation::getFrameRateLimit();
	if (maxFps < 1.f || maxFps > 240.f)
	{
		WARNING(true, ("FrameRateLimit clamped to safe range (was %.2f)", maxFps));
		maxFps = 60.f;
	}
	data.frameRateLimit = maxFps;

		SetLastError(0);
		HANDLE semaphore = CreateSemaphore(NULL, 0, 1, "SwgClientInstanceRunning");
		if (GetLastError() == ERROR_ALREADY_EXISTS && !isMultipleInstanceAllowed())
		{
			MessageBox(NULL, "Another instance of this application is already running. Application will now close.", NULL, MB_OK | MB_ICONSTOP);
			if (semaphore)
				CloseHandle(semaphore);
			return 1;
		}

		// Configure game features
	uint32 gameFeatures = ConfigFile::getKeyInt("Station", "gameFeatures", 0) & ~ConfigFile::getKeyInt("ClientGame", "gameBitsToClear", 0);

	// Handle beta or preorder feature settings
	if (ConfigFile::getKeyBool("ClientGame", "setJtlRetailIfBetaIsSet", 0))
	{
		if (gameFeatures & (ClientGameFeature::SpaceExpansionBeta | ClientGameFeature::SpaceExpansionPreOrder))
			gameFeatures |= ClientGameFeature::SpaceExpansionRetail;
	}
	if (gameFeatures & (ClientGameFeature::Episode3PreorderDownload))
		gameFeatures |= ClientGameFeature::Episode3ExpansionRetail;
	if (gameFeatures & ClientGameFeature::TrialsOfObiwanPreorder)
		gameFeatures |= ClientGameFeature::TrialsOfObiwanRetail;

	// Set game and subscription feature bits
	Game::setGameFeatureBits(gameFeatures);
	Game::setSubscriptionFeatureBits(ConfigFile::getKeyInt("Station", "subscriptionFeatures", 0));
	Game::setExternalCommandHandler(externalCommandHandler);

	// Setup compression (ensure threads are properly managed)
	{
		SetupSharedCompression::Data compressionData;
		compressionData.numberOfThreadsAccessingZlib = 3;
		SetupSharedCompression::install(compressionData);
	}

	// Install shared regex support
	SetupSharedRegex::install();

	// Setup file system, ensuring we handle multiple SKU bits properly
	{
		uint32 skuBits = 0;
		if ((Game::getGameFeatureBits() & ClientGameFeature::Base) != 0)
			skuBits |= BINARY1(0001);
		if ((Game::getGameFeatureBits() & ClientGameFeature::SpaceExpansionRetail) != 0)
			skuBits |= BINARY1(0010);
		if ((Game::getGameFeatureBits() & ClientGameFeature::Episode3ExpansionRetail) != 0)
			skuBits |= BINARY1(0100);
		if ((Game::getGameFeatureBits() & ClientGameFeature::TrialsOfObiwanRetail) != 0)
			skuBits |= BINARY1(1000);

		SetupSharedFile::install(true, skuBits);
	}

	installConfigFileOverride();

	// Setup shared components (math, utility, random, log, etc.)
	SetupSharedMath::install();

	SetupSharedUtility::Data setupUtilityData;
	SetupSharedUtility::setupGameData(setupUtilityData);
	setupUtilityData.m_allowFileCaching = true;
	SetupSharedUtility::install(setupUtilityData);

	SetupSharedRandom::install(static_cast<uint32>(time(NULL)));
	if (g_swgPlus.profile[0]) { char logName[96]="SwgClient-"; strcat_s(logName, 96, g_swgPlus.profile); SetupSharedLog::install(logName);} else { SetupSharedLog::install("SwgClient"); }

	// Setup image support
	SetupSharedImage::Data setupImageData;
	SetupSharedImage::setupDefaultData(setupImageData);
	SetupSharedImage::install(setupImageData);

	// Setup network
	SetupSharedNetwork::SetupData networkSetupData;
	SetupSharedNetwork::getDefaultClientSetupData(networkSetupData);
	SetupSharedNetwork::install(networkSetupData);
	SetupSharedNetworkMessages::install();
	SetupSwgSharedNetworkMessages::install();

	// Setup object support
	SetupSharedObject::Data setupObjectData;
	SetupSharedObject::setupDefaultGameData(setupObjectData);
	setupObjectData.useTimedAppearanceTemplates = true;
	SetupSharedObject::addSlotIdManagerData(setupObjectData, true);
	SetupSharedObject::addCustomizationSupportData(setupObjectData);
	SetupSharedObject::addMovementTableData(setupObjectData);
	SetupSharedObject::install(setupObjectData);

	// Setup game features
	SetupSharedGame::Data setupSharedGameData;
	setupSharedGameData.setUseGameScheduler(true);
	setupSharedGameData.setUseMountValidScaleRangeTable(true);
	setupSharedGameData.m_debugBadStringsFunc = CuiManager::debugBadStringIdsFunc;
	SetupSharedGame::install(setupSharedGameData);

	// Auction and filter attributes
	CommoditiesAdvancedSearchAttribute::install();
	SwgCuiAuctionFilter::buildAttributeFilterDisplayString();

	// Setup terrain
	SetupSharedTerrain::Data setupSharedTerrainData;
	SetupSharedTerrain::setupGameData(setupSharedTerrainData);
	SetupSharedTerrain::install(setupSharedTerrainData);

	// Install XML and pathfinding
	SetupSharedXml::install();
	SetupSharedPathfinding::install();

	// Setup the client

	// Setup client audio
	SetupClientAudio::install();

	// Setup graphics (ensure memory for large resources like textures is handled properly)
	SetupClientGraphics::Data setupGraphicsData;
	setupGraphicsData.screenWidth = 1920;
	setupGraphicsData.screenHeight = 1080;
	setupGraphicsData.alphaBufferBitDepth = 0;
	SetupClientGraphics::setupDefaultGameData(setupGraphicsData);

	if (SetupClientGraphics::install(setupGraphicsData))
	{
		VideoList::install(Audio::getMilesDigitalDriver());

		// Setup DirectInput
		SetupClientDirectInput::install(hInstance, Os::getWindow(), DIK_LCONTROL, Graphics::isWindowed);
		DirectInput::setScreenShotFunction(ScreenShotHelper::screenShot);
		DirectInput::setToggleWindowedModeFunction(Graphics::toggleWindowedMode);
		DirectInput::setRequestDebugMenuFunction(Os::requestPopupDebugMenu);
		// --- SWG+ apply borderless if configured ---
		bool cfgBorderless = (ConfigFile::getKeyBool("ClientGraphics", "Borderless", false) != 0);
		int cfgMonitor = ConfigFile::getKeyInt("ClientGraphics", "Monitor", 0);
		if (g_swgPlus.borderless) cfgBorderless = true;
		if (g_swgPlus.monitorIndex) cfgMonitor = g_swgPlus.monitorIndex;
		if (cfgBorderless)
		{
			applyBorderlessToWindow(Os::getWindow(), cfgMonitor);
		}

		Os::setLostFocusHookFunction(DirectInput::unacquireAllDevices);

		// Setup client object
		SetupClientObject::Data setupClientObjectData;
		SetupClientObject::setupGameData(setupClientObjectData);
		SetupClientObject::install(setupClientObjectData);

		// Setup client animation and skeletal animation
		SetupClientAnimation::install();

		SetupClientSkeletalAnimation::Data saData;
		SetupClientSkeletalAnimation::setupGameData(saData);
		SetupClientSkeletalAnimation::install(saData);

		// Setup texture renderer, terrain, and particle system
		SetupClientTextureRenderer::install();
		SetupClientTerrain::install();
		SetupClientParticle::install();

		// Setup the client game
		SetupClientGame::Data clientGameData;
		SetupClientGame::setupGameData(clientGameData);
		SetupClientGame::install(clientGameData);

		// Setup UI manager functions
		CuiManager::setImplementationInstallFunctions(SwgCuiManager::install, SwgCuiManager::remove, SwgCuiManager::update);
		CuiManager::setImplementationTestFunction(SwgCuiManager::test);

		// Install bug reporting and IO window
		SetupClientBugReporting::install();
		SetupSharedIoWin::install();

		// Setup the client UI
		SetupSwgClientUserInterface::install();

		// Setup G15 LCD support
		SwgCuiG15Lcd::initializeLcd();

		// Run the game
		rootInstallTimer.manualExit();
		SetupSharedFoundation::callbackWithExceptionHandling(Game::run);

		// Save settings before exit
		if (CuiWorkspace * workspace = CuiWorkspace::getGameWorkspace())
		{
			workspace->saveAllSettings();
			if (SwgCuiChatWindow * chatWindow = safe_cast<SwgCuiChatWindow *>(workspace->findMediatorByType(typeid(SwgCuiChatWindow))))
				chatWindow->saveSettings();
		}
		CuiSettings::save();
		CuiChatHistory::save();
		CurrentUserOptionManager::save();
		LocalMachineOptionManager::save();
	}
	// Cleanup and exit
	SetupSharedFoundation::remove();
	SetupSharedThread::remove();

	return 0;
}
