#pragma once

//----------------------------------------------------------------------
//
// GuidedCreationFrame
// Hosts the GuidedCreationView docked window for the modernised terrain
// authoring workflow.
//
//----------------------------------------------------------------------

#include "FirstTerrainEditor.h"

class GuidedCreationFrame : public CMDIChildWnd
{
        DECLARE_DYNCREATE(GuidedCreationFrame)

public:
        GuidedCreationFrame();
        virtual ~GuidedCreationFrame();

protected:
        virtual BOOL PreCreateWindow(CREATESTRUCT &cs);

protected:
        afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
        afx_msg void OnDestroy();

        DECLARE_MESSAGE_MAP()
};

//----------------------------------------------------------------------
