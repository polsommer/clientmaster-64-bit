//
// FormLayer.h
// asommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#ifndef INCLUDED_FormLayer_H
#define INCLUDED_FormLayer_H

//-------------------------------------------------------------------

#include "FormLayerItem.h"
#include <vector>

//-------------------------------------------------------------------

class FormLayer : public FormLayerItem
{
private:

	TerrainGenerator::Layer* layer;

private:

	//{{AFX_DATA(FormLayer)
	enum { IDD = IDD_FORM_LAYER };
	CString	m_name;
	BOOL	m_invertBoundaries;
	BOOL	m_invertFilters;
	CString	m_notes;
	//}}AFX_DATA

protected:

	FormLayer (void);           
	DECLARE_DYNCREATE(FormLayer)
	virtual ~FormLayer();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	//{{AFX_MSG(FormLayer)
	afx_msg void OnChangeEditNotes();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:

	virtual void Initialize (PropertyView::ViewData* vd);
	virtual bool HasChanged () const;
	virtual void ApplyChanges ();

	//{{AFX_VIRTUAL(FormLayer)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

private:

	struct ControlLayout
	{
		HWND hwnd;
		CRect rect;
	};

private:

	void captureInitialLayout();
	void applyLayout(int cx, int cy);
	static BOOL CALLBACK storeControlLayout(HWND hwnd, LPARAM lParam);

private:

	std::vector<ControlLayout> m_controlLayout;
	CSize m_initialClientSize;
	bool m_layoutInitialized;
};

//-------------------------------------------------------------------

//{{AFX_INSERT_LOCATION}}

//-------------------------------------------------------------------

#endif 
