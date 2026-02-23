// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__7FF23841_3837_4FAA_BBCB_6B25D0E321FE__INCLUDED_)
#define AFX_MAINFRM_H__7FF23841_3837_4FAA_BBCB_6B25D0E321FE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ChildView.h"
#include "MenuTipper.h"
#include "EditorViews.h"
#include "DesignAssistant.h"

#define USE_TREE_PROPERTY_DIALOG 0

// ---------------------------------------------

class UIDirect3DPrimaryCanvas;
class UIPage;
class ObjectEditor;
class ObjectFactory;
class DefaultObjectPropertiesManager;
class ObjectBrowserDialog;
class ObjectPropertiesDialog;
class ObjectPropertiesTreeDialog;

// ---------------------------------------------

#include "UIUtils.h"
#include "UIBaseObject.h"

class CMainFrame : public CFrameWnd, public EditorViews
{
	
public:
        enum ThemeStyle
        {
                ThemeStyleNeoDark = 0,
                ThemeStyleLuminousLight = 1,
                ThemeStyleBlueprint = 2,
                ThemeStyleAurora = 3,
                ThemeStyleSolar = 4,
                ThemeStyleCarbon = 5,
                ThemeStyleNordic = 6
        };

        CMainFrame();
protected:
        DECLARE_DYNAMIC(CMainFrame)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

	void CalcWindowRect(PRECT prcClient, UINT nAdjustType);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	ObjectEditor *getEditor() { return m_editor; }
	bool isEditing() const { return m_editor!=0; }

	bool openFile(const char *i_fileName);

	virtual void setCursor(CursorType showType);
	virtual void setCapture();
	virtual void releaseCapture();
	virtual void redrawViews(bool synchronous);
	virtual void copyObjectBuffersToClipboard(const std::list<SerializedObjectBuffer> &i_buffers);
	virtual bool pasteObjectBuffersFromClipboard(std::list<SerializedObjectBuffer> &o_buffers);
	virtual void onRootSize(int width, int height);

	// --------------------------------------------

protected:  // control bar embedded members

#if USE_TREE_PROPERTY_DIALOG
	typedef ObjectPropertiesTreeDialog PropertiesDialog;
#else
	typedef ObjectPropertiesDialog PropertiesDialog;
#endif

	void _resize(int desiredClientWidth, int desiredClientHeight);

	void _createEditingObjects();
	void _destroyEditingObjects();

        void _createNewWorkspace();
        bool _openWorkspaceFile(const char *Filename);
        bool _saveWorkspaceFile(const char *fileName);
        bool _closeWorkspaceFile();
        void _unloadObjects();
        bool _getSaveFileName(CString &o_filename);
        void _setMainWindowTitle();
        void _setActiveAppearance(bool i_showActive);
        void _loadModernPreferences();
        void _applyTheme(ThemeStyle style, bool persist);
        void _drawSafeZonesOverlay(long viewportWidth, long viewportHeight);
        void _drawLayoutGuidesOverlay(const UIRect &bounds, const UIPoint &canvasTranslation, long canvasWidth, long canvasHeight);
        void _renderFocusModeOverlay(const UIRect &selectionBounds, bool hasSelection, const UIPoint &canvasTranslation, long canvasWidth, long canvasHeight);
        CString _buildStatusMetrics();
        CString _getThemeDisplayName() const;

        ObjectBrowserDialog *_getObjectBrowserDialog();
        PropertiesDialog *_getObjectPropertiesDialog();

	bool _objectBrowserIsFocus();
	bool _objectPropertiesIsFocus();

	void _drawBoxOutline(UIRect box, UIColor color);

	void _renderUI();

	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CReBar      m_wndReBar;
	CDialogBar  m_wndDlgBar;
	CChildView  m_wndView;

	UINarrowString                  m_fileName;
	UINarrowString                  gInitialDirectory;
	ObjectEditor                   *m_editor;
	ObjectBrowserDialog            *m_browserDialog;
	PropertiesDialog               *m_propertiesDialog;
	ObjectFactory                  *m_factory;
	DefaultObjectPropertiesManager *m_defaultsManager;
	DesignAssistant                 m_designAssistant;
	
	UIPage *gCurrentlySelectedPage;
	bool gVersionFilename;
	bool g_showShaders;
	bool m_inVisualEditingMode;
	bool gDrawCursor;
	bool gDrawGrid;
	bool m_drawSelectionRect;
        bool m_showActive;
        bool m_browserDialogOpen;
        bool m_propertiesDialogOpen;
        unsigned long m_refreshTimer;
        UIColor gGridColor;
        unsigned long gGridMajorTicks;
        long gTriangleCount;
        long gFlushCount;
        long gFrameCount;
        bool m_showLayoutGuides;
        bool m_showSafeZones;
        bool m_showStatusMetrics;
        bool m_showAssistantOverlay;
        bool m_focusMode;
        ThemeStyle m_activeTheme;
        UIColor m_canvasBackgroundColor;
        UIColor m_layoutGuideColor;
        UIColor m_safeZoneColor;
        UIColor m_focusOverlayColor;
        float m_safeZoneHorizontalPadding;
        float m_safeZoneVerticalPadding;

        CMenuTipManager m_menuTipManager;

        static UINT m_clipboardFormat;

	HCURSOR m_cursors[CT_COUNT];

	static UIColor m_highlightOutlineColor;
	static UIColor m_selectionOutlineColor;
	static UIColor m_highlightSelectionOutlineColor;
	static UIColor m_selectionFillColor;
	static UIColor m_selectionBoxOutlineColor;
	static UIColor m_selectionDragBoxOutlineColor;

// Generated message map functions
protected:
	afx_msg void OnInsertObject(UINT nId);
	afx_msg void OnUpdateInsertObject(CCmdUI* pCmdUI);
	afx_msg LRESULT OnPaintChild(WPARAM, LPARAM);
	afx_msg LRESULT closePropertiesDialog(WPARAM, LPARAM);
	afx_msg LRESULT closeObjectBrowserDialog(WPARAM, LPARAM);
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnDestroy();
	afx_msg void OnFileNew();
	afx_msg void OnUpdateFileNew(CCmdUI* pCmdUI);
	afx_msg void OnFileOpen();
	afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
	afx_msg void OnFileClose();
	afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);
	afx_msg void OnFileSave();
	afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
	afx_msg void OnFileSaveas();
	afx_msg void OnUpdateFileSaveas(CCmdUI* pCmdUI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnEditCut();
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnEditPaste();
        afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
        afx_msg void OnViewDefaultProperties();
        afx_msg void OnUpdateViewDefaultProperties(CCmdUI* pCmdUI);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnSelectionBurrow();
	afx_msg void OnUpdateSelectionBurrow(CCmdUI* pCmdUI);
	afx_msg void OnSelectionClearall();
	afx_msg void OnUpdateSelectionClearall(CCmdUI* pCmdUI);
	afx_msg void OnSelectionDescendants();
	afx_msg void OnUpdateSelectionDescendants(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAncestors();
	afx_msg void OnUpdateSelectionAncestors(CCmdUI* pCmdUI);
	afx_msg void OnEditCanceldrag();
	afx_msg void OnUpdateEditCanceldrag(CCmdUI* pCmdUI);
	afx_msg void OnSelectionDelete();
	afx_msg void OnUpdateSelectionDelete(CCmdUI* pCmdUI);
	afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
	afx_msg void OnToggleGrid();
	afx_msg void OnUpdateToggleGrid(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAlignbottom();
	afx_msg void OnUpdateSelectionAlignbottom(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAlignleft();
	afx_msg void OnUpdateSelectionAlignleft(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAlignright();
	afx_msg void OnUpdateSelectionAlignright(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAligntop();
	afx_msg void OnUpdateSelectionAligntop(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAlignwidth();
	afx_msg void OnUpdateSelectionAlignwidth(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAlignheight();
	afx_msg void OnUpdateSelectionAlignheight(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAligncenterx();
	afx_msg void OnUpdateSelectionAligncenterx(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAligncentery();
	afx_msg void OnUpdateSelectionAligncentery(CCmdUI* pCmdUI);
	afx_msg void OnSelectionSmartupscale();
	afx_msg void OnUpdateSelectionSmartupscale(CCmdUI* pCmdUI);
	afx_msg void OnSelectionAutoupscale();
	afx_msg void OnUpdateSelectionAutoupscale(CCmdUI* pCmdUI);
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
	afx_msg void OnCheckout();
	afx_msg void OnUpdateCheckout(CCmdUI* pCmdUI);
	afx_msg void OnViewObjectbrowser();
	afx_msg void OnUpdateViewObjectbrowser(CCmdUI* pCmdUI);
	afx_msg void OnViewSelectionproperties();
        afx_msg void OnUpdateViewSelectionproperties(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeDark();
        afx_msg void OnUpdateViewThemeDark(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeLight();
        afx_msg void OnUpdateViewThemeLight(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeBlueprint();
        afx_msg void OnUpdateViewThemeBlueprint(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeAurora();
        afx_msg void OnUpdateViewThemeAurora(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeSolar();
        afx_msg void OnUpdateViewThemeSolar(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeCarbon();
        afx_msg void OnUpdateViewThemeCarbon(CCmdUI* pCmdUI);
        afx_msg void OnViewThemeNordic();
        afx_msg void OnUpdateViewThemeNordic(CCmdUI* pCmdUI);
        afx_msg void OnViewToggleLayoutguides();
        afx_msg void OnUpdateViewToggleLayoutguides(CCmdUI* pCmdUI);
        afx_msg void OnViewToggleSafezones();
        afx_msg void OnUpdateViewToggleSafezones(CCmdUI* pCmdUI);
        afx_msg void OnViewToggleStatusmetrics();
        afx_msg void OnUpdateViewToggleStatusmetrics(CCmdUI* pCmdUI);
        afx_msg void OnViewToggleAssistant();
        afx_msg void OnUpdateViewToggleAssistant(CCmdUI* pCmdUI);
        afx_msg void OnViewToggleFocusmode();
        afx_msg void OnUpdateViewToggleFocusmode(CCmdUI* pCmdUI);
        afx_msg void OnToolsAutolayoutResponsive();
        afx_msg void OnUpdateToolsAutolayoutResponsive(CCmdUI* pCmdUI);
        afx_msg void OnToolsBalancepadding();
        afx_msg void OnUpdateToolsBalancepadding(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__7FF23841_3837_4FAA_BBCB_6B25D0E321FE__INCLUDED_)
