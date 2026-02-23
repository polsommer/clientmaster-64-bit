// ======================================================================
//
// ClientMachine.h
// asommers
//
// copyright 2003, sony online entertainment
//
// ======================================================================

#ifndef INCLUDED_ClientMachine_H
#define INCLUDED_ClientMachine_H

// ======================================================================

#include <cstdint>

class ClientMachine
{
public:

	static void install();

	static CString const getHardwareInformationString();

        static std::uint64_t getPhysicalMemorySize ();
	static int getNumberOfPhysicalProcessors ();
	static int getNumberOfLogicalProcessors ();
	static TCHAR const * getCpuVendor ();
        static TCHAR const * getCpuIdentifier ();
        static int getCpuSpeed ();
        static int getNumberOfCdDrives();
        static int getNumberOfDvdDrives();

        static TCHAR const * getOs();
        static bool isWindows10OrGreater();
        static bool isWindows11OrGreater();

        static bool getDirectXSupported ();
        static bool getSupportsHardwareMouseCursor ();
        static const TCHAR * getDeviceDescription ();
        static unsigned short getVendorIdentifier ();
        static unsigned short getDeviceIdentifier ();
        static bool isAmdGpu();
        static const TCHAR * getDeviceDriverVersionText ();
        static int getDeviceDriverProduct();
        static int getDeviceDriverVersion ();
        static int getDeviceDriverSubversion ();
        static int getDeviceDriverBuild();

        static std::uint64_t getVideoMemorySize ();
        static int getVertexShaderMajorVersion ();
        static int getVertexShaderMinorVersion ();
        static int getPixelShaderMajorVersion ();
        static int getPixelShaderMinorVersion ();
        static const TCHAR * getDirectXVersion ();
        static int getDirectXVersionMajor ();
        static int getDirectXVersionMinor ();
        static char getDirectXVersionLetter ();
        static bool supportsDirectX12 ();
        static bool supportsVertexShaders ();
        static bool supportsPixelShaders ();
        static bool supportsVertexAndPixelShaders ();
        static bool supportsDirectX9Ex ();
        static bool supportsDirectX10 ();
        static int  getDxgiOutputCount ();
        static bool hasHdrCapableDisplay ();

        static int getNumberOfDisplayModes();
        static int getDisplayModeWidth(int displayModeIndex);
        static int getDisplayModeHeight(int displayModeIndex);
        static int getDisplayModeRefreshRate(int displayModeIndex);

	static TCHAR const * getSoundVersion();
	static bool getSoundHas2dProvider();
	static int getNumberOfSoundProviders();
	static TCHAR const * getSoundProvider(int soundProviderIndex);

	static TCHAR const * getBitsStatus();

	static int getNumberOfJoysticks();
	static TCHAR const * getJoystickDescription(int joystickIndex);
	static bool getJoystickSupportsForceFeedback(int joystickIndex);

	static CString const getTrackIRVersion();
};

// ======================================================================

#endif
