#pragma once

//----------------------------------------------------------------------
//
// GuidedCreationView
// High level guidance dashboard for the modern terrain editor. Displays
// AI-driven metrics, quick actions and blueprint suggestions.
//
//----------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "SmartTerrainAnalyzer.h"
#include "Resource.h"
#include <afxcmn.h>

class TerrainEditorDoc;

class GuidedCreationView : public CFormView
{
        DECLARE_DYNCREATE(GuidedCreationView)

protected:
        GuidedCreationView();
        virtual ~GuidedCreationView();

public:
        enum { IDD = IDD_GUIDED_CREATION_VIEW };

        TerrainEditorDoc *getDocument() const;

        virtual void DoDataExchange(CDataExchange *pDX);
        virtual void OnInitialUpdate();
        virtual void OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint);

private:
        CStatic         m_overviewHeader;
        CStatic         m_metricsText;
        CProgressCtrl   m_structureProgress;
        CProgressCtrl   m_ecosystemProgress;
        CProgressCtrl   m_workflowProgress;
        CListCtrl       m_insightList;
        CListCtrl       m_blueprintList;
        CListCtrl       m_quickActionList;
        CButton         m_refreshButton;
        CButton         m_overlayToggle;
        CButton         m_heatmapToggle;
        CButton         m_guidelineToggle;

private:
        void refreshFromDocument();
        void updateProgressBars(float structureScore, float ecosystemScore, float workflowScore);
        void populateInsights(const SmartTerrainAnalyzer::AuditReport &report);
        void populateBlueprint(const SmartTerrainAnalyzer::AuditReport &report);
        void populateQuickActions(const SmartTerrainAnalyzer::AuditReport &report);
        void syncToggleState();

private:
        afx_msg void OnRefreshClicked();
        afx_msg void OnToggleOverlay();
        afx_msg void OnToggleHeatmap();
        afx_msg void OnToggleGuidelines();
        afx_msg void OnQuickActionActivated(NMHDR *pNMHDR, LRESULT *pResult);

        DECLARE_MESSAGE_MAP()
};

//----------------------------------------------------------------------
