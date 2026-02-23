#include "TerrainAutoPainter.h"
#include "SmartTerrainAnalyzer.h"
#include "TerrainEditorDoc.h"

#include <afxwin.h>
#include <atlconv.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Headless application stub so the TerrainEditor document lifecycle can
// run without the UI frame.
class HeadlessTerrainEditorApp : public CWinApp
{
public:
        BOOL InitInstance() override
        {
                CWinApp::InitInstance();
                return TRUE;
        }
};

HeadlessTerrainEditorApp theApp;

namespace
{
        std::string narrow(const CString &value)
        {
                CT2A converted(value.GetString());
                return std::string(converted);
        }

        std::string escapeJson(const std::string &value)
        {
                std::ostringstream stream;
                for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
                {
                        const char ch = *it;
                        switch (ch)
                        {
                        case '\\': stream << "\\\\"; break;
                        case '"': stream << "\\\""; break;
                        case '\n': stream << "\\n"; break;
                        case '\r': stream << "\\r"; break;
                        case '\t': stream << "\\t"; break;
                        default: stream << ch; break;
                        }
                }

                return stream.str();
        }

        template <typename Container>
        void writeJsonArray(std::ostream &out, const char *name, const Container &items, int indent = 2)
        {
                std::string pad(static_cast<size_t>(indent), ' ');
                out << pad << '"' << name << "\": [";
                for (typename Container::const_iterator it = items.begin(); it != items.end(); ++it)
                {
                        if (it != items.begin())
                                out << ", ";
                        out << '"' << escapeJson(narrow(*it)) << '"';
                }
                out << "]";
        }

        TerrainEditorDoc *loadTerrainDocument(const char *path)
        {
                // MFC requires the module state to be set for document operations.
                AFX_MANAGE_STATE(AfxGetStaticModuleState());

                TerrainEditorDoc *document = new TerrainEditorDoc();
                if (!document->OnOpenDocument(path))
                {
                        delete document;
                        return nullptr;
                }

                return document;
        }
}

int main(int argc, char **argv)
{
        if (argc < 2)
        {
                std::cerr << "Usage: terrain_autopainter_headless <terrain_file>\n";
                return 1;
        }

        TerrainEditorDoc *document = loadTerrainDocument(argv[1]);
        if (!document)
        {
                std::cerr << "Failed to open terrain document: " << argv[1] << "\n";
                return 2;
        }

        TerrainAutoPainter::Config config;
        config.gridSize = std::max(257, config.gridSize);

        TerrainAutoPainter::Result result = TerrainAutoPainter::generateAndApply(*document, config);

        TerrainEditorDoc::AutoGenerationResult autoResult;
        autoResult.report = result;
        autoResult.valid = true;
        document->setAutoGenerationResult(autoResult);

        const SmartTerrainAnalyzer::AuditReport audit = SmartTerrainAnalyzer::analyze(*document);

        const float mapWidth = document->getMapWidthInMeters();
        const float worldScale = mapWidth / static_cast<float>(config.gridSize - 1);

        delete document;

        std::cout << "{\n";
        std::cout << "  \"mapWidthMeters\": " << mapWidth << ",\n";
        std::cout << "  \"gridSize\": " << config.gridSize << ",\n";
        std::cout << "  \"worldScaleMetersPerSample\": " << worldScale << ",\n";
        std::cout << "  \"blueprintSummary\": \"" << escapeJson(narrow(result.blueprintSummary)) << "\",\n";
        writeJsonArray(std::cout, "settlementRecommendations", result.settlementRecommendations, 2);
        std::cout << ",\n";
        writeJsonArray(std::cout, "travelCorridors", result.travelCorridors, 2);
        std::cout << ",\n";
        writeJsonArray(std::cout, "automationToolkit", result.automationToolkit, 2);
        std::cout << ",\n";
        std::cout << "  \"operationsChecklist\": \"" << escapeJson(narrow(result.operationsChecklist)) << "\",\n";
        std::cout << "  \"audit\": {\n";
        std::cout << "    \"foresightScore\": " << audit.foresightScore << ",\n";
        std::cout << "    \"structureScore\": " << audit.structureScore << ",\n";
        std::cout << "    \"ecosystemScore\": " << audit.ecosystemScore << ",\n";
        std::cout << "    \"workflowScore\": " << audit.workflowScore << ",\n";
        writeJsonArray(std::cout, "copilotModules", audit.copilotModules, 6);
        std::cout << ",\n";
        writeJsonArray(std::cout, "automationOpportunities", audit.automationOpportunities, 6);
        std::cout << ",\n";
        writeJsonArray(std::cout, "monitoringSignals", audit.monitoringSignals, 6);
        std::cout << "\n  }\n";
        std::cout << "}\n";

        return 0;
}
