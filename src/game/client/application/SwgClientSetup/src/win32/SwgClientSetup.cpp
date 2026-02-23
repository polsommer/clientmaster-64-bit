// ======================================================================
//
// SwgClientSetup.cpp
// asommers
//
// copyright 2002, sony online entertainment
//
// ======================================================================

// **********************************************************************
// SWG Source 2021 - Aconite
// modernized to avoid legacy crash-email behavior and to surface
// contemporary hardware/OS detection results directly in the wizard.
// **********************************************************************

#include "FirstSwgClientSetup.h"
#include "SwgClientSetup.h"

#include "ClientMachine.h"
#include "Crc.h"
#include "DialogContact.h"
#include "DialogFinish.h"
#include "DialogHardwareInformation.h"
#include "DialogMinidump.h"
#include "DialogProgress.h"
#include "DialogRating.h"
#include "DialogStationId.h"
#include "Options.h"
#include "SwgClientSetupDlg.h"
#include "MessageBox2.h"

#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>
#include <string>
#include <cstdint>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// ======================================================================

namespace SwgClientSetupNamespace
{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	TCHAR const * const cms_registryFolder = _T("Software\\Sony Online Entertainment\\StarWarsGalaxies\\SwgClient");
	TCHAR const * const cms_sendMinidumpsRegistryKey = _T("SendCrashLogs");
	TCHAR const * const cms_sendHardwareInformationRegistryKey = _T("SendHardwareInformation");
	TCHAR const * const cms_informationCrcRegistryKey = _T("HardwareInformationCrc");
	TCHAR const * const cms_hardwareInformationCrcRegistryKey = _T("HardwareInformationCrc");
	TCHAR const * const cms_lastRatingTimeRegistryKey = _T("LastRatingTime");
	TCHAR const * const cms_machineRequirementsDisplayCountRegistryKey = _T("MachineRequirementsDisplayCount");
	TCHAR const * const cms_applicationName = _T("SwgClient_r.exe");
	//char const * const cms_fromEmailAddress = "swgbetatestcrashes@soe.sony.com";
	//char const * const cms_toEmailAddress = "swgbetatestcrashes@soe.sony.com";
	TCHAR const * const cms_fileNameMask = _T("SwgClient_?.exe-*.*");
	TCHAR const * const cms_languageStringJapanese = _T("ja");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getVersion (CString const & fileName)
	{
		int const start = fileName.Find ('-', 0) + 1;
		int const end = fileName.ReverseFind ('-');

		return fileName.Mid (start, end - start);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	int getMajorVersion (CString const & fileName)
	{
		CString const version = getVersion (fileName);

		int const end = version.Find ('.', 0);

		return _ttoi (version.Mid (0, end));
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	int getMinorVersion (CString const & fileName)
	{
		CString const version = getVersion (fileName);

		int const end = version.Find ('.', 0);

		return _ttoi (version.Mid (end + 1, version.GetLength () - end));
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getSubject (CString const & fileName)
	{
		return "automated crash dump " + fileName;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getBody (CString const & fileName)
	{
		CString body;

		CStdioFile file;
		if (file.Open (fileName + ".txt", CFile::modeRead | CFile::typeText))
		{
			CString line;
			while (file.ReadString (line))
				body += line + "\n";
		}
		else
			body = "automated crash dump\n\nunknown: FATAL 00000000: minidump from options program\n";

		return body;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getCode (CString const & fileName)
	{
		CString body;

		CStdioFile file;
		if (file.Open (fileName + ".txt", CFile::modeRead | CFile::typeText))
		{
			CString line;
			while (file.ReadString (line))
			{
				if (line.Find (_T("Exception")) != -1 || line.Find (_T("FATAL")) != -1)
				{
					body += line;

					break;
				}
			}
		}

		if (body.GetLength () == 0)
			body = "unknown";

		return body;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getMinidump (CString const & fileName)
	{
		CString result;

		CFile file;
		if (file.Open (fileName + ".mdmp", CFile::modeRead))
			result += fileName + ".mdmp";

		return result;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getLog (CString const & fileName)
	{
		CString result;

		CFile file;
		if (file.Open (fileName + ".log", CFile::modeRead))
			result += fileName + ".log";

		return result;
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	bool registryKeyExists (TCHAR const * const key)
	{
		CRegKey regKey;
		regKey.Open (HKEY_CURRENT_USER, cms_registryFolder);

		DWORD value = 0;
#if _MSC_VER < 1300
		return regKey.QueryValue (value, key) == ERROR_SUCCESS;
#else
		return regKey.QueryDWORDValue (key, value) == ERROR_SUCCESS;
#endif
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	DWORD getRegistryKey (TCHAR const * const key)
	{
		CRegKey regKey;
		regKey.Open (HKEY_CURRENT_USER, cms_registryFolder);

		DWORD value = 0;
#if _MSC_VER < 1300
		regKey.QueryValue (value, key);
#else
		regKey.QueryDWORDValue (key, value);
#endif

		return value;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void setRegistryKey (TCHAR const * const key, DWORD const value)
	{
		CRegKey regKey;
		regKey.Create (HKEY_CURRENT_USER, cms_registryFolder);
#if _MSC_VER < 1300
		regKey.SetValue (value, key);
#else
		regKey.SetDWORDValue (key, value);
#endif
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	CString getStationId (CString const & commandLine)
	{
		int index = commandLine.Find (_T("stationId="));
		if (index != -1)
		{
			index = commandLine.Find (_T("="), index);
			CString result = commandLine.Right (commandLine.GetLength () - index - 1);
			index = result.Find (_T(" "));
			if (index != -1)
				result = result.Left (index);

			return result;
		}

		return _T("0");
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	int getLanguageCode (CString const & commandLine)
	{
		int index = commandLine.Find (_T("locale="));
		if (index != -1)
		{
			index = commandLine.Find (_T("="), index);
			CString result = commandLine.Right (commandLine.GetLength () - index - 1);
			index = result.Find (_T(" "));
			if (index != -1)
				result = result.Left (index);

			if (wcsncmp(result, cms_languageStringJapanese, 2) == 0)
				return cms_languageCodeJapanese;
		}

		return cms_languageCodeEnglish;
	}
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	bool hasJumpToLightspeed (CString const & commandLine)
	{
		int index = commandLine.Find (_T("gameFeatures="));
		if (index != -1)
		{
			index = commandLine.Find (_T("="), index);
			CString result = commandLine.Right (commandLine.GetLength () - index - 1);
			index = result.Find (_T(" "));
			if (index != -1)
				result = result.Left (index);

			int const gameFeatures = _ttoi(result);

			return (gameFeatures & 16) != 0;
		}

		return false;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void RemoveFile(char const * fileName)
	{
		if (_unlink(fileName) == -1 && errno == EACCES)
		{
			_chmod(fileName, _S_IREAD | _S_IWRITE);
			_unlink(fileName);
		}
	}
}

using namespace SwgClientSetupNamespace;

// ======================================================================

BEGIN_MESSAGE_MAP(SwgClientSetupApp, CWinApp)
	//{{AFX_MSG_MAP(SwgClientSetupApp)
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

// ----------------------------------------------------------------------

SwgClientSetupApp::SwgClientSetupApp()
{
}

// ----------------------------------------------------------------------

SwgClientSetupApp theApp;

// ----------------------------------------------------------------------

class CCommandLineInfo2 : public CCommandLineInfo
{
public:

	CCommandLineInfo2 () :
		CCommandLineInfo (),
		m_numberOfParameters(0),
		m_numberOfErrors(5),
		m_debugExitPoll(false)
	{
	}

	virtual void ParseParam(TCHAR const * pszParam, BOOL /*bFlag*/, BOOL /*bLast*/)
	{
		if (wcsstr(pszParam, _T("locale")) == 0)
			++m_numberOfParameters;

		if (wcsstr(pszParam, _T("Station")) != 0)
			--m_numberOfErrors;

		if (wcsstr(pszParam, _T("sessionId")) != 0)
			--m_numberOfErrors;

		if (wcsstr(pszParam, _T("stationId")) != 0)
			--m_numberOfErrors;

		if (wcsstr(pszParam, _T("subscriptionFeatures")) != 0)
			--m_numberOfErrors;

		if (wcsstr(pszParam, _T("gameFeatures")) != 0)
			--m_numberOfErrors;

		m_debugExitPoll |= (wcsstr(pszParam, _T("debugExitPoll")) != 0);
	}

	bool shouldLaunchSwgClient() const
	{
		return m_numberOfParameters > 0;
	}

	bool looksValid() const
	{
		return m_numberOfErrors == 0;
	}

	bool debugExitPoll() const
	{
		return m_debugExitPoll;
	}

private:

	int m_numberOfParameters;
	int m_numberOfErrors;
	bool m_debugExitPoll;
};

// ----------------------------------------------------------------------

BOOL SwgClientSetupApp::InitInstance()
{
	MessageBox2::install(cms_registryFolder);

	// Initialize MFC controls.
	AfxEnableControlContainer();

#if _MSC_VER < 1300
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
#endif


	// Standard initialization
	HANDLE semaphore = CreateSemaphore(NULL, 0, 1, _T("SwgClientSetupInstanceRunning"));
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CString anotherStr;
		VERIFY(anotherStr.LoadString(IDS_ANOTHER_INSTANCE));
		MessageBox(NULL, anotherStr, NULL, MB_OK | MB_ICONSTOP);
		CloseHandle(semaphore);
		return FALSE;
	}

        ClientMachine::install ();
	
	int const langCode = getLanguageCode(AfxGetApp()->m_lpCmdLine);
	Options::load (langCode);

	
	//-- Set the thread locale in order to use the correct string table
	if (langCode == cms_languageCodeJapanese)	
		SetThreadLocale(MAKELCID(0x0411, SORT_DEFAULT));
	else
	{
		//-- English
		SetThreadLocale(MAKELCID(0x0409, SORT_DEFAULT));
	}
	
	//--
	CCommandLineInfo2 commandLineInfo;
	ParseCommandLine (commandLineInfo);

	if (commandLineInfo.shouldLaunchSwgClient())
	{
		if (!commandLineInfo.looksValid())
		{

			OutputDebugString(CString("SwgClientSetup command line: ") + m_lpCmdLine + "\n");

			CString message;
			VERIFY(message.LoadString(IDS_INVALIDCOMMANDLINE));
			if (MessageBox(NULL, message, NULL, MB_YESNO | MB_ICONSTOP) == IDNO)
				return FALSE;
	}
	if (ClientMachine::getDirectXVersionMajor() < 9 || ClientMachine::getDirectXVersionLetter() < 'c')
	{
		MessageBox2 messageBox(CString("You have DirectX ") + CString(ClientMachine::getDirectXVersion()) + CString(" installed but DirectX 9.0c is currently required to play Star Wars Galaxies.  For upgrade information, please see:\n\n\thttp://starwarsgalaxies.station.sony.com/content.jsp?page=Directx%20Upgrade"));
		messageBox.setOkayButton("Quit");
		messageBox.setCancelButton("");
		messageBox.setWebButton("Go to web page", "http://starwarsgalaxies.station.sony.com/content.jsp?page=Directx%20Upgrade");
		messageBox.DoModal();
		return FALSE;
	}                  

	unsigned short const vendor = ClientMachine::getVendorIdentifier ();
	unsigned short const device = ClientMachine::getDeviceIdentifier ();
	int const driverProduct = ClientMachine::getDeviceDriverProduct();
	int const driverVersion = ClientMachine::getDeviceDriverVersion ();
	int const driverSubversion = ClientMachine::getDeviceDriverSubversion ();
	int const driverBuild = ClientMachine::getDeviceDriverBuild();

	if (vendor == 0x10de)
	{
		//NVidia tests
		if (driverBuild >= 2700 && driverBuild < 2800)
		{
			MessageBox2 messageBox("The application has detected very old NVidia video card drivers that have known issues that will cause a crash.\nPlease upgrade your video drivers.  They may be downloaded from:\n\n\thttp://www.nvidia.com.\n\nWould you like to continue running anyway?");
			messageBox.setOkayButton("Continue");
			messageBox.setCancelButton("Quit");
			messageBox.setWebButton("Go to web page", "http://www.nvidia.com");
			messageBox.setDoNotShowAgainCheckBox("Do not show this warning again", "NVidia 2700");
			if (messageBox.DoModal() == IDCANCEL)
				return false;
		}
		else if ((device >= 0x0200 && device <= 0x020F) && driverBuild == 5216)
		{
			MessageBox2 messageBox("The application has detected a card/driver combination that has known issues that cause a crash.\nPlease upgrade your video drivers.  They may be downloaded from:\n\n\thttp://www.nvidia.com.\n\nWould you like to continue running anyway?");
			messageBox.setOkayButton("Continue");
			messageBox.setCancelButton("Quit");
			messageBox.setWebButton("Go to web page", "http://www.nvidia.com");
			messageBox.setDoNotShowAgainCheckBox("Do not show this warning again", "NVidia 5216");
			if (messageBox.DoModal() == IDCANCEL)
				return false;
		}
	}
	else if (vendor == 0x1002)
	{
		// ATI tests
		if (driverBuild <= 6467)
		{
			MessageBox2 messageBox("The application has detected old ATI video card drivers which have known issues that will cause a crash.\nFor more information, please see:\n\n\thttp://starwarsgalaxies.station.sony.com/content.jsp?page=ATI%20Video%20Card%20Driver\n\nWould you like to continue running anyway?");
			messageBox.setOkayButton("Continue");
			messageBox.setCancelButton("Quit");
			messageBox.setWebButton("Go to web page", "http://starwarsgalaxies.station.sony.com/content.jsp?page=ATI%20Video%20Card%20Driver");
			messageBox.setDoNotShowAgainCheckBox("Do not show this warning again", "ATI Catalyst 4.8");
			if (messageBox.DoModal() == IDCANCEL)
				return false;
		}
	}

		if (!registryKeyExists (cms_sendHardwareInformationRegistryKey))
			configure ();

		Options::save ();

                const std::uint64_t physicalMemorySize = ClientMachine::getPhysicalMemorySize ();

                bool const displayMessage =
                        (physicalMemorySize < 500) ||
                        (ClientMachine::getCpuSpeed () < 900) ||
                        (ClientMachine::getVideoMemorySize() < 28);
		if (displayMessage)
		{
			int const machineRequirementsDisplayCount = getRegistryKey(cms_machineRequirementsDisplayCountRegistryKey);
			if (machineRequirementsDisplayCount < 3)
			{
				CString os = _T("- Windows 98SE/ME/2000/XP");
				if (os == _T("unsupported"))
					os += _T(" (detected unsupported)\n");
				else
					os += _T("\n");

				CString memory;
				VERIFY(memory.LoadString(IDS_512MB_PHYSICAL));

				CString memoryDetected;
				VERIFY(memoryDetected.LoadString(IDS_512MB_PHYSICAL_DETECTED));
                                if (physicalMemorySize < 500)
                                        memory.Format (memoryDetected, static_cast<unsigned __int64>(physicalMemorySize));

				CString cpu;
				VERIFY(cpu.LoadString(IDS_900MHZ_PROCESSOR));
				if (ClientMachine::getCpuSpeed () < 900)
				{
					if (ClientMachine::getCpuSpeed() == 0)
					{
						CString detectedUnknown;
						VERIFY(detectedUnknown.LoadString(IDS_900MHZ_PROCESSOR_DETECTED_UNKNOWN));
						cpu.Format (detectedUnknown);
					}
					else
					{
						CString detectedCpu;
						VERIFY(detectedCpu.LoadString(IDS_900MHZ_PROCESSOR_DETECTED));
						cpu.Format (detectedCpu, ClientMachine::getCpuSpeed ());
					}
				}

				CString videoMemory;
				VERIFY(videoMemory.LoadString(IDS_32MB_VIDEO_MEMORY));
				CString videoMemoryTooSmall;
				VERIFY(videoMemoryTooSmall.LoadString(IDS_32MB_VIDEO_MEMORY_DETECTED));				
                                if (ClientMachine::getVideoMemorySize () < 28)
                                        videoMemory.Format (videoMemoryTooSmall, static_cast<int>(ClientMachine::getVideoMemorySize ()));

				CString harshMessage;
				VERIFY(harshMessage.LoadString(IDS_NOT_MINIMUM));
				CString niceMessage;
				VERIFY(niceMessage.LoadString(IDS_NICE_MINIMUM));
                                bool const displayHarshMessage =
                                        (hasJumpToLightspeed(AfxGetApp()->m_lpCmdLine) && physicalMemorySize < 500) ||
                                        (!hasJumpToLightspeed(AfxGetApp()->m_lpCmdLine) && physicalMemorySize < 250) ||
                                        (ClientMachine::getCpuSpeed () < 900) ||
                                        (ClientMachine::getVideoMemorySize() < 28);

				CString message(displayHarshMessage ? harshMessage : niceMessage);
				message += os;
				message += memory;
				message += cpu;
				message += videoMemory;
				CString finalMessageStr;
				VERIFY(finalMessageStr.LoadString(IDS_FINAL_CONFIG));
				message += finalMessageStr;
				
				CString noChangeStr;
				VERIFY(noChangeStr.LoadString(IDS_NO_CHANGE_DETECTED));
				if (machineRequirementsDisplayCount == 2)
					message += noChangeStr;

				setRegistryKey(cms_machineRequirementsDisplayCountRegistryKey, machineRequirementsDisplayCount + 1);

				AfxMessageBox (message, NULL, MB_OK | MB_ICONSTOP);
			}
		}
		else
			setRegistryKey(cms_machineRequirementsDisplayCountRegistryKey, 0);

		//-- spawn cms_applicationName
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory (&si, sizeof(si));
		si.cb = sizeof (si);
		ZeroMemory (&pi, sizeof(pi));

		TCHAR commandLine [1024];
		_stprintf_s (commandLine, _countof(commandLine), _T("%s %s"), cms_applicationName, AfxGetApp()->m_lpCmdLine);
		BOOL const result = CreateProcess (cms_applicationName, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		if (!result)
		{
			CString error;
			CString failedStr;
			VERIFY(failedStr.LoadString(IDS_FAILED_START));
			error.Format (failedStr, cms_applicationName);
			AfxMessageBox (error, NULL, MB_ICONERROR | MB_OK);
		}

		if (semaphore)
		{
			CloseHandle(semaphore);
			semaphore = NULL;
		}

		return FALSE;
	}

	if (!ClientMachine::getDirectXSupported ())
	{
		CString directXStr;
		VERIFY(directXStr.LoadString(IDS_NO_DIRECTX));
		AfxMessageBox (directXStr);
		return FALSE;
	}

	SwgClientSetupDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		if (!Options::save ())
		{
			CString buffer;
			CString cantSaveStr;
			VERIFY(cantSaveStr.LoadString(IDS_CANT_SAVE));
			buffer.Format (cantSaveStr, Options::getFileName ());
			AfxMessageBox (buffer);
		}
	}
	else if (nResponse == IDCANCEL)
	{
	}

	if (semaphore)
		CloseHandle(semaphore);

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

// ======================================================================

//int callBlat(int argc, char **argv, char **envp);

// ----------------------------------------------------------------------

void SwgClientSetupNamespace::sendMail(std::string const & to, std::string const & from, std::string const & subject, std::string const & body, std::vector<std::string> const & attachments)
{
	int const numAttachments = attachments.size();

	const int static_args = 13;

	int argc = static_args + (2 * numAttachments);
	char** argv = new char*[argc];
	int argv_value = 1;
	argv[argv_value++] = "-to";
	argv[argv_value++] = const_cast<char*>(to.c_str());
	argv[argv_value++] = "-subject";
	argv[argv_value++] = const_cast<char*>(subject.c_str());
	argv[argv_value++] = "-smtphost";
	argv[argv_value++] = "mail.station.sony.com";
	argv[argv_value++] = "-port";
	argv[argv_value++] = "2525";
	argv[argv_value++] = "-f";
	argv[argv_value++] = const_cast<char*>(from.c_str());
	argv[argv_value++] = "-body";
	argv[argv_value++] = const_cast<char*>(body.c_str());

	//iterate through all attachments, building the argv entries for them
	for(std::vector<std::string>::const_iterator i = attachments.begin(); i != attachments.end(); ++i)
	{
		argv[argv_value++] = "-attach";
		argv[argv_value++] = const_cast<char*>(i->c_str());
	}

	//this doesn't get used, but make sure to send initialized memory anyway
	char* envp = new char[256];
	memset(envp, 0, 256);

	//call into blat to send the mail
	//callBlat(argc, argv, &envp);

	//clean up allocated memory
	delete[] argv;
	delete[] envp;
}

// ----------------------------------------------------------------------

void SwgClientSetupApp::configure ()
{
	bool const oldSendMinidumps = getSendMinidumps ();
	bool const oldSendStationId = Options::getSendStationId ();
	bool const oldAutomaticallySendHardwareInformation = getAutomaticallySendHardwareInformation ();
	bool const oldAllowCustomerContact = Options::getAllowCustomerContact ();

	//-- ask to send minidumps
	DialogMinidump dialogMinidump;
	bool cancelled = dialogMinidump.DoModal () == IDCANCEL;

	//-- if the user did not cancel and they want to send minidumps
	if (!cancelled)
	{
		if (getSendMinidumps ())
		{
			DialogStationId dialogStationId;
			cancelled = dialogStationId.DoModal () == IDCANCEL;

			//-- if the user did not cancel, ask to send hardware and contact information
			if (!cancelled)
			{
				if (Options::getSendStationId ())
				{
					DialogHardwareInformation dialogHardwareInformation;
					cancelled = dialogHardwareInformation.DoModal () == IDCANCEL;

					if (!cancelled)
					{
						DialogContact dialogContact;
						cancelled = dialogContact.DoModal () == IDCANCEL;
					}
				}
				else
				{
					setAutomaticallySendHardwareInformation (false);
					Options::setAllowCustomerContact (false);
				}
			}

			if (!cancelled)
			{
				DialogFinish dialogFinish;
				dialogFinish.DoModal ();
			}
		}
		else
		{
			Options::setSendStationId (false);
			setAutomaticallySendHardwareInformation (false);
			Options::setAllowCustomerContact (false);
		}
	}

	if (cancelled)
	{
		setSendMinidumps (oldSendMinidumps);
		Options::setSendStationId (oldSendStationId);
		setAutomaticallySendHardwareInformation (oldAutomaticallySendHardwareInformation);
		Options::setAllowCustomerContact (oldAllowCustomerContact);
	}
}

// ----------------------------------------------------------------------



// ----------------------------------------------------------------------



// ----------------------------------------------------------------------

TCHAR const * SwgClientSetupApp::getSendMinidumpsString ()
{
	CString sendMinidumpsString;
	VERIFY(sendMinidumpsString.LoadString(IDS_PLEASE_SEND_LOG));
	return sendMinidumpsString;	
}

// ----------------------------------------------------------------------

bool SwgClientSetupApp::getSendMinidumps ()
{
	return getRegistryKey (cms_sendMinidumpsRegistryKey) != 0;
}

// ----------------------------------------------------------------------

void SwgClientSetupApp::setSendMinidumps (bool const sendMinidumps)
{
	setRegistryKey (cms_sendMinidumpsRegistryKey, sendMinidumps ? 1 : 0);
}

// ----------------------------------------------------------------------

TCHAR const * SwgClientSetupApp::getSendStationIdString ()
{
	CString str;
	VERIFY(str.LoadString(IDS_SEND_STATION_ID));
	return str;
}

// ----------------------------------------------------------------------

TCHAR const * SwgClientSetupApp::getAutomaticallySendHardwareInformationString ()
{
	CString str;
	VERIFY(str.LoadString(IDS_AUTOMATICALLY_SEND_HARDWARE));
	return str;
}

// ----------------------------------------------------------------------
bool SwgClientSetupApp::getAutomaticallySendHardwareInformation ()
{
	return getRegistryKey (cms_sendHardwareInformationRegistryKey) != 0;
}

// ----------------------------------------------------------------------

void SwgClientSetupApp::setAutomaticallySendHardwareInformation (bool const automaticallySendHardwareInformation)
{
	setRegistryKey (cms_sendHardwareInformationRegistryKey, automaticallySendHardwareInformation ? 1 : 0);
}

// ----------------------------------------------------------------------

TCHAR const * SwgClientSetupApp::getAllowContactString ()
{
	CString str;
	VERIFY(str.LoadString(IDS_ALLOW_CONTACT));
	return str;
}

// ----------------------------------------------------------------------

TCHAR const * SwgClientSetupApp::getThankYouString ()
{
	CString str;
	VERIFY(str.LoadString(IDS_THANK_YOU));
	return str;
}

// ======================================================================
