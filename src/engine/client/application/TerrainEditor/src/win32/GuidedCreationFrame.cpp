#include "FirstTerrainEditor.h"
#include "GuidedCreationFrame.h"

#include "GuidedCreationView.h"
#include "TerrainEditor.h"
#include "TerrainEditorDoc.h"

//----------------------------------------------------------------------

IMPLEMENT_DYNCREATE(GuidedCreationFrame, CMDIChildWnd)

//----------------------------------------------------------------------

GuidedCreationFrame::GuidedCreationFrame()
:       CMDIChildWnd()
{
}

//----------------------------------------------------------------------

GuidedCreationFrame::~GuidedCreationFrame()
{
}

//----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(GuidedCreationFrame, CMDIChildWnd)
        ON_WM_CREATE()
        ON_WM_DESTROY()
END_MESSAGE_MAP()

//----------------------------------------------------------------------

int GuidedCreationFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
        if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
                return -1;

        TerrainEditorApp *const app = static_cast<TerrainEditorApp*>(AfxGetApp());
        if (!app->RestoreWindowPosition(this, "GuidedCreationFrame"))
        {
                CRect mainRect;
                AfxGetApp()->GetMainWnd()->GetClientRect(&mainRect);
                mainRect.right  -= 2;
                mainRect.bottom -= 96;
                IGNORE_RETURN(SetWindowPos(&wndTop, mainRect.right - (mainRect.right/3), 0,
                        (mainRect.right/3), mainRect.bottom / 2, SWP_SHOWWINDOW));
        }

        return 0;
}

//----------------------------------------------------------------------

void GuidedCreationFrame::OnDestroy()
{
        TerrainEditorApp *const app = static_cast<TerrainEditorApp*>(AfxGetApp());
        app->SaveWindowPosition(this, "GuidedCreationFrame");

        CMDIChildWnd::OnDestroy();

        TerrainEditorDoc *const document = static_cast<TerrainEditorDoc*>(GetActiveDocument());
        if (document)
                document->setGuidedCreationFrame(0);
}

//----------------------------------------------------------------------

BOOL GuidedCreationFrame::PreCreateWindow(CREATESTRUCT &cs)
{
        cs.lpszName = _T("Guided Creation");
        cs.style &= ~static_cast<LONG>(FWS_ADDTOTITLE | FWS_PREFIXTITLE);
        return CMDIChildWnd::PreCreateWindow(cs);
}

//----------------------------------------------------------------------
