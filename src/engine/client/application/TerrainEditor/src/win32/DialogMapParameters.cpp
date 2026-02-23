//
// DialogMapParameters.cpp
// asommers 10-9-2000
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "DialogMapParameters.h"

#include "RecentDirectory.h"
#include "sharedUtility/FileName.h"
#include "TerrainEditorDoc.h"

//-------------------------------------------------------------------

DialogMapParameters::DialogMapParameters(CWnd* pParent /*=NULL*/) : 
	CDialog(DialogMapParameters::IDD, pParent),
	shaderSize (0),
	mapWidth (0),
	chunkWidth (0),
	tilesPerChunk (0),
	m_layoutInitialized (false),
	
	//-- widgets
	m_shaderSize (),
	m_tilesPerChunk (),
	m_mapWidth (),
	m_chunkWidth (),
	m_useGlobalWater (FALSE),
	m_globalWaterHeight (0.f),
	m_globalWaterShader (),
	m_globalWaterShaderSize (0.f)
{
	//{{AFX_DATA_INIT(DialogMapParameters)
	m_useGlobalWater = FALSE;
	m_globalWaterHeight = 0.0f;
	m_globalWaterShader = _T("");
	m_globalWaterShaderSize = 0.0f;
	m_heightPoleDistance = 0;
	m_tileSize = 0;
	m_hours = 0;
	m_minutes = 0;
	m_legacyMap = FALSE;
	//}}AFX_DATA_INIT
}

//-------------------------------------------------------------------

void DialogMapParameters::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(DialogMapParameters)
	DDX_Control(pDX, IDC_COMBO_SHADER_SIZE, m_shaderSize);
	DDX_Control(pDX, IDC_COMBO_TILES_PER_CHUNK, m_tilesPerChunk);
	DDX_Control(pDX, IDC_COMBO_MAPWIDTH, m_mapWidth);
	DDX_Control(pDX, IDC_COMBO_CHUNK_WIDTH, m_chunkWidth);
	DDX_Check(pDX, IDC_CHECK_USEGLOBALWATER, m_useGlobalWater);
	DDX_Text(pDX, IDC_EDIT_GLOBALWATERHEIGHT, m_globalWaterHeight);
	DDX_Text(pDX, IDC_EDIT_GLOBALWATERSHADER, m_globalWaterShader);
	DDX_Text(pDX, IDC_EDIT_GLOBALWATERSHADERSIZE, m_globalWaterShaderSize);
	DDV_MinMaxFloat(pDX, m_globalWaterShaderSize, 2.f, 100.f);
	DDX_Text(pDX, IDC_EDIT_HEIGHTPOLEDISTANCE, m_heightPoleDistance);
	DDX_Text(pDX, IDC_EDIT_TILESIZE, m_tileSize);
	DDX_Text(pDX, IDC_EDIT_HOURS, m_hours);
	DDX_Text(pDX, IDC_EDIT_MINUTES, m_minutes);
	DDX_Check(pDX, IDC_EDIT_LEGACYMAP, m_legacyMap);
	//}}AFX_DATA_MAP
}

//-------------------------------------------------------------------

BEGIN_MESSAGE_MAP(DialogMapParameters, CDialog)
	//{{AFX_MSG_MAP(DialogMapParameters)
	ON_BN_CLICKED(IDC_BUTTON_GLOBALWATERSHADERBROWSE, OnButtonGlobalwatershaderbrowse)
	ON_BN_CLICKED(IDC_CHECK_USEGLOBALWATER, OnCheckUseglobalwater)
	ON_CBN_SELCHANGE(IDC_COMBO_CHUNK_WIDTH, OnSelchangeComboChunkWidth)
	ON_CBN_SELCHANGE(IDC_COMBO_TILES_PER_CHUNK, OnSelchangeComboTilesPerChunk)
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
END_MESSAGE_MAP()

//-------------------------------------------------------------------

BOOL DialogMapParameters::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	IGNORE_RETURN (m_shaderSize.SetCurSel (shaderSize));
	IGNORE_RETURN (m_mapWidth.SetCurSel (mapWidth));
	IGNORE_RETURN (m_chunkWidth.SetCurSel (chunkWidth));
	IGNORE_RETURN (m_tilesPerChunk.SetCurSel (tilesPerChunk));

	recalculate ();

	captureInitialLayout();
	m_layoutInitialized = true;

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//-------------------------------------------------------------------

void DialogMapParameters::OnOK (void)
{
	shaderSize    = m_shaderSize.GetCurSel ();
	mapWidth      = m_mapWidth.GetCurSel ();
	chunkWidth    = m_chunkWidth.GetCurSel ();
	tilesPerChunk = m_tilesPerChunk.GetCurSel ();

	CDialog::OnOK ();
}

//-------------------------------------------------------------------

void DialogMapParameters::OnButtonGlobalwatershaderbrowse() 
{
	CFileDialog dlg (true, "*.sht", 0, (OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR));
	dlg.m_ofn.lpstrInitialDir = RecentDirectory::find ("ShaderTemplate_Water");
	dlg.m_ofn.lpstrTitle      = "Select Shader Template File";

	if (dlg.DoModal () == IDOK)
	{
		IGNORE_RETURN (RecentDirectory::update ("ShaderTemplate_Water", dlg.GetPathName ()));

		FileName filename (dlg.GetPathName ());
		filename.stripPathAndExt ();

		m_globalWaterShader = filename;

		IGNORE_RETURN (UpdateData (false));
	}
}

//-------------------------------------------------------------------

void DialogMapParameters::OnCheckUseglobalwater() 
{
	IGNORE_RETURN (UpdateData (true));
}

//-------------------------------------------------------------------

void DialogMapParameters::recalculate ()
{
	UpdateData (true);

	int chunkWidthInMeters = 0;

	switch (m_chunkWidth.GetCurSel ())
	{
	case TerrainEditorParameters::CW_32_32:  chunkWidthInMeters = 32;  break;
	case TerrainEditorParameters::CW_16_16:  chunkWidthInMeters = 16;  break;
	default:
	case TerrainEditorParameters::CW_8_8:    chunkWidthInMeters = 8;  break;
	case TerrainEditorParameters::CW_4_4:    chunkWidthInMeters = 4;  break;
	}

	int numberOfTilesPerChunk = 1;

	switch (m_tilesPerChunk.GetCurSel ())
	{
	case TerrainEditorParameters::NOTPC_16: numberOfTilesPerChunk = 16; break;
	case TerrainEditorParameters::NOTPC_8:  numberOfTilesPerChunk = 8;  break;
	default:
	case TerrainEditorParameters::NOTPC_4:  numberOfTilesPerChunk = 4;  break;
	case TerrainEditorParameters::NOTPC_2:  numberOfTilesPerChunk = 2;  break;
	case TerrainEditorParameters::NOTPC_1:  numberOfTilesPerChunk = 1;  break;
	}

	m_tileSize           = chunkWidthInMeters / numberOfTilesPerChunk;
	m_heightPoleDistance = m_tileSize / 2;

	UpdateData (false);
}

//-------------------------------------------------------------------

void DialogMapParameters::OnSelchangeComboChunkWidth() 
{
	recalculate ();
}

//-------------------------------------------------------------------

void DialogMapParameters::OnSelchangeComboTilesPerChunk() 
{
	recalculate ();
}

//-------------------------------------------------------------------

void DialogMapParameters::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	applyLayout(cx, cy);
}

//-------------------------------------------------------------------

void DialogMapParameters::captureInitialLayout()
{
	m_controlLayout.clear();

	CRect rect;
	GetClientRect(&rect);
	m_initialClientSize = rect.Size();

	EnumChildWindows(m_hWnd, &DialogMapParameters::storeControlLayout,
		reinterpret_cast<LPARAM>(this));
}

//-------------------------------------------------------------------

void DialogMapParameters::applyLayout(int cx, int cy)
{
	if (!m_layoutInitialized || m_initialClientSize.cx == 0 || m_initialClientSize.cy == 0)
		return;

	const double scaleX = static_cast<double>(cx) / static_cast<double>(m_initialClientSize.cx);
	const double scaleY = static_cast<double>(cy) / static_cast<double>(m_initialClientSize.cy);

	for (const ControlLayout& layout : m_controlLayout)
	{
		const int left = static_cast<int>(layout.rect.left * scaleX + 0.5);
		const int top = static_cast<int>(layout.rect.top * scaleY + 0.5);
		const int right = static_cast<int>(layout.rect.right * scaleX + 0.5);
		const int bottom = static_cast<int>(layout.rect.bottom * scaleY + 0.5);

		::MoveWindow(layout.hwnd, left, top, right - left, bottom - top, TRUE);
	}
}

//-------------------------------------------------------------------

BOOL CALLBACK DialogMapParameters::storeControlLayout(HWND hwnd, LPARAM lParam)
{
	DialogMapParameters* dialog = reinterpret_cast<DialogMapParameters*>(lParam);
	if (!dialog)
		return FALSE;

	CRect rect;
	::GetWindowRect(hwnd, &rect);
	dialog->ScreenToClient(&rect);

	ControlLayout layout = { hwnd, rect };
	dialog->m_controlLayout.push_back(layout);

	return TRUE;
}

//===================================================================
