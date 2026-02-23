#include "FirstTerrainEditor.h"
#include "GuidedCreationView.h"

#include "GuidedCreationFrame.h"
#include "TerrainEditorDoc.h"
#include "TerrainEditor.h"
#include "MapFrame.h"
#include "MapView.h"

//----------------------------------------------------------------------

IMPLEMENT_DYNCREATE(GuidedCreationView, CFormView)

//----------------------------------------------------------------------

GuidedCreationView::GuidedCreationView()
:       CFormView(GuidedCreationView::IDD)
{
}

//----------------------------------------------------------------------

GuidedCreationView::~GuidedCreationView()
{
}

//----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(GuidedCreationView, CFormView)
        ON_BN_CLICKED(IDC_GUIDANCE_REFRESH, OnRefreshClicked)
        ON_BN_CLICKED(IDC_GUIDANCE_TOGGLE_OVERLAY, OnToggleOverlay)
        ON_BN_CLICKED(IDC_GUIDANCE_TOGGLE_HEATMAP, OnToggleHeatmap)
        ON_BN_CLICKED(IDC_GUIDANCE_TOGGLE_GUIDES, OnToggleGuidelines)
        ON_NOTIFY(NM_DBLCLK, IDC_GUIDANCE_QUICKACTION_LIST, OnQuickActionActivated)
END_MESSAGE_MAP()

//----------------------------------------------------------------------

void GuidedCreationView::DoDataExchange(CDataExchange *pDX)
{
        CFormView::DoDataExchange(pDX);
        DDX_Control(pDX, IDC_GUIDANCE_HEADER, m_overviewHeader);
        DDX_Control(pDX, IDC_GUIDANCE_METRICS, m_metricsText);
        DDX_Control(pDX, IDC_GUIDANCE_STRUCTURE_PROGRESS, m_structureProgress);
        DDX_Control(pDX, IDC_GUIDANCE_ECOSYSTEM_PROGRESS, m_ecosystemProgress);
        DDX_Control(pDX, IDC_GUIDANCE_WORKFLOW_PROGRESS, m_workflowProgress);
        DDX_Control(pDX, IDC_GUIDANCE_INSIGHT_LIST, m_insightList);
        DDX_Control(pDX, IDC_GUIDANCE_BLUEPRINT_LIST, m_blueprintList);
        DDX_Control(pDX, IDC_GUIDANCE_QUICKACTION_LIST, m_quickActionList);
        DDX_Control(pDX, IDC_GUIDANCE_REFRESH, m_refreshButton);
        DDX_Control(pDX, IDC_GUIDANCE_TOGGLE_OVERLAY, m_overlayToggle);
        DDX_Control(pDX, IDC_GUIDANCE_TOGGLE_HEATMAP, m_heatmapToggle);
        DDX_Control(pDX, IDC_GUIDANCE_TOGGLE_GUIDES, m_guidelineToggle);
}

//----------------------------------------------------------------------

TerrainEditorDoc *GuidedCreationView::getDocument() const
{
        return static_cast<TerrainEditorDoc*>(m_pDocument);
}

//----------------------------------------------------------------------

void GuidedCreationView::OnInitialUpdate()
{
        CFormView::OnInitialUpdate();

        m_structureProgress.SetRange(0, 100);
        m_ecosystemProgress.SetRange(0, 100);
        m_workflowProgress.SetRange(0, 100);

        const DWORD extendedStyle = LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER;

        m_insightList.SetExtendedStyle(m_insightList.GetExtendedStyle() | extendedStyle);
        m_insightList.InsertColumn(0, _T("Insight"), LVCFMT_LEFT, 220);
        m_insightList.InsertColumn(1, _T("Confidence"), LVCFMT_RIGHT, 90);
        m_insightList.InsertColumn(2, _T("Detail"), LVCFMT_LEFT, 360);

        m_blueprintList.SetExtendedStyle(m_blueprintList.GetExtendedStyle() | extendedStyle);
        m_blueprintList.InsertColumn(0, _T("Action"), LVCFMT_LEFT, 220);
        m_blueprintList.InsertColumn(1, _T("Impact"), LVCFMT_RIGHT, 90);
        m_blueprintList.InsertColumn(2, _T("Rationale"), LVCFMT_LEFT, 360);

        m_quickActionList.SetExtendedStyle(m_quickActionList.GetExtendedStyle() | extendedStyle);
        m_quickActionList.InsertColumn(0, _T("Helper"), LVCFMT_LEFT, 220);
        m_quickActionList.InsertColumn(1, _T("Description"), LVCFMT_LEFT, 430);

        syncToggleState();
        refreshFromDocument();
}

//----------------------------------------------------------------------

void GuidedCreationView::OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint)
{
        UNREF(pSender);
        UNREF(lHint);
        UNREF(pHint);

        refreshFromDocument();
}

//----------------------------------------------------------------------

void GuidedCreationView::refreshFromDocument()
{
        TerrainEditorDoc *const document = getDocument();
        if (!document)
                return;

        const SmartTerrainAnalyzer::AuditReport report = SmartTerrainAnalyzer::analyze(*document);

        CString header;
        header.Format(_T("Foresight %0.1f/100 | Structure %0.1f | Ecosystem %0.1f | Workflow %0.1f"),
                report.foresightScore,
                report.structureScore,
                report.ecosystemScore,
                report.workflowScore);
        m_overviewHeader.SetWindowText(header);

        CString metrics;
        metrics.Format(_T("Layers: %d total (%d active) • Boundaries %d • Filters %d • Affectors %d\r\n")
                _T("Libraries: shader %d • flora %d • radial %d • environment %d • fractal %d • bitmap %d\r\n")
                _T("Environment: cycle %d minutes • water table %s"),
                report.totalLayers,
                report.activeLayers,
                report.totalBoundaries,
                report.totalFilters,
                report.totalAffectors,
                report.shaderFamilies,
                report.floraFamilies,
                report.radialFamilies,
                report.environmentFamilies,
                report.fractalFamilies,
                report.bitmapFamilies,
                report.environmentCycleMinutes,
                report.hasGlobalWaterTable ? _T("enabled") : _T("disabled"));

        if (document->hasAutoGenerationResult())
        {
                const TerrainEditorDoc::AutoGenerationResult &autoGen = document->getAutoGenerationResult();
                metrics += _T("\r\nAutoSynth: ") + autoGen.report.blueprintSummary;
                if (!autoGen.report.aiStatusHeadline.IsEmpty())
                        metrics += _T("\r\nAI Status: ") + autoGen.report.aiStatusHeadline;
                if (!autoGen.report.operationsChecklist.IsEmpty())
                        metrics += _T("\r\nOps: ") + autoGen.report.operationsChecklist;
        }
        m_metricsText.SetWindowText(metrics);

        updateProgressBars(report.structureScore, report.ecosystemScore, report.workflowScore);
        populateInsights(report);
        populateBlueprint(report);
        populateQuickActions(report);
        syncToggleState();
}

//----------------------------------------------------------------------

void GuidedCreationView::updateProgressBars(float structureScore, float ecosystemScore, float workflowScore)
{
        m_structureProgress.SetPos(static_cast<int>(structureScore + 0.5f));
        m_ecosystemProgress.SetPos(static_cast<int>(ecosystemScore + 0.5f));
        m_workflowProgress.SetPos(static_cast<int>(workflowScore + 0.5f));
}

//----------------------------------------------------------------------

static CString formatPercentString(float value)
{
        CString text;
        text.Format(_T("%0.0f%%"), value);
        return text;
}

//----------------------------------------------------------------------

void GuidedCreationView::populateInsights(const SmartTerrainAnalyzer::AuditReport &report)
{
        m_insightList.DeleteAllItems();

        for (size_t i = 0; i < report.insights.size(); ++i)
        {
                const SmartTerrainAnalyzer::Insight &insight = report.insights[i];
                const int item = m_insightList.InsertItem(static_cast<int>(i), insight.headline);
                m_insightList.SetItemText(item, 1, formatPercentString(insight.confidence * 100.0f));
                m_insightList.SetItemText(item, 2, insight.detail);
        }

        if (report.insights.empty())
        {
                const int row = m_insightList.InsertItem(0, _T("All clear"));
                m_insightList.SetItemText(row, 2, _T("No blocking insights detected."));
        }
}

//----------------------------------------------------------------------

void GuidedCreationView::populateBlueprint(const SmartTerrainAnalyzer::AuditReport &report)
{
        m_blueprintList.DeleteAllItems();

        for (size_t i = 0; i < report.blueprint.size(); ++i)
        {
                const SmartTerrainAnalyzer::BlueprintAction &action = report.blueprint[i];
                const int item = m_blueprintList.InsertItem(static_cast<int>(i), action.label);
                m_blueprintList.SetItemText(item, 1, formatPercentString(action.predictedImpact * 100.0f));
                m_blueprintList.SetItemText(item, 2, action.rationale);
        }

        if (report.blueprint.empty())
        {
                const int row = m_blueprintList.InsertItem(0, _T("Up to date"));
                m_blueprintList.SetItemText(row, 2, _T("The AI blueprint queue is empty."));
        }
}

//----------------------------------------------------------------------

void GuidedCreationView::populateQuickActions(const SmartTerrainAnalyzer::AuditReport &report)
{
        m_quickActionList.DeleteAllItems();

        const TerrainEditorDoc *const document = getDocument();
        const MapFrame *const mapFrame = document ? document->getMapFrame() : 0;
        const bool overlaysEnabled = document && document->isGuidanceOverlayEnabled();
        const bool heatmapEnabled = document && document->isHeatmapPreviewEnabled();
        const bool guidesEnabled = document && document->isGuidelineLayerEnabled();

        int row = 0;
        CString helper;

        helper.Format(_T("Visual overlays %s"), overlaysEnabled ? _T("enabled") : _T("disabled"));
        m_quickActionList.InsertItem(row, helper);
        m_quickActionList.SetItemText(row++, 1, _T("Toggle live rule density overlays in the map view."));

        helper.Format(_T("Heatmap %s"), heatmapEnabled ? _T("enabled") : _T("disabled"));
        m_quickActionList.InsertItem(row, helper);
        m_quickActionList.SetItemText(row++, 1, _T("Preview biome coverage using AI balanced gradients."));

        helper.Format(_T("Guideline grid %s"), guidesEnabled ? _T("enabled") : _T("disabled"));
        m_quickActionList.InsertItem(row, helper);
        m_quickActionList.SetItemText(row++, 1, _T("Display rule-of-thirds and golden-ratio guides while sculpting."));

        const bool shaderAutofillEnabled = document && document->isAutoShaderAssignmentEnabled();
        helper.Format(_T("Shader autofill %s"), shaderAutofillEnabled ? _T("enabled") : _T("disabled"));
        m_quickActionList.InsertItem(row, helper);
        m_quickActionList.SetItemText(row++, 1, _T("Map AI biome labels to shader families (double-click to toggle)."));

        if (mapFrame)
        {
                CString mapInfo;
                mapInfo.Format(_T("Focused chunk @ (%0.0f, %0.0f)"), mapFrame->getCenter().x, mapFrame->getCenter().y);
                m_quickActionList.InsertItem(row, _T("Active focus"));
                m_quickActionList.SetItemText(row++, 1, mapInfo);
        }

        if (!report.blueprint.empty())
        {
                m_quickActionList.InsertItem(row, _T("Run AI blueprint"));
                m_quickActionList.SetItemText(row++, 1, report.blueprint.front().rationale);
        }

        if (document && document->hasAutoGenerationResult())
        {
                const TerrainEditorDoc::AutoGenerationResult &autoGen = document->getAutoGenerationResult();
                if (!autoGen.report.aiStatusHeadline.IsEmpty())
                {
                        m_quickActionList.InsertItem(row, _T("AI status"));
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.aiStatusHeadline);
                }

                if (!autoGen.report.operationsChecklist.IsEmpty())
                {
                        m_quickActionList.InsertItem(row, _T("AI ops"));
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.operationsChecklist);
                }

                m_quickActionList.InsertItem(row, _T("Auto terrain blueprint"));
                m_quickActionList.SetItemText(row++, 1, autoGen.report.blueprintSummary);

                for (size_t i = 0; i < autoGen.report.settlementRecommendations.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Settlement %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.settlementRecommendations[i]);
                }

                for (size_t i = 0; i < autoGen.report.biomeAdjustments.size(); ++i)
                {
                        m_quickActionList.InsertItem(row, _T("Biome tuning"));
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.biomeAdjustments[i]);
                }

                for (size_t i = 0; i < autoGen.report.travelCorridors.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Corridor %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.travelCorridors[i]);
                }

                for (size_t i = 0; i < autoGen.report.lightingPlan.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Lighting beat %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.lightingPlan[i]);
                }

                for (size_t i = 0; i < autoGen.report.weatherTimeline.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Weather %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.weatherTimeline[i]);
                }

                for (size_t i = 0; i < autoGen.report.encounterScripts.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Encounter %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.encounterScripts[i]);
                }

                for (size_t i = 0; i < autoGen.report.cinematicMoments.size(); ++i)
                {
                        CString label;
                        label.Format(_T("Cinematic %u"), static_cast<unsigned>(i + 1));
                        m_quickActionList.InsertItem(row, label);
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.cinematicMoments[i]);
                }

                for (size_t i = 0; i < autoGen.report.automationToolkit.size(); ++i)
                {
                        m_quickActionList.InsertItem(row, _T("Automation tool"));
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.automationToolkit[i]);
                }

                for (size_t i = 0; i < autoGen.report.hotspotAnnotations.size(); ++i)
                {
                        m_quickActionList.InsertItem(row, _T("Hotspot"));
                        m_quickActionList.SetItemText(row++, 1, autoGen.report.hotspotAnnotations[i]);
                }
        }
}

//----------------------------------------------------------------------

void GuidedCreationView::syncToggleState()
{
        TerrainEditorDoc *const document = getDocument();
        if (!document)
                return;

        m_overlayToggle.SetCheck(document->isGuidanceOverlayEnabled() ? BST_CHECKED : BST_UNCHECKED);
        m_heatmapToggle.SetCheck(document->isHeatmapPreviewEnabled() ? BST_CHECKED : BST_UNCHECKED);
        m_guidelineToggle.SetCheck(document->isGuidelineLayerEnabled() ? BST_CHECKED : BST_UNCHECKED);
}

//----------------------------------------------------------------------

void GuidedCreationView::OnRefreshClicked()
{
        refreshFromDocument();
}

//----------------------------------------------------------------------

void GuidedCreationView::OnToggleOverlay()
{
        TerrainEditorDoc *const document = getDocument();
        if (!document)
                return;

        const bool newValue = !document->isGuidanceOverlayEnabled();
        document->setGuidanceOverlayEnabled(newValue);
        syncToggleState();
}

//----------------------------------------------------------------------

void GuidedCreationView::OnToggleHeatmap()
{
        TerrainEditorDoc *const document = getDocument();
        if (!document)
                return;

        const bool newValue = !document->isHeatmapPreviewEnabled();
        document->setHeatmapPreviewEnabled(newValue);
        syncToggleState();
}

//----------------------------------------------------------------------

void GuidedCreationView::OnToggleGuidelines()
{
        TerrainEditorDoc *const document = getDocument();
        if (!document)
                return;

        const bool newValue = !document->isGuidelineLayerEnabled();
        document->setGuidelineLayerEnabled(newValue);
        syncToggleState();
}

//----------------------------------------------------------------------

void GuidedCreationView::OnQuickActionActivated(NMHDR *pNMHDR, LRESULT *pResult)
{
        if (pResult)
                *pResult = 0;

        LPNMITEMACTIVATE activated = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
        if (!activated || activated->iItem < 0)
                return;

        const CString label = m_quickActionList.GetItemText(activated->iItem, 0);
        if (label.Find(_T("Shader autofill")) == 0)
        {
                TerrainEditorDoc *const document = getDocument();
                if (!document)
                        return;

                document->setAutoShaderAssignmentEnabled(!document->isAutoShaderAssignmentEnabled());
                refreshFromDocument();
                syncToggleState();
        }
}

//----------------------------------------------------------------------
