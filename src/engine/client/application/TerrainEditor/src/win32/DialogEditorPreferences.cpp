//
// DialogEditorPreferences.cpp
// asommers 
//
// copyright 2000, sony online entertainment
//

//-------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "DialogEditorPreferences.h"

//-------------------------------------------------------------------

DialogEditorPreferences::DialogEditorPreferences(CWnd* pParent /*=NULL*/) : 
	CDialog(DialogEditorPreferences::IDD, pParent),
	m_tooltip (),
	m_layoutInitialized (false),

	//-- widgets
	m_maxHeight (0.f),
	m_minHeight (0.f)
{
	//{{AFX_DATA_INIT(DialogEditorPreferences)
	m_maxHeight = 0.0f;
	m_minHeight = 0.0f;
	//}}AFX_DATA_INIT
}

//-------------------------------------------------------------------

void DialogEditorPreferences::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(DialogEditorPreferences)
	DDX_Text(pDX, IDC_EDIT_MAXHEIGHT, m_maxHeight);
	DDX_Text(pDX, IDC_EDIT_MINHEIGHT, m_minHeight);
	//}}AFX_DATA_MAP
}

//-------------------------------------------------------------------

BEGIN_MESSAGE_MAP(DialogEditorPreferences, CDialog)
	//{{AFX_MSG_MAP(DialogEditorPreferences)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
END_MESSAGE_MAP()

//-------------------------------------------------------------------

BOOL DialogEditorPreferences::PreTranslateMessage(MSG* pMsg)
{
	// CG: The following block was added by the ToolTips component.
	{
		// Let the ToolTip process this message.
		m_tooltip.RelayEvent(pMsg);
	}
	return CDialog::PreTranslateMessage(pMsg);	// CG: This was added by the ToolTips component.
}

//-------------------------------------------------------------------

BOOL DialogEditorPreferences::OnInitDialog()
{
	CDialog::OnInitDialog();	// CG: This was added by the ToolTips component.
	// CG: The following block was added by the ToolTips component.
	{
		// Create the ToolTip control.
		IGNORE_RETURN (m_tooltip.Create(this));
		m_tooltip.Activate(TRUE);

		// TODO: Use one of the following forms to add controls:
		// m_tooltip.AddTool(GetDlgItem(IDC_<name>), <string-table-id>);
		// m_tooltip.AddTool(GetDlgItem(IDC_<name>), "<text>");
	}

	captureInitialLayout();
	m_layoutInitialized = true;

	return TRUE;	// CG: This was added by the ToolTips component.
}

//-------------------------------------------------------------------

void DialogEditorPreferences::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	applyLayout(cx, cy);
}

//-------------------------------------------------------------------

void DialogEditorPreferences::captureInitialLayout()
{
	m_controlLayout.clear();

	CRect rect;
	GetClientRect(&rect);
	m_initialClientSize = rect.Size();

	EnumChildWindows(m_hWnd, &DialogEditorPreferences::storeControlLayout,
		reinterpret_cast<LPARAM>(this));
}

//-------------------------------------------------------------------

void DialogEditorPreferences::applyLayout(int cx, int cy)
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

BOOL CALLBACK DialogEditorPreferences::storeControlLayout(HWND hwnd, LPARAM lParam)
{
	DialogEditorPreferences* dialog = reinterpret_cast<DialogEditorPreferences*>(lParam);
	if (!dialog)
		return FALSE;

	CRect rect;
	::GetWindowRect(hwnd, &rect);
	dialog->ScreenToClient(&rect);

	ControlLayout layout = { hwnd, rect };
	dialog->m_controlLayout.push_back(layout);

	return TRUE;
}

//-------------------------------------------------------------------
