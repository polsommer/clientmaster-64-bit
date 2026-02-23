#pragma once

//----------------------------------------------------------------------
//
// SmartTerrainAnalyzer
// A lightweight analytic helper that inspects the currently loaded
// terrain generator and produces human readable insights for the
// modernised Terrain Editor experience.
//
//----------------------------------------------------------------------

#include "FirstTerrainEditor.h"

#include <vector>

class TerrainEditorDoc;

class SmartTerrainAnalyzer
{
public:
        struct Insight
        {
                CString headline;
                CString detail;
                float   confidence;
        };

        struct BlueprintAction
        {
                CString label;
                CString rationale;
                float   predictedImpact;
                float   confidence;
        };

        struct AuditReport
        {
                float foresightScore;
                float structureScore;
                float ecosystemScore;
                float workflowScore;

                int totalLayers;
                int activeLayers;
                int inactiveLayers;
                int totalBoundaries;
                int totalFilters;
                int totalAffectors;
                int hierarchyDepth;

                int shaderFamilies;
                int floraFamilies;
                int radialFamilies;
                int environmentFamilies;
                int fractalFamilies;
                int bitmapFamilies;

                bool hasGlobalWaterTable;
                int environmentCycleMinutes;

                std::vector<CString> dormantLayers;
                std::vector<CString> emptyLayers;
                std::vector<CString> boundaryFreeLayers;
                std::vector<CString> heroLayers;

                std::vector<Insight> insights;
                std::vector<BlueprintAction> blueprint;

                std::vector<CString> copilotModules;
                std::vector<CString> automationOpportunities;
                std::vector<CString> monitoringSignals;
        };

public:
        static AuditReport analyze(const TerrainEditorDoc &doc);
        static CString runAudit(const TerrainEditorDoc &doc);
};

//----------------------------------------------------------------------
