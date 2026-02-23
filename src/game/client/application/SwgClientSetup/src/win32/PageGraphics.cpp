// ======================================================================
//
// PageGraphics.cpp
// asommers
//
// copyright 2002, sony online entertainment
//
// ======================================================================

#include "FirstSwgClientSetup.h"
#include "PageGraphics.h"

#include "ClientMachine.h"
#include "Options.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// ======================================================================

namespace PageGraphicsNamespace
{
	enum GraphicsPreset
	{
		GP_auto,
		GP_ultra,
		GP_high,
		GP_balanced,
		GP_performance,
		GP_compatibility
	};
}

using namespace PageGraphicsNamespace;

// ======================================================================

IMPLEMENT_DYNCREATE(PageGraphics, CPropertyPage)

PageGraphics::PageGraphics() : CPropertyPage(PageGraphics::IDD)
{
	//{{AFX_DATA_INIT(PageGraphics)
	m_lblGameResolution = _T("");
	m_lblPixelShaderVersion = _T("");

	m_lblWindowedMode = _T("");
	m_lblBorderlessWindow = _T("");
	m_lblDisableBumpMapping = _T("");
	m_lblDisableHardwareMouseControl = _T("");
	m_lblUseLowDetailTextures = _T("");
	m_lblUseLowDetailNormalMaps = _T("");
	m_lblDisableMultipassRendering = _T("");
	m_lblDisableVsync = _T("");
	m_lblDisableFastMouseCursor = _T("");
	m_lblUseSafeRenderer = _T("");
	m_lblGraphicsPreset = _T("");

	m_windowed = FALSE;
	m_borderlessWindow = FALSE;
	m_disableBumpMapping = FALSE;
	m_disableHardwareMouseCursor = FALSE;
	m_useLowDetailTextures = FALSE;
	m_useLowDetailNormalMaps = FALSE;
	m_disableMultipassRendering = FALSE;
	m_disableVsync = FALSE;
	m_disableFastMouseCursor = FALSE;
	m_useSafeRenderer = FALSE;
	//}}AFX_DATA_INIT
}

PageGraphics::~PageGraphics()
{
}

void PageGraphics::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(PageGraphics)

	DDX_Text(pDX, IDC_LBL_GRAPH_GAME_RESOLUTION, m_lblGameResolution);
	DDX_Text(pDX, IDC_GRAPH_LBL_PIXEL_SHADER_VERSION, m_lblPixelShaderVersion);

	DDX_Text(pDX, IDC_CHECK_WINDOWED_MODE, m_lblWindowedMode);
	DDX_Text(pDX, IDC_CHECK_BORDERLESSWINDOW, m_lblBorderlessWindow);
	DDX_Text(pDX, IDC_CHECK_DISABLEBUMP, m_lblDisableBumpMapping);
	DDX_Text(pDX, IDC_CHECK_DISABLEHARDWAREMOUSECURSOR, m_lblDisableHardwareMouseControl);
	DDX_Text(pDX, IDC_CHECK_USE_LOW_DETAIL_TEXTURES, m_lblUseLowDetailTextures);
	DDX_Text(pDX, IDC_CHECK_USE_LOW_DETAIL_NORMAL_MAPS, m_lblUseLowDetailNormalMaps);
        DDX_Text(pDX, IDC_CHECK_DISABLE_MULTIPASS_RENDERING, m_lblDisableMultipassRendering);
        DDX_Text(pDX, IDC_CHECK_DISABLE_VSYNC, m_lblDisableVsync);
	DDX_Text(pDX, IDC_CHECK_DISABLE_FAST_MOUSE_CURSOR, m_lblDisableFastMouseCursor);
	DDX_Text(pDX, IDC_CHECK_USE_SAFE_RENDERER, m_lblUseSafeRenderer);
        DDX_Text(pDX, IDC_STATIC_GRAPHICS_RECOMMENDATION, m_graphicsRecommendationText);
	DDX_Text(pDX, IDC_LBL_GRAPHICS_PRESET, m_lblGraphicsPreset);

	DDX_Control(pDX, IDC_RESOLUTION, m_resolution);
	DDX_Control(pDX, IDC_VERTEXPIXELSHADERVERSION, m_vertexPixelShaderVersion);
	DDX_Control(pDX, IDC_GRAPHICS_PRESET, m_graphicsPreset);
	
	DDX_Check(pDX, IDC_CHECK_WINDOWED_MODE, m_windowed);
	DDX_Check(pDX, IDC_CHECK_BORDERLESSWINDOW, m_borderlessWindow);
    DDX_Control(pDX, IDC_CHECK_DISABLEBUMP, m_disableBumpMappingButton);
	DDX_Check(pDX,   IDC_CHECK_DISABLEBUMP, m_disableBumpMapping);
    DDX_Control(pDX, IDC_CHECK_DISABLEHARDWAREMOUSECURSOR, m_disableHardwareMouseCursorButton);
	DDX_Check(pDX,   IDC_CHECK_DISABLEHARDWAREMOUSECURSOR, m_disableHardwareMouseCursor);
	DDX_Check(pDX, IDC_CHECK_USE_LOW_DETAIL_TEXTURES, m_useLowDetailTextures);
	DDX_Check(pDX, IDC_CHECK_USE_LOW_DETAIL_NORMAL_MAPS, m_useLowDetailNormalMaps);
	DDX_Check(pDX, IDC_CHECK_DISABLE_MULTIPASS_RENDERING, m_disableMultipassRendering);
	DDX_Check(pDX, IDC_CHECK_DISABLE_VSYNC, m_disableVsync);
	DDX_Check(pDX, IDC_CHECK_DISABLE_FAST_MOUSE_CURSOR, m_disableFastMouseCursor);
	DDX_Check(pDX, IDC_CHECK_USE_SAFE_RENDERER, m_useSafeRenderer);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(PageGraphics, CPropertyPage)
	//{{AFX_MSG_MAP(PageGraphics)
	ON_CBN_SELCHANGE(IDC_RESOLUTION, OnSelchangeResolution)
	ON_CBN_SELCHANGE(IDC_VERTEXPIXELSHADERVERSION, OnSelchangeVertexpixelshaderversion)
	ON_CBN_SELCHANGE(IDC_GRAPHICS_PRESET, OnSelchangeGraphicspreset)

	ON_BN_CLICKED(IDC_CHECK_WINDOWED_MODE, OnCheckWindowed)
	ON_BN_CLICKED(IDC_CHECK_BORDERLESSWINDOW, OnCheckBorderlesswindow)
	ON_BN_CLICKED(IDC_CHECK_DISABLEBUMP, OnCheckDisablebump)
	ON_BN_CLICKED(IDC_CHECK_DISABLEHARDWAREMOUSECURSOR, OnCheckDisablehardwaremousecursor)
	ON_BN_CLICKED(IDC_CHECK_USE_LOW_DETAIL_TEXTURES, OnCheckUselowdetailtextures)
	ON_BN_CLICKED(IDC_CHECK_USE_LOW_DETAIL_NORMAL_MAPS, OnCheckUselowdetailnormalmaps)
	ON_BN_CLICKED(IDC_CHECK_DISABLE_MULTIPASS_RENDERING, OnCheckDisablemultipassrendering)
	ON_BN_CLICKED(IDC_CHECK_DISABLE_VSYNC, OnCheckDisablevsync)
	ON_BN_CLICKED(IDC_CHECK_DISABLE_FAST_MOUSE_CURSOR, OnCheckDisablefastmousecursor)
	ON_BN_CLICKED(IDC_CHECK_USE_SAFE_RENDERER, OnCheckUsesaferenderer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// ======================================================================

void PageGraphics::initializeDialog() 
{

        VERIFY(m_lblGameResolution.LoadString(IDS_GRAPHICS_GAME_RESOLUTION));
        VERIFY(m_lblPixelShaderVersion.LoadString(IDS_GRAPHICS_PIXEL_SHADER_VERSION));
        VERIFY(m_lblWindowedMode.LoadString(IDS_GRAPHICS_WINDOWED_MODE));
        VERIFY(m_lblBorderlessWindow.LoadString(IDS_GRAPHICS_BORDERLESS_WINDOW));
        VERIFY(m_lblDisableBumpMapping.LoadString(IDS_GRAPHICS_DISABLE_BUMP_MAPPING));
        VERIFY(m_lblDisableHardwareMouseControl.LoadString(IDS_GRAPHICS_DISABLE_HARDWARE_MOUSE_CURSOR));
        VERIFY(m_lblUseLowDetailTextures.LoadString(IDS_GRAPHICS_USE_LOW_DETAIL_TEXTURES));
        VERIFY(m_lblUseLowDetailNormalMaps.LoadString(IDS_GRAPHICS_USE_LOW_DETAIL_NORMAL_MAPS));
        VERIFY(m_lblDisableMultipassRendering.LoadString(IDS_GRAPHICS_DISABLE_MULTIPASS_RENDERING));
        VERIFY(m_lblDisableVsync.LoadString(IDS_GRAPHICS_DISABLE_VSYNC));
        VERIFY(m_lblDisableFastMouseCursor.LoadString(IDS_GRAPHICS_DISABLE_FAST_MOUSE_CURSOR));
        VERIFY(m_lblUseSafeRenderer.LoadString(IDS_GRAPHICS_USE_SAFE_RENDERER));
        VERIFY(m_lblGraphicsPreset.LoadString(IDS_GRAPHICS_PRESET));
        VERIFY(m_lblVspsOptimal.LoadString(IDS_VSPS_OPTIMAL));
        VERIFY(m_lblVsps40.LoadString(IDS_VSPS_4_0));
        VERIFY(m_lblVsps30.LoadString(IDS_VSPS_3_0));
        VERIFY(m_lblVsps20.LoadString(IDS_VSPS_2_0));
        VERIFY(m_lblVsps14.LoadString(IDS_VSPS_1_4));
	VERIFY(m_lblVsps11.LoadString(IDS_VSPS_1_1));
	VERIFY(m_lblVspsDisabled.LoadString(IDS_VSPS_DISABLED));
	VERIFY(m_lblPresetAuto.LoadString(IDS_GRAPHICS_PRESET_AUTO));
	VERIFY(m_lblPresetUltra.LoadString(IDS_GRAPHICS_PRESET_ULTRA));
	VERIFY(m_lblPresetHigh.LoadString(IDS_GRAPHICS_PRESET_HIGH));
	VERIFY(m_lblPresetBalanced.LoadString(IDS_GRAPHICS_PRESET_BALANCED));
	VERIFY(m_lblPresetPerformance.LoadString(IDS_GRAPHICS_PRESET_PERFORMANCE));
	VERIFY(m_lblPresetCompatibility.LoadString(IDS_GRAPHICS_PRESET_COMPATIBILITY));

	
	UpdateData (false);
	
}
// ======================================================================

BOOL PageGraphics::OnSetActive( )
{
	BOOL const result = CPropertyPage::OnSetActive();
	initializeDialog();
	return result;
}

// ======================================================================

BOOL PageGraphics::OnInitDialog() 
{
	BOOL const result = CPropertyPage::OnInitDialog();

	initializeDialog();

	//-- resolutions
	int const screenWidth = Options::getScreenWidth ();
	int const screenHeight = Options::getScreenHeight ();
	int const refreshRate = Options::getFullScreenRefreshRate ();

	//-- select the appropriate refresh rate
	{
		int selection = 0;

		for (int i = 0; i < ClientMachine::getNumberOfDisplayModes (); ++i)
		{
			int const displayModeWidth = ClientMachine::getDisplayModeWidth (i);
			int const displayModeHeight = ClientMachine::getDisplayModeHeight (i);
			int const displayModeRefreshRate = ClientMachine::getDisplayModeRefreshRate (i);

			TCHAR buffer [256];
			_stprintf_s (buffer, _countof(buffer), _T("%i x %i @ %i Hz"), displayModeWidth, displayModeHeight, displayModeRefreshRate);

			m_resolution.AddString (buffer);

			if (screenWidth == displayModeWidth && screenHeight == displayModeHeight && refreshRate == displayModeRefreshRate)
				selection = i;
		}

		if (selection < ClientMachine::getNumberOfDisplayModes ())
			m_resolution.SetCurSel (selection);
	}

        //-- vertex and pixel shader version
        {
                m_vertexPixelShaderVersion.ResetContent ();
                int selection = 0;
                int pixelShaderMajorVersion = Options::getPixelShaderMajorVersion ();
                int pixelShaderMinorVersion = Options::getPixelShaderMinorVersion ();
                bool const supportsShaderModel40 = ClientMachine::supportsDirectX10() || ClientMachine::supportsDirectX12();
                if ((ClientMachine::getPixelShaderMajorVersion () > 0) && (ClientMachine::getPhysicalMemorySize() > 260))
                {
                        m_vertexPixelShaderVersion.EnableWindow (true);
                        int item = m_vertexPixelShaderVersion.AddString (m_lblVspsOptimal);
                        m_vertexPixelShaderVersion.SetItemData (item, static_cast<DWORD_PTR>(-1));

                        int const supportedPixelShaderMajorVersion = ClientMachine::getPixelShaderMajorVersion ();
                        int const supportedPixelShaderMinorVersion = ClientMachine::getPixelShaderMinorVersion ();

                        if (supportsShaderModel40)
                        {
                                item = m_vertexPixelShaderVersion.AddString(m_lblVsps40);
                                m_vertexPixelShaderVersion.SetItemData(item, 40);
                        }

                        if (supportedPixelShaderMajorVersion >= 3)
                        {
                                item = m_vertexPixelShaderVersion.AddString (m_lblVsps30);
                                m_vertexPixelShaderVersion.SetItemData (item, 30);
                        }

                        if (supportedPixelShaderMajorVersion >= 2)
                        {
                                item = m_vertexPixelShaderVersion.AddString (m_lblVsps20);
                                m_vertexPixelShaderVersion.SetItemData (item, 20);
                        }

                        if (supportedPixelShaderMajorVersion > 1 || (supportedPixelShaderMajorVersion == 1 && supportedPixelShaderMinorVersion >= 4))
                        {
                                item = m_vertexPixelShaderVersion.AddString (m_lblVsps14);
                                m_vertexPixelShaderVersion.SetItemData (item, 14);
                        }

                        if (supportedPixelShaderMajorVersion >= 1)
                        {
                                item = m_vertexPixelShaderVersion.AddString (m_lblVsps11);
                                m_vertexPixelShaderVersion.SetItemData (item, 11);
                        }
                }
                else
                {
                        Options::setPixelShaderVersion(0, 0);
                        pixelShaderMajorVersion = 0;
                        pixelShaderMinorVersion = 0;
                        m_vertexPixelShaderVersion.EnableWindow (false);
                }

                int const item = m_vertexPixelShaderVersion.AddString (m_lblVspsDisabled);
                m_vertexPixelShaderVersion.SetItemData (item, 0);

                DWORD_PTR desiredData;
                if (pixelShaderMajorVersion < 0)
                        desiredData = static_cast<DWORD_PTR>(-1);
                else if (pixelShaderMajorVersion == 0)
                        desiredData = 0;
                else
                        desiredData = static_cast<DWORD_PTR>(pixelShaderMajorVersion * 10 + pixelShaderMinorVersion);

                int const itemCount = m_vertexPixelShaderVersion.GetCount ();
                for (int i = 0; i < itemCount; ++i)
                {
                        if (m_vertexPixelShaderVersion.GetItemData (i) == desiredData)
                        {
                                selection = i;
                                break;
                        }
                }

                m_vertexPixelShaderVersion.SetCurSel (selection);
        }

	//-- smart preset
	{
		m_graphicsPreset.ResetContent();
		int item = m_graphicsPreset.AddString(m_lblPresetAuto);
		m_graphicsPreset.SetItemData(item, GP_auto);
		item = m_graphicsPreset.AddString(m_lblPresetUltra);
		m_graphicsPreset.SetItemData(item, GP_ultra);
		item = m_graphicsPreset.AddString(m_lblPresetHigh);
		m_graphicsPreset.SetItemData(item, GP_high);
		item = m_graphicsPreset.AddString(m_lblPresetBalanced);
		m_graphicsPreset.SetItemData(item, GP_balanced);
		item = m_graphicsPreset.AddString(m_lblPresetPerformance);
		m_graphicsPreset.SetItemData(item, GP_performance);
		item = m_graphicsPreset.AddString(m_lblPresetCompatibility);
		m_graphicsPreset.SetItemData(item, GP_compatibility);

		m_graphicsPreset.SetCurSel(0);
	}

	//-- hardware mouse cursor
	m_disableHardwareMouseCursor = ClientMachine::getSupportsHardwareMouseCursor () ? Options::getDisableHardwareMouseCursor () : TRUE;
	m_disableHardwareMouseCursorButton.EnableWindow (ClientMachine::getSupportsHardwareMouseCursor ());

	//-- other options
	m_disableBumpMapping = Options::getDisableBumpMapping ();

	m_windowed = Options::getWindowed ();
	m_borderlessWindow = Options::getBorderlessWindow ();

	m_useLowDetailTextures = Options::getDiscardHighestMipMapLevels ();
	m_useLowDetailNormalMaps = Options::getDiscardHighestNormalMipMapLevels ();

	m_disableMultipassRendering = Options::getDisableMultipassRendering ();
	m_disableVsync = Options::getAllowTearing ();

        m_disableFastMouseCursor = Options::getDisableFastMouseCursor ();
        m_useSafeRenderer = Options::getUseSafeRenderer ();

        Options::GraphicsRecommendation const recommendation = Options::getGraphicsRecommendation();
        if (recommendation.usingModel)
        {
                m_disableBumpMapping = recommendation.disableBumpMapping;
                m_useLowDetailTextures = recommendation.discardHighestMipMapLevels;
                m_useLowDetailNormalMaps = recommendation.discardHighestNormalMipMapLevels;
                m_disableMultipassRendering = recommendation.disableMultipassRendering;

                if (recommendation.qualityTier.CompareNoCase(_T("low")) == 0)
                {
                        m_disableBumpMappingButton.EnableWindow(false);
                        GetDlgItem(IDC_CHECK_USE_LOW_DETAIL_TEXTURES)->EnableWindow(FALSE);
                        GetDlgItem(IDC_CHECK_USE_LOW_DETAIL_NORMAL_MAPS)->EnableWindow(FALSE);
                }
        }
        else
        {
                if (ClientMachine::getPhysicalMemorySize() < 260)
                {
                        // legacy fallback: disable bump mapping and high mips on extremely low memory
                        Options::setDisableBumpMapping(true);
                        m_disableBumpMapping = true;
                        m_disableBumpMappingButton.EnableWindow(false);

                        Options::setDiscardHighestMipMapLevels(true);
                        Options::setDiscardHighestNormalMipMapLevels(true);
                }
                else if (ClientMachine::getPhysicalMemorySize() < 400)
                {
                        Options::setDisableBumpMapping(true);
                        m_disableBumpMapping = true;

                        Options::setDiscardHighestMipMapLevels(true);
                        Options::setDiscardHighestNormalMipMapLevels(true);
                }
        }

	refreshRecommendationText(m_lblPresetAuto, true);

        UpdateData(FALSE);

        return result;
}

// ----------------------------------------------------------------------

void PageGraphics::applyOptions ()
{
	UpdateData (true);

	int const selection = m_resolution.GetCurSel ();
	if (selection != CB_ERR)
	{
		Options::setScreenWidth (ClientMachine::getDisplayModeWidth (selection));
		Options::setScreenHeight (ClientMachine::getDisplayModeHeight (selection));
		Options::setFullScreenRefreshRate (ClientMachine::getDisplayModeRefreshRate (selection));
	}

        switch (m_vertexPixelShaderVersion.GetItemData (m_vertexPixelShaderVersion.GetCurSel ()))
        {
        case -1:
                Options::setPixelShaderVersion (-1, -1);
                break;

        case 30:
                Options::setPixelShaderVersion (3, 0);
                break;

        case 40:
                Options::setPixelShaderVersion (4, 0);
                break;

        case 11:
                Options::setPixelShaderVersion (1, 1);
                break;

        case 14:
                Options::setPixelShaderVersion (1, 4);
                break;

        case 20:
                Options::setPixelShaderVersion (2, 0);
                break;

        case 0:
        default:
                Options::setPixelShaderVersion (0, 0);
                break;
        }

	Options::setWindowed (m_windowed == TRUE);
	Options::setBorderlessWindow (m_borderlessWindow == TRUE);
	Options::setDisableBumpMapping (m_disableBumpMapping == TRUE);
	Options::setDisableHardwareMouseCursor (m_disableHardwareMouseCursor == TRUE);
	Options::setDiscardHighestMipMapLevels (m_useLowDetailTextures == TRUE);
	Options::setDiscardHighestNormalMipMapLevels (m_useLowDetailNormalMaps == TRUE);
	Options::setDisableMultipassRendering (m_disableMultipassRendering == TRUE);
	Options::setAllowTearing (m_disableVsync == TRUE);
	Options::setDisableFastMouseCursor (m_disableFastMouseCursor == TRUE);
	Options::setUseSafeRenderer (m_useSafeRenderer == TRUE);
}

// ----------------------------------------------------------------------

void PageGraphics::syncOptionsFromState()
{
	m_disableHardwareMouseCursor = ClientMachine::getSupportsHardwareMouseCursor () ? Options::getDisableHardwareMouseCursor () : TRUE;
	m_disableHardwareMouseCursorButton.EnableWindow (ClientMachine::getSupportsHardwareMouseCursor ());

	m_disableBumpMapping = Options::getDisableBumpMapping ();

	m_windowed = Options::getWindowed ();
	m_borderlessWindow = Options::getBorderlessWindow ();

	m_useLowDetailTextures = Options::getDiscardHighestMipMapLevels ();
	m_useLowDetailNormalMaps = Options::getDiscardHighestNormalMipMapLevels ();

	m_disableMultipassRendering = Options::getDisableMultipassRendering ();
	m_disableVsync = Options::getAllowTearing ();

	m_disableFastMouseCursor = Options::getDisableFastMouseCursor ();
	m_useSafeRenderer = Options::getUseSafeRenderer ();
}

// ----------------------------------------------------------------------

void PageGraphics::refreshRecommendationText(CString const &presetLabel, bool isAutoPreset)
{
	Options::GraphicsRecommendation const recommendation = Options::getGraphicsRecommendation();

	m_graphicsRecommendationText = _T("Preset: ") + presetLabel;

	if (recommendation.usingModel)
	{
		m_graphicsRecommendationText += _T("\r\nAuto recommendation: ");
		m_graphicsRecommendationText += recommendation.qualityTier;
	}
	else
	{
		m_graphicsRecommendationText += _T("\r\nAuto recommendation: legacy defaults");
	}

	if (!recommendation.rationale.IsEmpty())
	{
		CString rationale = recommendation.rationale;
		rationale.Replace(_T("\n"), _T("\r\n"));
		m_graphicsRecommendationText += _T("\r\n");
		m_graphicsRecommendationText += rationale;
	}

	if (!isAutoPreset)
	{
		m_graphicsRecommendationText += _T("\r\nManual preset applied.");
	}
}

// ----------------------------------------------------------------------

void PageGraphics::applyGraphicsPreset(int preset)
{
	CString presetLabel = m_lblPresetAuto;
	const bool isAutoPreset = preset == GP_auto;

	switch (preset)
	{
	case GP_ultra:
		presetLabel = m_lblPresetUltra;
		Options::setPixelShaderVersion(-1, -1);
		Options::setDisableBumpMapping(false);
		Options::setDiscardHighestMipMapLevels(false);
		Options::setDiscardHighestNormalMipMapLevels(false);
		Options::setDisableMultipassRendering(false);
		Options::setDisableTextureBaking(false);
		Options::setAllowTearing(false);
		Options::setDisableFastMouseCursor(false);
		Options::setUseSafeRenderer(false);
		Options::setDisableHardwareMouseCursor(false);
		break;
	case GP_high:
		presetLabel = m_lblPresetHigh;
		Options::setPixelShaderVersion(-1, -1);
		Options::setDisableBumpMapping(false);
		Options::setDiscardHighestMipMapLevels(false);
		Options::setDiscardHighestNormalMipMapLevels(false);
		Options::setDisableMultipassRendering(false);
		Options::setDisableTextureBaking(false);
		Options::setAllowTearing(false);
		Options::setDisableFastMouseCursor(false);
		Options::setUseSafeRenderer(false);
		Options::setDisableHardwareMouseCursor(false);
		break;
	case GP_balanced:
		presetLabel = m_lblPresetBalanced;
		Options::setPixelShaderVersion(-1, -1);
		Options::setDisableBumpMapping(false);
		Options::setDiscardHighestMipMapLevels(false);
		Options::setDiscardHighestNormalMipMapLevels(false);
		Options::setDisableMultipassRendering(false);
		Options::setDisableTextureBaking(false);
		Options::setAllowTearing(false);
		Options::setDisableFastMouseCursor(false);
		Options::setUseSafeRenderer(false);
		Options::setDisableHardwareMouseCursor(false);
		break;
	case GP_performance:
		presetLabel = m_lblPresetPerformance;
		Options::setPixelShaderVersion(-1, -1);
		Options::setDisableBumpMapping(true);
		Options::setDiscardHighestMipMapLevels(true);
		Options::setDiscardHighestNormalMipMapLevels(true);
		Options::setDisableMultipassRendering(true);
		Options::setDisableTextureBaking(true);
		Options::setAllowTearing(true);
		Options::setDisableFastMouseCursor(false);
		Options::setUseSafeRenderer(false);
		Options::setDisableHardwareMouseCursor(false);
		break;
	case GP_compatibility:
		presetLabel = m_lblPresetCompatibility;
		Options::setPixelShaderVersion(0, 0);
		Options::setDisableBumpMapping(true);
		Options::setDiscardHighestMipMapLevels(true);
		Options::setDiscardHighestNormalMipMapLevels(true);
		Options::setDisableMultipassRendering(true);
		Options::setDisableTextureBaking(true);
		Options::setAllowTearing(false);
		Options::setDisableFastMouseCursor(true);
		Options::setUseSafeRenderer(true);
		Options::setDisableHardwareMouseCursor(true);
		break;
	case GP_auto:
	default:
		presetLabel = m_lblPresetAuto;
		Options::applyGraphicsAutoPreset();
		break;
	}

	syncOptionsFromState();

	Options::GraphicsRecommendation const recommendation = Options::getGraphicsRecommendation();
	const bool isLowAuto = isAutoPreset && recommendation.usingModel &&
		recommendation.qualityTier.CompareNoCase(_T("low")) == 0;
	m_disableBumpMappingButton.EnableWindow(!isLowAuto);
	GetDlgItem(IDC_CHECK_USE_LOW_DETAIL_TEXTURES)->EnableWindow(!isLowAuto);
	GetDlgItem(IDC_CHECK_USE_LOW_DETAIL_NORMAL_MAPS)->EnableWindow(!isLowAuto);

	const int pixelShaderMajorVersion = Options::getPixelShaderMajorVersion();
	const int pixelShaderMinorVersion = Options::getPixelShaderMinorVersion();
	DWORD_PTR desiredData;
	if (pixelShaderMajorVersion < 0)
		desiredData = static_cast<DWORD_PTR>(-1);
	else if (pixelShaderMajorVersion == 0)
		desiredData = 0;
	else
		desiredData = static_cast<DWORD_PTR>(pixelShaderMajorVersion * 10 + pixelShaderMinorVersion);

	const int itemCount = m_vertexPixelShaderVersion.GetCount();
	for (int i = 0; i < itemCount; ++i)
	{
		if (m_vertexPixelShaderVersion.GetItemData(i) == desiredData)
		{
			m_vertexPixelShaderVersion.SetCurSel(i);
			break;
		}
	}

	refreshRecommendationText(presetLabel, isAutoPreset);
	UpdateData(FALSE);
}

// ----------------------------------------------------------------------

void PageGraphics::OnSelchangeResolution() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnSelchangeVertexpixelshaderversion() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckWindowed() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckBorderlesswindow() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckDisablebump() 
{
	applyOptions ();	
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckDisablehardwaremousecursor() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckUselowdetailtextures() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckUselowdetailnormalmaps() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckDisablemultipassrendering() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckDisablevsync() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckDisablefastmousecursor() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnCheckUsesaferenderer() 
{
	applyOptions ();
}

// ----------------------------------------------------------------------

void PageGraphics::OnSelchangeGraphicspreset()
{
	const int selection = m_graphicsPreset.GetCurSel();
	if (selection == CB_ERR)
		return;

	const int preset = static_cast<int>(m_graphicsPreset.GetItemData(selection));
	applyGraphicsPreset(preset);
}

// ----------------------------------------------------------------------

BOOL PageGraphics::OnKillActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	applyOptions ();
	
	return CPropertyPage::OnKillActive();
}

// ======================================================================
