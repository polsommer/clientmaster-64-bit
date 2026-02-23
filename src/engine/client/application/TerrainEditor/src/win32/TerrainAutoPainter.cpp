#include "FirstTerrainEditor.h"
#include "TerrainAutoPainter.h"

#include "TerrainEditorDoc.h"

#include "sharedMath/Vector2d.h"
#include "sharedRandom/RandomGenerator.h"
#include "sharedTerrain/AffectorColor.h"
#include "sharedTerrain/AffectorHeight.h"
#include "sharedTerrain/AffectorShader.h"
#include "sharedTerrain/TerrainGenerator.h"
#include "sharedTerrain/FractalGroup.h"
#include "sharedFractal/MultiFractal.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace
{
        inline int wrapIndex(int value, int bound)
        {
                if (value < 0)
                        return 0;
                if (value >= bound)
                        return bound - 1;
                return value;
        }

        inline int neighbourIndex(int gridSize, int x, int y)
        {
                return y * gridSize + x;
        }

        template <typename Iterator>
        std::pair<Iterator, Iterator> findMinMaxElement(Iterator first, Iterator last)
        {
                if (first == last)
                        return std::make_pair(last, last);

                Iterator minIt = first;
                Iterator maxIt = first;
                for (++first; first != last; ++first)
                {
                        if (*first < *minIt)
                                minIt = first;
                        if (*first > *maxIt)
                                maxIt = first;
                }

                return std::make_pair(minIt, maxIt);
        }

        CString makeLower(const CString &value)
        {
                CString lowered(value);
                lowered.MakeLower();
                return lowered;
        }

        bool containsKeyword(const CString &text, const CString &keyword)
        {
                const CString lowered = makeLower(text);
                return lowered.Find(makeLower(keyword)) != -1;
        }

        int getUniqueShaderFamilyId(ShaderGroup &shaderGroup)
        {
                int familyId = 1;
                int numberOfTries = 0;

                while (numberOfTries++ < 1000)
                {
                        bool found = false;

                        for (int familyIndex = 0; familyIndex < shaderGroup.getNumberOfFamilies(); ++familyIndex)
                        {
                                if (shaderGroup.getFamilyId(familyIndex) == familyId)
                                {
                                        found = true;
                                        break;
                                }
                        }

                        if (!found)
                                return familyId;

                        ++familyId;
                }

                return familyId;
        }
}

//--------------------------------------------------------------------------------------------------------------------------------------

TerrainAutoPainter::Config::Config()
:       gridSize(257)
,       roughness(0.72f)
,       seed(1337)
,       erosionIterations(2)
,       plateauBias(0.35f)
,       waterLevel(0.36f)
,       floraThreshold(0.58f)
,       settlementThreshold(0.42f)
,       desiredSettlementCount(6)
,       riverCount(3)
,       enableRiverCarving(true)
,       enableFloraEnrichment(true)
,       enableHotspotDetection(true)
,       travelCorridorThreshold(0.28f)
,       logisticsHubCount(4)
,       enableBiomeRebalancing(true)
,       enableSettlementZoning(true)
,       enableTravelCorridorPlanning(true)
,       enableLightingDirector(true)
,       enableWeatherSynthesis(true)
,       enableEncounterScripting(true)
,       enableCinematicMoments(true)
,       enableShaderAssignment(true)
{
}

//--------------------------------------------------------------------------------------------------------------------------------------

TerrainAutoPainter::Result TerrainAutoPainter::generateAndApply(TerrainEditorDoc &document, const Config &config)
{
        const HeightField heightField = generateHeightField(config.gridSize, config.roughness, config.seed);

        HeightField filteredField = heightField;
        normaliseHeightField(filteredField);
        biasPlateaus(filteredField, config.plateauBias);
        applyThermalErosion(filteredField, config.gridSize, config.erosionIterations);

        int carvedSamples = 0;
        int enrichedSamples = 0;

        if (config.enableRiverCarving && config.riverCount > 0)
                carvedSamples = carveRiverNetwork(filteredField, config.gridSize, config.waterLevel, config.seed, config.riverCount);

        if (config.enableFloraEnrichment)
                enrichedSamples = enrichFloraBands(filteredField, config.gridSize, config.floraThreshold, config.seed);

        normaliseHeightField(filteredField);

        Result result = analyseHeightField(filteredField, config.gridSize, config, document, carvedSamples, enrichedSamples);
        applyToDocument(filteredField, config.gridSize, config, document, result);

        return result;
}

//--------------------------------------------------------------------------------------------------------------------------------------

TerrainAutoPainter::HeightField TerrainAutoPainter::generateHeightField(int gridSize, float roughness, int seed)
{
        HeightField field(static_cast<size_t>(gridSize * gridSize), 0.0f);

        RandomGenerator rng(static_cast<uint32>(seed));

        const int maxIndex = gridSize - 1;
        field[neighbourIndex(gridSize, 0, 0)] = static_cast<float>(rng.randomReal());
        field[neighbourIndex(gridSize, maxIndex, 0)] = static_cast<float>(rng.randomReal());
        field[neighbourIndex(gridSize, 0, maxIndex)] = static_cast<float>(rng.randomReal());
        field[neighbourIndex(gridSize, maxIndex, maxIndex)] = static_cast<float>(rng.randomReal());

        int step = maxIndex;
        float scale = roughness;

        while (step > 1)
        {
                const int halfStep = step / 2;

                for (int y = halfStep; y < gridSize; y += step)
                {
                        for (int x = halfStep; x < gridSize; x += step)
                        {
                                const float a = field[neighbourIndex(gridSize, x - halfStep, y - halfStep)];
                                const float b = field[neighbourIndex(gridSize, x - halfStep, y + halfStep - step)];
                                const float c = field[neighbourIndex(gridSize, x + halfStep - step, y - halfStep)];
                                const float d = field[neighbourIndex(gridSize, x + halfStep - step, y + halfStep - step)];

                                const float average = (a + b + c + d) * 0.25f;
                                const float jitter = static_cast<float>(rng.randomReal(-scale, scale));
                                field[neighbourIndex(gridSize, x, y)] = average + jitter;
                        }
                }

                for (int y = 0; y < gridSize; y += halfStep)
                {
                        for (int x = ((y + halfStep) % step); x < gridSize; x += step)
                        {
                                float sum = 0.0f;
                                int count = 0;

                                const int offsets[4][2] =
                                {
                                        { -halfStep, 0 },
                                        { halfStep, 0 },
                                        { 0, -halfStep },
                                        { 0, halfStep }
                                };

                                for (int i = 0; i < 4; ++i)
                                {
                                        const int nx = x + offsets[i][0];
                                        const int ny = y + offsets[i][1];
                                        if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize)
                                        {
                                                sum += field[neighbourIndex(gridSize, nx, ny)];
                                                ++count;
                                        }
                                }

                                const float average = count > 0 ? sum / static_cast<float>(count) : 0.0f;
                                const float jitter = static_cast<float>(rng.randomReal(-scale, scale));
                                field[neighbourIndex(gridSize, x, y)] = average + jitter;
                        }
                }

                step = halfStep;
                scale *= 0.5f;
        }

        return field;
}

//--------------------------------------------------------------------------------------------------------------------------------------

void TerrainAutoPainter::normaliseHeightField(HeightField &field)
{
        if (field.empty())
                return;

        const auto range = findMinMaxElement(field.begin(), field.end());
        const float minValue = *range.first;
        const float maxValue = *range.second;
        if (std::fabs(maxValue - minValue) < 1.0e-6f)
        {
                std::fill(field.begin(), field.end(), 0.0f);
                return;
        }

        const float scale = 1.0f / (maxValue - minValue);
        for (float &value : field)
        {
                value = (value - minValue) * scale;
        }
}

//--------------------------------------------------------------------------------------------------------------------------------------

void TerrainAutoPainter::biasPlateaus(HeightField &field, float plateauBias)
{
        if (plateauBias <= 0.0f)
                return;

        for (float &value : field)
        {
                const float clamped = std::max(0.0f, std::min(1.0f, value));
                const float eased = std::pow(clamped, 1.0f - plateauBias) * std::pow(1.0f - clamped, plateauBias);
                const float adjustment = (0.5f - eased) * plateauBias;
                value = std::max(0.0f, std::min(1.0f, clamped + adjustment));
        }
}

//--------------------------------------------------------------------------------------------------------------------------------------

void TerrainAutoPainter::applyThermalErosion(HeightField &field, int gridSize, int iterations)
{
        if (iterations <= 0)
                return;

        HeightField delta(field.size(), 0.0f);

        for (int iter = 0; iter < iterations; ++iter)
        {
                std::fill(delta.begin(), delta.end(), 0.0f);

                for (int y = 0; y < gridSize; ++y)
                {
                        for (int x = 0; x < gridSize; ++x)
                        {
                                const int index = neighbourIndex(gridSize, x, y);
                                const float current = field[index];

                                for (int ny = y - 1; ny <= y + 1; ++ny)
                                {
                                        for (int nx = x - 1; nx <= x + 1; ++nx)
                                        {
                                                if (nx == x && ny == y)
                                                        continue;

                                                if (nx < 0 || ny < 0 || nx >= gridSize || ny >= gridSize)
                                                        continue;

                                                const int neighbour = neighbourIndex(gridSize, nx, ny);
                                                const float diff = current - field[neighbour];
                                                if (diff > 0.02f)
                                                {
                                                        const float transfer = diff * 0.25f;
                                                        delta[index] -= transfer;
                                                        delta[neighbour] += transfer;
                                                }
                                        }
                                }
                        }
                }

                for (size_t i = 0; i < field.size(); ++i)
                {
                        field[i] = std::max(0.0f, std::min(1.0f, field[i] + delta[i]));
                }
        }
}

//------------------------------------------------------------------------------------------------------------------------------

int TerrainAutoPainter::carveRiverNetwork(HeightField &field, int gridSize, float waterLevel, int seed, int riverCount)
{
        if (gridSize < 8 || riverCount <= 0)
                return 0;

        RandomGenerator rng(static_cast<uint32>(seed * 1664525u + 1013904223u));

        const int safeMin = 1;
        const int safeMax = gridSize - 2;
        const int channelCount = std::max(1, std::min(riverCount, std::max(1, gridSize / 6)));
        int carvedSamples = 0;

        for (int channel = 0; channel < channelCount; ++channel)
        {
                const int startX = safeMin + static_cast<int>(rng.randomReal(0.0f, static_cast<float>(safeMax - safeMin + 1)));
                int x = std::max(safeMin, std::min(safeMax, startX));
                int y = safeMin + static_cast<int>(rng.randomReal(0.0f, static_cast<float>(gridSize / 4 + 1)));
                y = std::max(safeMin, std::min(safeMax - 1, y));

                const int maxSteps = gridSize * 2;
                for (int step = 0; step < maxSteps; ++step)
                {
                        const int index = neighbourIndex(gridSize, x, y);
                        float &height = field[index];
                        const float carveTarget = waterLevel - 0.08f;
                        if (height > carveTarget)
                        {
                                height = std::max(0.0f, carveTarget - 0.02f);
                                ++carvedSamples;
                        }

                        if (y >= safeMax - 1)
                                break;

                        int bestX = x;
                        int bestY = y + 1;
                        float bestScore = height;

                        const int offsets[6][2] =
                        {
                                { -1, 1 },
                                { 0, 1 },
                                { 1, 1 },
                                { -1, 0 },
                                { 1, 0 },
                                { 0, 2 }
                        };

                        for (int i = 0; i < 6; ++i)
                        {
                                const int nx = x + offsets[i][0];
                                const int ny = y + offsets[i][1];
                                if (nx < safeMin || ny < safeMin || nx > safeMax || ny > safeMax)
                                        continue;

                                const int neighbour = neighbourIndex(gridSize, nx, ny);
                                float score = field[neighbour];
                                score += static_cast<float>(rng.randomReal(-0.03f, 0.03f));

                                if (i == 0 || score < bestScore)
                                {
                                        bestScore = score;
                                        bestX = nx;
                                        bestY = ny;
                                }
                        }

                        x = bestX;
                        y = bestY;
                }
        }

        return carvedSamples;
}

//------------------------------------------------------------------------------------------------------------------------------

int TerrainAutoPainter::enrichFloraBands(HeightField &field, int gridSize, float floraThreshold, int seed)
{
        if (gridSize < 4)
                return 0;

        RandomGenerator rng(static_cast<uint32>(seed * 22695477u + 1u));

        int enrichedSamples = 0;
        for (int y = 1; y < gridSize - 1; ++y)
        {
                for (int x = 1; x < gridSize - 1; ++x)
                {
                        const int index = neighbourIndex(gridSize, x, y);
                        float &value = field[index];
                        if (value < floraThreshold - 0.12f || value > 0.95f)
                                continue;

                        const float slope = computeSlope(field, gridSize, x, y);
                        if (slope > 0.55f)
                                continue;

                        const float boost = static_cast<float>(rng.randomReal(0.0f, 0.035f));
                        if (boost <= 0.0f)
                                continue;

                        const float newValue = std::min(1.0f, value + boost);
                        if (newValue > value)
                        {
                                value = newValue;
                                ++enrichedSamples;
                        }
                }
        }

        return enrichedSamples;
}

//------------------------------------------------------------------------------------------------------------------------------

float TerrainAutoPainter::sampleHeight(const HeightField &field, int gridSize, int x, int y)
{
        return field[neighbourIndex(gridSize, wrapIndex(x, gridSize), wrapIndex(y, gridSize))];
}

//------------------------------------------------------------------------------------------------------------------------------

float TerrainAutoPainter::computeSlope(const HeightField &field, int gridSize, int x, int y)
{
        const float center = sampleHeight(field, gridSize, x, y);
        const float dx = sampleHeight(field, gridSize, x + 1, y) - sampleHeight(field, gridSize, x - 1, y);
        const float dy = sampleHeight(field, gridSize, x, y + 1) - sampleHeight(field, gridSize, x, y - 1);
        return std::sqrt(dx * dx + dy * dy) + std::fabs(center - 0.5f) * 0.5f;
}

//------------------------------------------------------------------------------------------------------------------------------

TerrainAutoPainter::Result TerrainAutoPainter::analyseHeightField(const HeightField &field, int gridSize, const Config &config, const TerrainEditorDoc &document, int carvedSamples, int enrichedSamples)
{
        Result result = {};

        if (field.empty())
                return result;

        const float sum = std::accumulate(field.begin(), field.end(), 0.0f);
        result.averageHeight = sum / static_cast<float>(field.size());

        const auto range = findMinMaxElement(field.begin(), field.end());
        result.minimumHeight = *range.first;
        result.maximumHeight = *range.second;

        float variance = 0.0f;
        int waterCells = 0;
        int plateauCells = 0;
        int floraCells = 0;

        for (float value : field)
        {
                const float diff = value - result.averageHeight;
                variance += diff * diff;

                if (value <= config.waterLevel)
                        ++waterCells;
                if (value > config.settlementThreshold && std::fabs(value - 0.5f) < 0.15f)
                        ++plateauCells;
                if (value >= config.floraThreshold)
                        ++floraCells;
        }

        variance /= std::max(1, static_cast<int>(field.size()) - 1);
        result.standardDeviation = std::sqrt(variance);

        const float totalCells = static_cast<float>(field.size());
        result.waterCoverage = waterCells / totalCells;
        result.plateauCoverage = plateauCells / totalCells;
        result.floraCoverage = floraCells / totalCells;

        int steepCells = 0;
        int shorelineCells = 0;
        int valleyCells = 0;
        int northFacingCells = 0;
        int southFacingCells = 0;
        int hotspotSamples = 0;

        if (config.enableHotspotDetection)
        {
                for (int y = 1; y < gridSize - 1; y += 2)
                {
                        for (int x = 1; x < gridSize - 1; x += 2)
                        {
                                const float height = sampleHeight(field, gridSize, x, y);
                                const float slope = computeSlope(field, gridSize, x, y);
                                const float dy = sampleHeight(field, gridSize, x, y + 1) - sampleHeight(field, gridSize, x, y - 1);

                                ++hotspotSamples;

                                if (slope > 0.7f)
                                        ++steepCells;
                                if (height <= config.waterLevel + 0.05f)
                                        ++shorelineCells;
                                if (height > config.waterLevel + 0.02f && height < config.waterLevel + 0.12f && slope < 0.3f)
                                        ++valleyCells;

                                if (slope > 0.25f)
                                {
                                        if (dy > 0.02f)
                                                ++northFacingCells;
                                        else if (dy < -0.02f)
                                                ++southFacingCells;
                                }
                        }
                }
        }

        const int candidateCount = std::max(1, config.desiredSettlementCount);
        std::vector<std::pair<float, CPoint> > candidates;
        candidates.reserve(candidateCount * 2);

        for (int y = 4; y < gridSize - 4; y += 4)
        {
                for (int x = 4; x < gridSize - 4; x += 4)
                {
                        const float height = sampleHeight(field, gridSize, x, y);
                        if (height < config.settlementThreshold)
                                continue;

                        const float slope = computeSlope(field, gridSize, x, y);
                        if (slope > 0.55f)
                                continue;

                        candidates.push_back(std::make_pair(height - slope * 0.35f, CPoint(x, y)));
                }
        }

        std::sort(candidates.begin(), candidates.end(),
                [](const std::pair<float, CPoint> &lhs, const std::pair<float, CPoint> &rhs)
                {
                        return lhs.first > rhs.first;
                });

        const float mapWidth = document.getMapWidthInMeters();
        const float worldScale = mapWidth / static_cast<float>(gridSize - 1);

        CPoint highestCell(0, 0);
        CPoint lowestCell(0, 0);
        CPoint steepestCell(0, 0);
        CPoint flattestCell(0, 0);
        float highestHeight = -1.0f;
        float lowestHeight = 2.0f;
        float steepestSlope = -1.0f;
        float flattestSlope = 10.0f;

        for (int y = 1; y < gridSize - 1; ++y)
        {
                for (int x = 1; x < gridSize - 1; ++x)
                {
                        const float height = sampleHeight(field, gridSize, x, y);
                        if (height > highestHeight)
                        {
                                highestHeight = height;
                                highestCell = CPoint(x, y);
                        }
                        if (height < lowestHeight)
                        {
                                lowestHeight = height;
                                lowestCell = CPoint(x, y);
                        }

                        const float slope = computeSlope(field, gridSize, x, y);
                        if (slope > steepestSlope)
                        {
                                steepestSlope = slope;
                                steepestCell = CPoint(x, y);
                        }
                        if (slope < flattestSlope)
                        {
                                flattestSlope = slope;
                                flattestCell = CPoint(x, y);
                        }
                }
        }

        const int suggestionCount = std::min(candidateCount, static_cast<int>(candidates.size()));
        for (int i = 0; i < suggestionCount; ++i)
        {
                const CPoint &cell = candidates[i].second;
                const float worldX = (cell.x - gridSize / 2) * worldScale;
                const float worldZ = (cell.y - gridSize / 2) * worldScale;

                CString entry;
                entry.Format(_T("Hub %d @ (%.0fm, %.0fm) – slope %.2f, elevation %.0f%%"),
                        i + 1,
                        worldX,
                        worldZ,
                        computeSlope(field, gridSize, cell.x, cell.y),
                        sampleHeight(field, gridSize, cell.x, cell.y) * 100.0f);
                result.settlementRecommendations.push_back(entry);
        }

        const float hotspotBase = (hotspotSamples > 0) ? static_cast<float>(hotspotSamples) : 1.0f;
        const float ridgeDensity = static_cast<float>(steepCells) / hotspotBase;
        const float shorelineDensity = static_cast<float>(shorelineCells) / hotspotBase;
        const float valleyDensity = static_cast<float>(valleyCells) / hotspotBase;
        const float northRatio = static_cast<float>(northFacingCells) / hotspotBase;
        const float southRatio = static_cast<float>(southFacingCells) / hotspotBase;

        CString blueprint;
        blueprint.Format(_T("Height %.0f–%.0f%% • Avg %.0f%% • σ %.0f%% • Water %.0f%% • Flora %.0f%%"),
                result.minimumHeight * 100.0f,
                result.maximumHeight * 100.0f,
                result.averageHeight * 100.0f,
                result.standardDeviation * 100.0f,
                result.waterCoverage * 100.0f,
                result.floraCoverage * 100.0f);
        result.blueprintSummary = blueprint;

        CString biomeLow;
        biomeLow.Format(_T("Lowlands %.0f%% – primary water table candidates"), result.waterCoverage * 100.0f);
        result.biomeBreakdown.push_back(biomeLow);

        CString biomePlateau;
        biomePlateau.Format(_T("Plateaus %.0f%% – settlement ready terraces"), result.plateauCoverage * 100.0f);
        result.biomeBreakdown.push_back(biomePlateau);

        CString biomeFlora;
        biomeFlora.Format(_T("Lush bands %.0f%% – high flora density"), result.floraCoverage * 100.0f);
        result.biomeBreakdown.push_back(biomeFlora);

        result.contentHooks.push_back(_T("Queue river spline generator across water basins."));
        result.contentHooks.push_back(_T("Stamp hero POIs on top three settlement hubs."));
        result.contentHooks.push_back(_T("Layer regional flora palettes following lush bands."));

        if (carvedSamples > 0)
        {
                CString riverTool;
                riverTool.Format(_T("River lattice assistant carved %d samples – convert channels into river affectors."), carvedSamples);
                result.automationToolkit.push_back(riverTool);
        }

        if (enrichedSamples > 0)
        {
                CString floraTool;
                floraTool.Format(_T("Flora enrichment pass boosted %d samples – seed flora presets along the highlighted bands."), enrichedSamples);
                result.automationToolkit.push_back(floraTool);
        }

        if (ridgeDensity > 0.18f)
                result.automationToolkit.push_back(_T("Ridge sculptor heuristics primed – deploy the AI ridge brush to emphasise crests."));

        if (shorelineDensity > 0.12f)
                result.automationToolkit.push_back(_T("Shoreline sculptor ready – extrude coastal splines for water table polish."));

        if (valleyDensity > 0.1f)
                result.automationToolkit.push_back(_T("Valley filler queued – generate terrace affectors to support settlements."));

        if (hotspotSamples > 0)
        {
                CString summary;
                summary.Format(_T("Hotspot mix – ridges %.0f%% • shorelines %.0f%% • valleys %.0f%%"),
                        ridgeDensity * 100.0f,
                        shorelineDensity * 100.0f,
                        valleyDensity * 100.0f);
                result.hotspotAnnotations.push_back(summary);

                if (ridgeDensity > 0.18f)
                        result.hotspotAnnotations.push_back(_T("Steep ridge clusters detected – smooth transitions before adding hero props."));
                if (shorelineDensity > 0.12f)
                        result.hotspotAnnotations.push_back(_T("Wide shoreline band identified – blend shoreline shaders and foam maps."));
                if (valleyDensity > 0.1f)
                        result.hotspotAnnotations.push_back(_T("Valley terraces suitable for settlements – consider hub placement."));

                if (northRatio > southRatio + 0.05f)
                        result.hotspotAnnotations.push_back(_T("Northern slopes dominate – orient lighting and wind FX towards the north."));
                else if (southRatio > northRatio + 0.05f)
                        result.hotspotAnnotations.push_back(_T("Southern escarpments dominate – plan vistas looking south."));
        }

        if (config.enableBiomeRebalancing)
        {
                if (result.waterCoverage < 0.18f)
                {
                        CString guidance;
                        guidance.Format(_T("Water coverage at %.0f%% – widen basins or drop the table for wetlands."), result.waterCoverage * 100.0f);
                        result.biomeAdjustments.push_back(guidance);
                }
                else if (result.waterCoverage > 0.35f)
                {
                        CString guidance;
                        guidance.Format(_T("Water coverage at %.0f%% – raise terraces or levees to reclaim dry ground."), result.waterCoverage * 100.0f);
                        result.biomeAdjustments.push_back(guidance);
                }

                if (result.floraCoverage < 0.25f)
                {
                        CString guidance;
                        guidance.Format(_T("Flora coverage at %.0f%% – seed additional vegetation along lush bands."), result.floraCoverage * 100.0f);
                        result.biomeAdjustments.push_back(guidance);
                }
                else if (result.floraCoverage > 0.55f)
                {
                        CString guidance;
                        guidance.Format(_T("Flora coverage at %.0f%% – thin vegetation near hubs for readability."), result.floraCoverage * 100.0f);
                        result.biomeAdjustments.push_back(guidance);
                }

                if (result.plateauCoverage < 0.12f)
                {
                        CString guidance;
                        guidance.Format(_T("Plateau coverage at %.0f%% – carve extra terraces for base building."), result.plateauCoverage * 100.0f);
                        result.biomeAdjustments.push_back(guidance);
                }

                if (!result.biomeAdjustments.empty())
                        result.contentHooks.push_back(_T("Blend biome masks following the AI rebalance checklist."));
        }

        if (config.enableSettlementZoning && !result.settlementRecommendations.empty())
        {
                const int hubCount = std::max(1, config.logisticsHubCount);
                const int availableSettlements = static_cast<int>(result.settlementRecommendations.size());

                CString zoning;
                zoning.Format(_T("Zoning planner staged %d logistic hub%s – align with settlement hubs 1-%d."),
                        hubCount,
                        (hubCount == 1) ? _T("") : _T("s"),
                        std::min(hubCount, availableSettlements));
                result.automationToolkit.push_back(zoning);

                CString spacing;
                spacing.Format(_T("Target %.0fm spacing between hubs across the %.0fm play space."),
                        (availableSettlements > 0) ? mapWidth / static_cast<float>(hubCount) : mapWidth,
                        mapWidth);
                result.biomeAdjustments.push_back(spacing);
        }

        if (config.enableTravelCorridorPlanning)
        {
                struct CorridorCandidate
                {
                        float score;
                        CPoint cell;
                        float slope;
                        float waterBias;
                };

                std::vector<CorridorCandidate> corridors;

                for (int y = 2; y < gridSize - 2; y += 6)
                {
                        for (int x = 2; x < gridSize - 2; x += 6)
                        {
                                const float height = sampleHeight(field, gridSize, x, y);
                                const float slope = computeSlope(field, gridSize, x, y);
                                const float waterBias = std::fabs(height - config.waterLevel);
                                const float hubBias = std::fabs(height - config.settlementThreshold);
                                const float corridorScore = (0.75f - slope) + (1.0f - std::min(1.0f, waterBias * 2.0f)) * 0.4f + (1.0f - std::min(1.0f, hubBias * 2.0f)) * 0.6f;
                                if (corridorScore < config.travelCorridorThreshold)
                                        continue;

                                CorridorCandidate candidate;
                                candidate.score = corridorScore;
                                candidate.cell = CPoint(x, y);
                                candidate.slope = slope;
                                candidate.waterBias = waterBias;
                                corridors.push_back(candidate);
                        }
                }

                std::sort(corridors.begin(), corridors.end(),
                        [](const CorridorCandidate &lhs, const CorridorCandidate &rhs)
                        {
                                return lhs.score > rhs.score;
                        });

                const int corridorCount = std::min(std::max(1, config.logisticsHubCount), static_cast<int>(corridors.size()));
                for (int i = 0; i < corridorCount; ++i)
                {
                        const CorridorCandidate &candidate = corridors[i];
                        const float worldX = (candidate.cell.x - gridSize / 2) * worldScale;
                        const float worldZ = (candidate.cell.y - gridSize / 2) * worldScale;

                        CString corridor;
                        corridor.Format(_T("Route %d via (%.0fm, %.0fm) – slope %.2f, water offset %.0f%%"),
                                i + 1,
                                worldX,
                                worldZ,
                                candidate.slope,
                                (1.0f - std::min(1.0f, candidate.waterBias)) * 100.0f);
                        result.travelCorridors.push_back(corridor);
                }

                if (!result.travelCorridors.empty())
                {
                        result.automationToolkit.push_back(_T("Route planner solved travel corridors – convert them into spline roads."));
                        result.contentHooks.push_back(_T("Lay shuttle paths along the AI corridor map for fast traversal."));
                }
        }

        if (config.enableLightingDirector)
        {
                CString sunrise;
                sunrise.Format(_T("Sunrise key at (%.0fm, %.0fm) to graze ridge density %.0f%%."),
                        (steepestCell.x - gridSize / 2) * worldScale,
                        (steepestCell.y - gridSize / 2) * worldScale,
                        ridgeDensity * 100.0f);
                result.lightingPlan.push_back(sunrise);

                CString dusk;
                dusk.Format(_T("Dusk rim at (%.0fm, %.0fm) catching shoreline %.0f%% coverage."),
                        (lowestCell.x - gridSize / 2) * worldScale,
                        (lowestCell.y - gridSize / 2) * worldScale,
                        shorelineDensity * 100.0f);
                result.lightingPlan.push_back(dusk);

                CString fill;
                fill.Format(_T("Ambient fill over flat step near (%.0fm, %.0fm) – slope %.2f."),
                        (flattestCell.x - gridSize / 2) * worldScale,
                        (flattestCell.y - gridSize / 2) * worldScale,
                        flattestSlope);
                result.lightingPlan.push_back(fill);

                if (!result.lightingPlan.empty())
                        result.automationToolkit.push_back(_T("Lighting director queued – bake cinematic lighting rigs across the AI beats."));
        }

        if (config.enableWeatherSynthesis)
        {
                CString storm;
                storm.Format(_T("Kick off stormfront over wetlands (%.0f%% water coverage)."), result.waterCoverage * 100.0f);
                result.weatherTimeline.push_back(storm);

                CString clear;
                clear.Format(_T("Fade to clear skies above plateaus (%.0f%% coverage) for build phases."), result.plateauCoverage * 100.0f);
                result.weatherTimeline.push_back(clear);

                CString wind;
                wind.Format(_T("Pulse winds along corridors every %d minutes to keep dunes dynamic."), document.getEnvironmentCycleTime());
                result.weatherTimeline.push_back(wind);

                result.automationToolkit.push_back(_T("Weather tuner configured – export the cycle to the environment playlist."));
        }

        if (config.enableEncounterScripting)
        {
                struct EncounterCandidate
                {
                        float score;
                        CPoint cell;
                        float slope;
                        float height;
                };

                std::vector<EncounterCandidate> encounters;

                for (int y = 2; y < gridSize - 2; y += 4)
                {
                        for (int x = 2; x < gridSize - 2; x += 4)
                        {
                                const float height = sampleHeight(field, gridSize, x, y);
                                const float slope = computeSlope(field, gridSize, x, y);
                                if (slope < 0.25f)
                                        continue;

                                const float prominence = height - result.averageHeight;
                                EncounterCandidate candidate;
                                candidate.score = slope * 0.6f + std::fabs(prominence) * 0.4f;
                                candidate.cell = CPoint(x, y);
                                candidate.slope = slope;
                                candidate.height = height;
                                encounters.push_back(candidate);
                        }
                }

                std::sort(encounters.begin(), encounters.end(),
                        [](const EncounterCandidate &lhs, const EncounterCandidate &rhs)
                        {
                                return lhs.score > rhs.score;
                        });

                const int encounterCount = std::min(std::max(2, config.logisticsHubCount), static_cast<int>(encounters.size()));
                for (int i = 0; i < encounterCount; ++i)
                {
                        const EncounterCandidate &candidate = encounters[i];
                        const float worldX = (candidate.cell.x - gridSize / 2) * worldScale;
                        const float worldZ = (candidate.cell.y - gridSize / 2) * worldScale;

                        CString script;
                        script.Format(_T("Encounter %d at (%.0fm, %.0fm) – slope %.2f, elevation %.0f%%"),
                                i + 1,
                                worldX,
                                worldZ,
                                candidate.slope,
                                candidate.height * 100.0f);
                        result.encounterScripts.push_back(script);
                }

                if (!result.encounterScripts.empty())
                        result.contentHooks.push_back(_T("Block out encounter scripts across the AI scouted set-pieces."));
        }

        if (config.enableCinematicMoments)
        {
                        CString vista;
                        vista.Format(_T("Hero vista at (%.0fm, %.0fm) – peak elevation %.0f%%."),
                                (highestCell.x - gridSize / 2) * worldScale,
                                (highestCell.y - gridSize / 2) * worldScale,
                                highestHeight * 100.0f);
                        result.cinematicMoments.push_back(vista);

                        CString canyon;
                        canyon.Format(_T("Canyon fly-through at (%.0fm, %.0fm) – basin %.0f%%."),
                                (lowestCell.x - gridSize / 2) * worldScale,
                                (lowestCell.y - gridSize / 2) * worldScale,
                                lowestHeight * 100.0f);
                        result.cinematicMoments.push_back(canyon);

                        CString ridge;
                        ridge.Format(_T("Tracking shot along ridge slope %.2f at (%.0fm, %.0fm)."),
                                steepestSlope,
                                (steepestCell.x - gridSize / 2) * worldScale,
                                (steepestCell.y - gridSize / 2) * worldScale);
                        result.cinematicMoments.push_back(ridge);

                        result.automationToolkit.push_back(_T("Cinematic pathing queued – generate camera rails along AI vista beats."));
        }

        const size_t toolkitCount = result.automationToolkit.size();
        const size_t corridorCount = result.travelCorridors.size();
        const size_t weatherCount = result.weatherTimeline.size();
        const size_t encounterCount = result.encounterScripts.size();
        const size_t cinematicCount = result.cinematicMoments.size();

        CString operations;
        operations.Format(_T("Toolkit %u • Corridors %u • Weather beats %u • Encounters %u • Cinematics %u"),
                static_cast<unsigned>(toolkitCount),
                static_cast<unsigned>(corridorCount),
                static_cast<unsigned>(weatherCount),
                static_cast<unsigned>(encounterCount),
                static_cast<unsigned>(cinematicCount));
        result.operationsChecklist = operations;

        CString status;
        status.Format(_T("Seed %d • Hotspots %u • Automation hooks %u • Logistics hubs %d"),
                config.seed,
                static_cast<unsigned>(result.hotspotAnnotations.size()),
                static_cast<unsigned>(toolkitCount),
                config.logisticsHubCount);
        result.aiStatusHeadline = status;

        return result;
}

//--------------------------------------------------------------------------------------------------------------------------------------

void TerrainAutoPainter::applyToDocument(const HeightField &field, int gridSize, const Config &config, TerrainEditorDoc &document, Result &result)
{
        UNREFERENCED_PARAMETER(field);
        UNREFERENCED_PARAMETER(gridSize);

        TerrainGenerator *const generator = document.getTerrainGenerator();
        if (!generator)
                return;

        generator->reset();

        MultiFractal baseFractal;
        baseFractal.setSeed(static_cast<uint32>(config.seed));
        baseFractal.setNumberOfOctaves(6);
        baseFractal.setAmplitude(1.0f);
        baseFractal.setFrequency(2.2f + config.roughness);
        baseFractal.setScale(0.0035f, 0.0035f);
        baseFractal.setGain(true, 0.65f);

        const int baseFamilyId = generator->getFractalGroup().createFamily(&baseFractal, "AIBase");

        MultiFractal ridgeFractal;
        ridgeFractal.setSeed(static_cast<uint32>(config.seed + 1));
        ridgeFractal.setNumberOfOctaves(5);
        ridgeFractal.setAmplitude(0.85f);
        ridgeFractal.setFrequency(3.8f);
        ridgeFractal.setScale(0.0085f, 0.0085f);
        ridgeFractal.setCombinationRule(MultiFractal::CR_turbulenceClamp);

        const int ridgeFamilyId = generator->getFractalGroup().createFamily(&ridgeFractal, "AIRidge");

        TerrainGenerator::Layer *const baseLayer = new TerrainGenerator::Layer();
        baseLayer->setName("AI Base Terrain");

        AffectorHeightFractal *const baseHeight = new AffectorHeightFractal();
        baseHeight->setName("AI Base Height");
        baseHeight->setFamilyId(baseFamilyId);
        baseHeight->setOperation(TGO_replace);
        baseHeight->setScaleY(65.0f);
        baseLayer->addAffector(baseHeight);

        AffectorColorConstant *const baseColor = new AffectorColorConstant();
        baseColor->setName("AI Base Color");
        PackedRgb baseRgb(118, 108, 92);
        baseColor->setColor(baseRgb);
        baseColor->setOperation(TGO_replace);
        baseLayer->addAffector(baseColor);

        generator->addLayer(baseLayer);

        TerrainGenerator::Layer *const ridgeLayer = new TerrainGenerator::Layer();
        ridgeLayer->setName("AI Ridge Sculpt");

        AffectorHeightFractal *const ridgeHeight = new AffectorHeightFractal();
        ridgeHeight->setName("AI Ridge Height");
        ridgeHeight->setFamilyId(ridgeFamilyId);
        ridgeHeight->setOperation(TGO_add);
        ridgeHeight->setScaleY(28.0f);
        ridgeLayer->addAffector(ridgeHeight);

        AffectorColorConstant *const ridgeColor = new AffectorColorConstant();
        ridgeColor->setName("AI Ridge Tone");
        PackedRgb ridgeRgb(156, 140, 120);
        ridgeColor->setColor(ridgeRgb);
        ridgeColor->setOperation(TGO_replace);
        ridgeLayer->addAffector(ridgeColor);

        generator->addLayer(ridgeLayer);

        if (config.enableShaderAssignment)
        {
                ShaderGroup &shaderGroup = generator->getShaderGroup();
                const float defaultShaderSize = document.getDefaultShaderSize();

                struct ShaderFamilySeed
                {
                        CString label;
                        CString familyName;
                        PackedRgb color;
                };

                std::vector<ShaderFamilySeed> shaderSeeds;
                shaderSeeds.push_back({_T("Lowlands"), _T("AI Lowlands"), PackedRgb(90, 140, 166)});
                shaderSeeds.push_back({_T("Plateaus"), _T("AI Plateaus"), PackedRgb(164, 140, 118)});
                shaderSeeds.push_back({_T("Lush bands"), _T("AI Lush Bands"), PackedRgb(96, 148, 104)});

                const bool ridgeHotspot = std::find_if(result.hotspotAnnotations.begin(), result.hotspotAnnotations.end(),
                        [](const CString &entry)
                        {
                                return containsKeyword(entry, _T("ridge"));
                        }) != result.hotspotAnnotations.end();

                const bool shorelineHotspot = std::find_if(result.hotspotAnnotations.begin(), result.hotspotAnnotations.end(),
                        [](const CString &entry)
                        {
                                return containsKeyword(entry, _T("shore"));
                        }) != result.hotspotAnnotations.end();

                const bool valleyHotspot = std::find_if(result.hotspotAnnotations.begin(), result.hotspotAnnotations.end(),
                        [](const CString &entry)
                        {
                                return containsKeyword(entry, _T("valley"));
                        }) != result.hotspotAnnotations.end();

                if (ridgeHotspot)
                        shaderSeeds.push_back({_T("Ridges"), _T("AI Ridges"), PackedRgb(170, 122, 94)});

                if (shorelineHotspot)
                        shaderSeeds.push_back({_T("Shorelines"), _T("AI Shorelines"), PackedRgb(96, 138, 180)});

                if (valleyHotspot)
                        shaderSeeds.push_back({_T("Valleys"), _T("AI Valleys"), PackedRgb(120, 110, 98)});

                auto ensureFamily = [&](const ShaderFamilySeed &seed)
                {
                        if (shaderGroup.hasFamily(seed.familyName))
                        {
                                const int familyId = shaderGroup.getFamilyId(seed.familyName);
                                shaderGroup.setFamilyColor(familyId, seed.color);
                                shaderGroup.setFamilyShaderSize(familyId, defaultShaderSize);
                                return familyId;
                        }

                        const int familyId = getUniqueShaderFamilyId(shaderGroup);
                        shaderGroup.addFamily(familyId, seed.familyName, seed.color);
                        shaderGroup.setFamilyShaderSize(familyId, defaultShaderSize);
                        return familyId;
                };

                std::vector<int> seededFamilies;
                seededFamilies.reserve(shaderSeeds.size());
                for (const ShaderFamilySeed &seed : shaderSeeds)
                        seededFamilies.push_back(ensureFamily(seed));

                if (!seededFamilies.empty())
                {
                        TerrainGenerator::Layer *const shaderLayer = new TerrainGenerator::Layer();
                        shaderLayer->setName(_T("AI Shader Assignment"));

                        CString shaderNotes(_T("TerrainAutoPainter mapped biome guidance to shader families:\r\n"));
                        for (size_t i = 0; i < shaderSeeds.size(); ++i)
                        {
                                shaderNotes += _T("• ");
                                shaderNotes += shaderSeeds[i].label + _T(" → ") + shaderSeeds[i].familyName + _T("\r\n");
                        }

                        if (!result.biomeBreakdown.empty())
                        {
                                shaderNotes += _T("\r\nBiomes:\r\n");
                                for (const CString &biome : result.biomeBreakdown)
                                {
                                        shaderNotes += _T("- ") + biome + _T("\r\n");
                                }
                        }

                        if (!result.hotspotAnnotations.empty())
                        {
                                shaderNotes += _T("\r\nHotspots:\r\n");
                                for (const CString &hotspot : result.hotspotAnnotations)
                                {
                                        shaderNotes += _T("- ") + hotspot + _T("\r\n");
                                }
                        }

                        shaderLayer->setNotes(shaderNotes);

                        AffectorShaderConstant *const shaderFill = new AffectorShaderConstant();
                        shaderFill->setName(_T("AI Base Shader Fill"));
                        shaderFill->setFamilyId(seededFamilies.front());
                        shaderLayer->addAffector(shaderFill);

                        generator->addLayer(shaderLayer);
                }
        }

        document.useGlobalWaterTable = true;
        const float heightRange = document.getWhiteHeight() - document.getBlackHeight();
        document.globalWaterTableHeight = document.getBlackHeight() + heightRange * config.waterLevel * 0.85f;
        document.m_environmentCycleTime = 72;

        document.m_collidableSeed = static_cast<uint32>(config.seed * 3 + 1);
        document.m_nonCollidableSeed = static_cast<uint32>(config.seed * 3 + 7);
        document.m_radialSeed = static_cast<uint32>(config.seed * 3 + 13);
        document.m_farRadialSeed = static_cast<uint32>(config.seed * 3 + 19);

        document.m_collidableTileSize = 6.0f;
        document.m_nonCollidableTileSize = 4.0f;
        document.m_radialTileSize = 3.0f;
        document.m_farRadialTileSize = 20.0f;

        document.m_collidableMinimumDistance = 6.0f;
        document.m_collidableMaximumDistance = 64.0f;
        document.m_nonCollidableMinimumDistance = 2.0f;
        document.m_nonCollidableMaximumDistance = 28.0f;
        document.m_radialMinimumDistance = 1.5f;
        document.m_radialMaximumDistance = 16.0f;
        document.m_farRadialMinimumDistance = 80.0f;
        document.m_farRadialMaximumDistance = 260.0f;

        CString overlay;
        overlay.Format(_T("AutoSynth seed %d – water %.0f%% • plateaus %.0f%% • flora %.0f%%"),
                config.seed,
                result.waterCoverage * 100.0f,
                result.plateauCoverage * 100.0f,
                result.floraCoverage * 100.0f);
        result.contentHooks.insert(result.contentHooks.begin(), overlay);

        document.setGuidanceOverlayEnabled(true);
        document.setHeatmapPreviewEnabled(true);
        document.setGuidelineLayerEnabled(true);

        document.UpdateAllViews(0);
}

//--------------------------------------------------------------------------------------------------------------------------------------
