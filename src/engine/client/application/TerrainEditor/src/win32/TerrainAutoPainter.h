#pragma once

//----------------------------------------------------------------------
//
// TerrainAutoPainter
// In-editor automation helper that procedurally sketches a complete
// terrain stack directly inside the Terrain Editor. The generator uses
// the same diamond-square, erosion, and biome analysis heuristics that
// previously lived in the external Python prototype, but now produces
// layers, affectors, and authoring guidance in-place.
//
//----------------------------------------------------------------------

#include "FirstTerrainEditor.h"

#include <vector>

class TerrainEditorDoc;

class TerrainAutoPainter
{
public:
        struct Config
        {
                int   gridSize;
                float roughness;
                int   seed;
                int   erosionIterations;
                float plateauBias;
                float waterLevel;
                float floraThreshold;
                float settlementThreshold;
                int   desiredSettlementCount;
                int   riverCount;
                bool  enableRiverCarving;
                bool  enableFloraEnrichment;
                bool  enableHotspotDetection;
                float travelCorridorThreshold;
                int   logisticsHubCount;
                bool  enableBiomeRebalancing;
                bool  enableSettlementZoning;
                bool  enableTravelCorridorPlanning;
                bool  enableLightingDirector;
                bool  enableWeatherSynthesis;
                bool  enableEncounterScripting;
                bool  enableCinematicMoments;
                bool  enableShaderAssignment;

                Config();
        };

        struct Result
        {
                float minimumHeight;
                float maximumHeight;
                float averageHeight;
                float standardDeviation;
                float waterCoverage;
                float plateauCoverage;
                float floraCoverage;

                CString blueprintSummary;
                std::vector<CString> biomeBreakdown;
                std::vector<CString> settlementRecommendations;
                std::vector<CString> contentHooks;
                std::vector<CString> automationToolkit;
                std::vector<CString> hotspotAnnotations;
                std::vector<CString> biomeAdjustments;
                std::vector<CString> travelCorridors;
                std::vector<CString> lightingPlan;
                std::vector<CString> weatherTimeline;
                std::vector<CString> encounterScripts;
                std::vector<CString> cinematicMoments;
                CString aiStatusHeadline;
                CString operationsChecklist;
        };

public:
        static Result generateAndApply(TerrainEditorDoc &document, const Config &config = Config());

private:
        typedef std::vector<float> HeightField;

private:
        static HeightField generateHeightField(int gridSize, float roughness, int seed);
        static void normaliseHeightField(HeightField &field);
        static void biasPlateaus(HeightField &field, float plateauBias);
        static void applyThermalErosion(HeightField &field, int gridSize, int iterations);
        static int  carveRiverNetwork(HeightField &field, int gridSize, float waterLevel, int seed, int riverCount);
        static int  enrichFloraBands(HeightField &field, int gridSize, float floraThreshold, int seed);
        static float sampleHeight(const HeightField &field, int gridSize, int x, int y);
        static float computeSlope(const HeightField &field, int gridSize, int x, int y);
        static Result analyseHeightField(const HeightField &field, int gridSize, const Config &config, const TerrainEditorDoc &document, int carvedSamples, int enrichedSamples);
        static void applyToDocument(const HeightField &field, int gridSize, const Config &config, TerrainEditorDoc &document, Result &result);
};

//----------------------------------------------------------------------
