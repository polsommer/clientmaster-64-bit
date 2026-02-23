// ======================================================================
//
// ContentCreationAssistantDialog.cpp
//
// Drives the smart content assistant surface that streamlines authoring
// modern SWG assets inside the legacy viewer tool.
//
// ======================================================================

#include "FirstViewer.h"
#include "ContentCreationAssistantDialog.h"

#include "ViewerDoc.h"
#include "resource.h"

#include "sharedFile/TreeFile.h"

#include <algorithm>
#include <tchar.h>

// ----------------------------------------------------------------------

namespace
{
        const TCHAR *const s_allCategoryLabel = _T("All Presets");

        CString makeLower(const CString &value)
        {
                CString lower(value);
                lower.MakeLower();
                return lower;
        }

        CString joinStrings(const std::vector<CString> &values, const CString &separator)
        {
                CString result;

                for (std::vector<CString>::const_iterator it = values.begin(); it != values.end(); ++it)
                {
                        if (it != values.begin())
                                result += separator;

                        result += *it;
                }

                return result;
        }

        bool equalsNoCase(const CString &lhs, const CString &rhs)
        {
                return lhs.CompareNoCase(rhs) == 0;
        }

        bool fileExists(const CString &path)
        {
                if (!path.GetLength())
                        return false;

                CStringA ansiPath(path);
                return TreeFile::exists(ansiPath.GetString());
        }

        struct CategoryLessNoCase
        {
                bool operator()(const CString &lhs, const CString &rhs) const
                {
                        return lhs.CompareNoCase(rhs) < 0;
                }
        };
}

// ----------------------------------------------------------------------

BEGIN_MESSAGE_MAP(ContentCreationAssistantDialog, CDialog)
        ON_EN_CHANGE(IDC_CONTENT_FILTER, OnFilterChanged)
        ON_CBN_SELCHANGE(IDC_CONTENT_CATEGORY, OnCategoryChanged)
        ON_BN_CLICKED(IDC_CONTENT_SHOW_READY, OnToggleShowReady)
        ON_LBN_SELCHANGE(IDC_CONTENT_PRESET_LIST, OnPresetSelectionChanged)
        ON_BN_CLICKED(IDC_CONTENT_CREATE_WORKSPACE, OnCreateWorkspace)
        ON_BN_CLICKED(IDC_CONTENT_LOAD_SKELETON, OnPreviewSkeleton)
        ON_BN_CLICKED(IDC_CONTENT_LOAD_MESH, OnPreviewMesh)
        ON_BN_CLICKED(IDC_CONTENT_QUEUE_ANIMATIONS, OnQueueAnimations)
        ON_BN_CLICKED(IDC_CONTENT_COPY_SUMMARY, OnCopySummary)
        ON_BN_CLICKED(IDC_CONTENT_SAVE_WORKSPACE, OnSaveWorkspace)
        ON_BN_CLICKED(IDC_CONTENT_SAVE_TEMPLATE, OnSaveTemplate)
        ON_BN_CLICKED(IDC_CONTENT_APPLY_SHADER, OnApplyShader)
        ON_BN_CLICKED(IDC_CONTENT_AUTOMATE, OnAutomateBuild)
        ON_BN_CLICKED(IDC_CONTENT_COPY_AVAILABILITY, OnCopyAvailability)
END_MESSAGE_MAP()

// ----------------------------------------------------------------------

ContentCreationAssistantDialog::ContentCreationAssistantDialog(CViewerDoc *viewerDoc, CWnd *parent)
:       CDialog(ContentCreationAssistantDialog::IDD, parent),
        m_viewerDoc(viewerDoc),
        m_presetList(),
        m_categoryCombo(),
        m_filterText(),
        m_selectedCategory(s_allCategoryLabel),
        m_autoQueueAnimations(TRUE),
        m_promptToSaveWorkspace(TRUE),
        m_showReadyOnly(FALSE),
        m_presets(),
        m_filteredIndices(),
        m_categories(),
        m_lastAppliedPresetIndex(-1)
{
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::DoDataExchange(CDataExchange *pDX)
{
        CDialog::DoDataExchange(pDX);
        DDX_Control(pDX, IDC_CONTENT_PRESET_LIST, m_presetList);
        DDX_Control(pDX, IDC_CONTENT_CATEGORY, m_categoryCombo);
        DDX_Text(pDX, IDC_CONTENT_FILTER, m_filterText);
        DDX_CBString(pDX, IDC_CONTENT_CATEGORY, m_selectedCategory);
        DDX_Check(pDX, IDC_CONTENT_AUTO_QUEUE, m_autoQueueAnimations);
        DDX_Check(pDX, IDC_CONTENT_AUTO_SAVE, m_promptToSaveWorkspace);
        DDX_Check(pDX, IDC_CONTENT_SHOW_READY, m_showReadyOnly);
}

// ----------------------------------------------------------------------

BOOL ContentCreationAssistantDialog::OnInitDialog()
{
        CDialog::OnInitDialog();

        populatePresets();
        populateCategories();
        refreshPresetList();

        UpdateData(FALSE);

        return TRUE;
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::populatePresets()
{
	m_presets.clear();

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Heroic Biped");
		preset.description             = _T("Smart defaults for heroic humanoid protagonists.");
		preset.category                = _T("Characters");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_humanoid_basic.skt");
		preset.latMapping              = _T("appearance/lat/shared_humanoid_basic.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_player_human_female_body.mgn");
		preset.shaderTemplate          = _T("shader/character/player_character.sht");
		preset.defaultWorkspaceName    = _T("heroic_biped_workspace.saws");
		preset.quickTips               = _T("Includes idle, walk and run cycles with a ready-to-go animation controller.");
		preset.workflowNotes           = _T("Ideal for player avatar look-dev. Preset wiring matches the character customization and cinematic pipelines.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_idle_stand.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_walk_forward.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_run_forward.ans"));
		preset.tags.push_back(_T("Humanoid"));
		preset.tags.push_back(_T("Player"));
		preset.tags.push_back(_T("Biped"));
		preset.automationSteps.push_back(_T("Apply customization defaults through the Variable Set view once the preset loads."));
		preset.automationSteps.push_back(_T("Preview shader to validate subsurface and emissive channels."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Frontier Creature");
		preset.description             = _T("Quadruped creature kit with rich locomotion suggestions.");
		preset.category                = _T("Creatures");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_quadruped_medium.skt");
		preset.latMapping              = _T("appearance/lat/shared_quadruped_medium.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_creature_quadruped_body.mgn");
		preset.shaderTemplate          = _T("shader/creature/shared_creature_skin.sht");
		preset.defaultWorkspaceName    = _T("frontier_creature_workspace.saws");
		preset.quickTips               = _T("Great starting point for mounts or wildlife. Comes with turn-in-place animations.");
		preset.workflowNotes           = _T("Pairs with mount behaviors. LAT mapping wires in automatic gait switching.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_creature_idle.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_creature_walk.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_creature_run.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_creature_turn_left.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_creature_turn_right.ans"));
		preset.tags.push_back(_T("Quadruped"));
		preset.tags.push_back(_T("Mount"));
		preset.tags.push_back(_T("Creature"));
		preset.automationSteps.push_back(_T("Queue the locomotion pack to confirm foot planting before export."));
		preset.automationSteps.push_back(_T("Bake turning animations into the animation set after validation."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Astromech Droid");
		preset.description             = _T("Compact droid rig with hard-surface shading callouts.");
		preset.category                = _T("Droids");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_astromech.skt");
		preset.latMapping              = _T("appearance/lat/shared_astromech.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_astromech_droid_body.mgn");
		preset.shaderTemplate          = _T("shader/droid/shared_astromech_pbr.sht");
		preset.defaultWorkspaceName    = _T("astromech_workspace.saws");
		preset.quickTips               = _T("Auto-suggests dome rotation loops and projector extends.");
		preset.workflowNotes           = _T("Great for support droids. Shader preset includes grime and emissive passes for holo projectors.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_astromech_idle.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_astromech_roll_forward.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_astromech_dome_scan.ans"));
		preset.tags.push_back(_T("Droid"));
		preset.tags.push_back(_T("Mechanical"));
		preset.tags.push_back(_T("Utility"));
		preset.automationSteps.push_back(_T("Queue projector animations to ensure servo timing lines up."));
		preset.automationSteps.push_back(_T("Preview shader with neutral lighting to balance metallic roughness."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Starship Hangar Preview");
		preset.description             = _T("Ready-made setup for vehicle polish passes.");
		preset.category                = _T("Vehicles");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_vehicle_starship.skt");
		preset.latMapping              = _T("appearance/lat/shared_vehicle_starship.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_starship_freighter_body.mgn");
		preset.shaderTemplate          = _T("shader/vehicle/shared_vehicle_starship_pbr.sht");
		preset.defaultWorkspaceName    = _T("starship_hangar_workspace.saws");
		preset.quickTips               = _T("Pairs nicely with the lighting rig options in the viewer toolbar.");
		preset.workflowNotes           = _T("Designed for vehicle polish sweeps. Swap the lighting rig to simulate hangar spots and exterior flybys.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_starship_flyby.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_starship_land.ans"));
		preset.tags.push_back(_T("Starship"));
		preset.tags.push_back(_T("Vehicle"));
		preset.tags.push_back(_T("Polish"));
		preset.automationSteps.push_back(_T("Queue flight path animations to validate landing gear timing."));
		preset.automationSteps.push_back(_T("Preview shader to ensure panel wear highlights read under harsh lighting."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Scene Prop Wizard");
		preset.description             = _T("Zero-friction pipeline for props that do not require animation.");
		preset.category                = _T("Environment");
		preset.skeletonTemplate        = _T("");
		preset.latMapping              = _T("");
		preset.meshGenerator           = _T("appearance/mesh/shared_prop_display_model.mgn");
		preset.shaderTemplate          = _T("shader/prop/shared_prop_standard.sht");
		preset.defaultWorkspaceName    = _T("scene_prop_workspace.saws");
		preset.quickTips               = _T("Ideal for world-building objects or hero props.");
		preset.workflowNotes           = _T("Skip straight to shading and placement. Includes guidance for quick LOD sweeps.");
		preset.tags.push_back(_T("Prop"));
		preset.tags.push_back(_T("Environment"));
		preset.tags.push_back(_T("Static"));
		preset.automationSteps.push_back(_T("Preview shader variants to choose the right wear level."));
		preset.automationSteps.push_back(_T("Export a workspace template to share prop lighting with the world team."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Cinematic NPC");
		preset.description             = _T("High-detail humanoid preset tuned for cutscenes and holograms.");
		preset.category                = _T("Characters");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_humanoid_cinematic.skt");
		preset.latMapping              = _T("appearance/lat/shared_humanoid_cinematic.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_humanoid_cinematic_body.mgn");
		preset.shaderTemplate          = _T("shader/character/shared_cinematic_hologram.sht");
		preset.defaultWorkspaceName    = _T("cinematic_npc_workspace.saws");
		preset.quickTips               = _T("Includes suggestions for emote loops and hologram idle blends.");
		preset.workflowNotes           = _T("Perfect for story moments. Shader preset enables holographic bloom and emissive pulses.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_cinematic_idle.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_cinematic_talk.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_cinematic_emote_point.ans"));
		preset.tags.push_back(_T("Humanoid"));
		preset.tags.push_back(_T("Cinematic"));
		preset.tags.push_back(_T("Hologram"));
		preset.automationSteps.push_back(_T("Queue dialogue animations to check lip-sync helpers."));
		preset.automationSteps.push_back(_T("Preview shader with Scene Lighting > Hologram to verify scanline pulses."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Companion Beast");
		preset.description             = _T("Pet-sized creature tuned for companion systems and emote loops.");
		preset.category                = _T("Creatures");
		preset.skeletonTemplate        = _T("appearance/skeleton/shared_beast_companion.skt");
		preset.latMapping              = _T("appearance/lat/shared_beast_companion.lat");
		preset.meshGenerator           = _T("appearance/mesh/shared_creature_companion_body.mgn");
		preset.shaderTemplate          = _T("shader/creature/shared_companion_skin.sht");
		preset.defaultWorkspaceName    = _T("companion_beast_workspace.saws");
		preset.quickTips               = _T("Ships with playful emotes and idle variants ready to bind to AI cues.");
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_companion_idle.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_companion_sit.ans"));
		preset.recommendedAnimations.push_back(_T("appearance/animations/shared_companion_beg.ans"));
		preset.workflowNotes           = _T("Hook into pet AI quickly. LAT data includes vocalization hooks for barks and chirps.");
		preset.tags.push_back(_T("Pet"));
		preset.tags.push_back(_T("Creature"));
		preset.tags.push_back(_T("Companion"));
		preset.automationSteps.push_back(_T("Queue playful emotes to validate personality beats."));
		preset.automationSteps.push_back(_T("Export automation notes to the design team using Copy Summary."));
		m_presets.push_back(preset);
	}

	{
		ViewerContentPreset preset;
		preset.name                    = _T("Holographic Diorama");
		preset.description             = _T("Environment vignette preset for quick storytelling moments.");
		preset.category                = _T("Environment");
		preset.skeletonTemplate        = _T("");
		preset.latMapping              = _T("");
		preset.meshGenerator           = _T("appearance/mesh/shared_scene_hologram_tableau.mgn");
		preset.shaderTemplate          = _T("shader/environment/shared_hologram_fx.sht");
		preset.defaultWorkspaceName    = _T("holographic_diorama_workspace.saws");
		preset.quickTips               = _T("Great for mission terminals or ambient storytelling set dressing.");
		preset.workflowNotes           = _T("Preset automatically highlights emissive surfaces and suggests hologram flicker timing.");
		preset.tags.push_back(_T("Scene"));
		preset.tags.push_back(_T("Hologram"));
		preset.tags.push_back(_T("Story"));
		preset.automationSteps.push_back(_T("Preview shader to dial in hologram flicker and scan lines."));
		preset.automationSteps.push_back(_T("Save a workspace template to reuse layout across mission hubs."));
		m_presets.push_back(preset);
	}
}

// ----------------------------------------------------------------------

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::populateCategories()
{
	m_categories.clear();
	m_categories.push_back(s_allCategoryLabel);

	for (PresetVector::const_iterator it = m_presets.begin(); it != m_presets.end(); ++it)
	{
		const CString &category = it->category;
		if (category.IsEmpty())
			continue;

		bool alreadyPresent = false;
		for (CategoryVector::const_iterator catIt = m_categories.begin(); catIt != m_categories.end(); ++catIt)
		{
			if (equalsNoCase(*catIt, category))
			{
				alreadyPresent = true;
				break;
			}
		}

		if (!alreadyPresent)
			m_categories.push_back(category);
	}

	if (m_categories.size() > 1)
		std::sort(m_categories.begin() + 1, m_categories.end(), CategoryLessNoCase());

	if (m_categoryCombo.GetSafeHwnd())
		m_categoryCombo.ResetContent();

	for (CategoryVector::const_iterator it = m_categories.begin(); it != m_categories.end(); ++it)
	{
		if (m_categoryCombo.GetSafeHwnd())
			m_categoryCombo.AddString(*it);
	}

	int selection = 0;
	for (size_t i = 0; i < m_categories.size(); ++i)
	{
		if (equalsNoCase(m_categories[i], m_selectedCategory))
		{
			selection = static_cast<int>(i);
			break;
		}
	}

	if (m_categoryCombo.GetSafeHwnd())
		m_categoryCombo.SetCurSel(selection);

	if (!m_categories.empty())
		m_selectedCategory = m_categories[static_cast<size_t>(selection)];
	else
		m_selectedCategory = s_allCategoryLabel;
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::refreshPresetList()
{
	UpdateData(TRUE);

	CString filterCopy(m_filterText);
	filterCopy.Trim();
	CString loweredFilter = makeLower(filterCopy);

	const bool filterByCategory = !m_selectedCategory.IsEmpty() && !equalsNoCase(m_selectedCategory, s_allCategoryLabel);

	m_filteredIndices.clear();
	m_presetList.ResetContent();

	for (size_t i = 0; i < m_presets.size(); ++i)
	{
		const ViewerContentPreset &preset = m_presets[i];

		if (filterByCategory && !equalsNoCase(preset.category, m_selectedCategory))
			continue;

		const bool ready = isPresetReady(preset);
		if (m_showReadyOnly && !ready)
			continue;

		CString haystack = preset.name + _T(" ") + preset.description + _T(" ") + preset.quickTips + _T(" ") + preset.workflowNotes;
		if (!preset.category.IsEmpty())
		{
			haystack += _T(" ");
			haystack += preset.category;
		}
		if (!preset.tags.empty())
		{
			haystack += _T(" ");
			haystack += formatStringList(preset.tags, _T(" ") );
		}
		haystack.MakeLower();

		if (!loweredFilter.IsEmpty() && haystack.Find(loweredFilter) == -1)
			continue;

		m_filteredIndices.push_back(static_cast<int>(i));

		CString displayName;
		if (ready)
			displayName = _T("✓ ");
		displayName += preset.name;

		if (!preset.category.IsEmpty())
		{
			displayName += _T(" [");
			displayName += preset.category;
			displayName += _T("]");
		}

		m_presetList.AddString(displayName);
	}

	if (m_presetList.GetCount() > 0)
	{
		int selection = 0;

		if (m_lastAppliedPresetIndex >= 0)
		{
			for (size_t i = 0; i < m_filteredIndices.size(); ++i)
			{
				if (m_filteredIndices[i] == m_lastAppliedPresetIndex)
				{
					selection = static_cast<int>(i);
					break;
				}
			}
		}

		m_presetList.SetCurSel(selection);
	}

	updatePresetDetails();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::updatePresetDetails()
{
        const ViewerContentPreset *preset = getSelectedPreset();

        if (!preset)
        {
                SetDlgItemText(IDC_CONTENT_SUMMARY, _T("Select a preset to see intelligent recommendations."));
                SetDlgItemText(IDC_CONTENT_AVAILABILITY, _T(""));
                SetDlgItemText(IDC_CONTENT_PLAN, _T("Select a preset to generate an automation plan."));
                return;
        }

        SetDlgItemText(IDC_CONTENT_SUMMARY, buildSummary(*preset));
        SetDlgItemText(IDC_CONTENT_AVAILABILITY, buildAvailabilityReport(*preset));
        SetDlgItemText(IDC_CONTENT_PLAN, buildAutomationPlan(*preset));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::pushStatus(const CString &statusText)
{
        SetDlgItemText(IDC_CONTENT_STATUS, statusText);
}

// ----------------------------------------------------------------------

CString ContentCreationAssistantDialog::buildSummary(const ViewerContentPreset &preset) const
{
        CString summary;

        CString const category = preset.category.GetLength() ? preset.category : _T("<unspecified>");
        CString const tagLine = !preset.tags.empty() ? formatStringList(preset.tags, _T(", ")) : _T("<none>");

        summary.Format(_T("%s\r\n\r\nCategory: %s\r\nTags: %s\r\n\r\n%s\r\n\r\nSkeleton: %s\r\nMesh Generator: %s\r\nLAT Mapping: %s\r\nSuggested Shader: %s"),
                preset.name,
                category,
                tagLine,
                preset.description,
                preset.skeletonTemplate.GetLength() ? preset.skeletonTemplate : _T("<none>"),
                preset.meshGenerator.GetLength() ? preset.meshGenerator : _T("<none>"),
                preset.latMapping.GetLength() ? preset.latMapping : _T("<none>"),
                preset.shaderTemplate.GetLength() ? preset.shaderTemplate : _T("<none>"));

        if (!preset.quickTips.IsEmpty())
        {
                summary += _T("\r\n\r\nQuick Tips:\r\n  ");
                summary += preset.quickTips;
        }

        if (!preset.workflowNotes.IsEmpty())
        {
                summary += _T("\r\n\r\nWorkflow Notes:\r\n  ");
                summary += preset.workflowNotes;
        }

        if (!preset.recommendedAnimations.empty())
        {
                summary += _T("\r\n\r\nSuggested Animations:");
                for (std::vector<CString>::const_iterator it = preset.recommendedAnimations.begin(); it != preset.recommendedAnimations.end(); ++it)
                {
                        summary += _T("\r\n  - ");
                        summary += *it;
                }
        }

        return summary;
}

// ----------------------------------------------------------------------

CString ContentCreationAssistantDialog::buildAvailabilityReport(const ViewerContentPreset &preset) const
{
        CString report;

        int requiredAssets = 0;
        int availableAssets = 0;

        report += _T("Asset Availability:\r\n");

        const struct AssetCheck
        {
                LPCTSTR label;
                const CString *value;

                AssetCheck(LPCTSTR const labelIn, const CString &valueIn)
                : label(labelIn),
                  value(&valueIn)
                {
                }
        } checks[] =
        {
                AssetCheck(_T("  Skeleton"), preset.skeletonTemplate),
                AssetCheck(_T("  Mesh Generator"), preset.meshGenerator),
                AssetCheck(_T("  LAT"), preset.latMapping),
                AssetCheck(_T("  Shader"), preset.shaderTemplate)
        };

        for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i)
        {
                report += checks[i].label;
                report += _T(": ");

                const CString &value = *checks[i].value;

                if (value.GetLength())
                {
                        ++requiredAssets;
                        const bool exists = fileExists(value);
                        if (exists)
                                ++availableAssets;
                        report += exists ? _T("Found ✓") : _T("Missing ✗");
                }
                else
                {
                        report += _T("<not required>");
                }

                report += _T("\r\n");
        }

        if (!preset.recommendedAnimations.empty())
        {
                report += _T("\r\nRecommended Animations:\r\n");
                for (std::vector<CString>::const_iterator it = preset.recommendedAnimations.begin(); it != preset.recommendedAnimations.end(); ++it)
                {
                        if (!(*it).GetLength())
                                continue;

                        ++requiredAssets;
                        const bool exists = fileExists(*it);
                        if (exists)
                                ++availableAssets;

                        report += _T("  - ");
                        report += *it;
                        report += _T(" : ");
                        report += exists ? _T("Found ✓") : _T("Missing ✗");
                        report += _T("\r\n");
                }
        }

        const int readinessPercent = requiredAssets > 0 ? (availableAssets * 100) / requiredAssets : 100;
        report.AppendFormat(_T("\r\nReadiness: %d/%d assets (%d%%)"), availableAssets, requiredAssets, readinessPercent);

        if (preset.defaultWorkspaceName.GetLength())
        {
                        report += _T("\r\nDefault Workspace: ");
                        report += preset.defaultWorkspaceName;
        }

        return report;
}

// ----------------------------------------------------------------------

CString ContentCreationAssistantDialog::buildAutomationPlan(const ViewerContentPreset &preset) const
{
        CString plan;

        plan += _T("Automation Plan:\r\n");

        int step = 1;
        plan.AppendFormat(_T("%d. Apply preset to load the configured assets."), step++);

        if (!preset.recommendedAnimations.empty())
        {
                plan.AppendFormat(_T("\r\n%d. Queue recommended animations to validate motion coverage."), step++);
        }

        if (preset.shaderTemplate.GetLength())
        {
                plan.AppendFormat(_T("\r\n%d. Preview shader %s to evaluate material response."), step++, preset.shaderTemplate.GetString());
        }

        if (preset.defaultWorkspaceName.GetLength())
        {
                plan.AppendFormat(_T("\r\n%d. Save workspace as %s to share with collaborators."), step++, preset.defaultWorkspaceName.GetString());
        }

        plan += _T("\r\n");
        plan += isPresetReady(preset) ? _T("Status: Ready to build.") : _T("Status: Missing assets detected (see availability report).");

        if (!preset.automationSteps.empty())
        {
                plan += _T("\r\n\r\nAssistant Notes:");
                for (std::vector<CString>::const_iterator it = preset.automationSteps.begin(); it != preset.automationSteps.end(); ++it)
                {
                        if ((*it).IsEmpty())
                                continue;

                        plan += _T("\r\n  - ");
                        plan += *it;
                }
        }

        if (!preset.workflowNotes.IsEmpty())
        {
                plan += _T("\r\n\r\nWorkflow Notes:\r\n  ");
                plan += preset.workflowNotes;
        }

        return plan;
}

// ----------------------------------------------------------------------

bool ContentCreationAssistantDialog::isPresetReady(const ViewerContentPreset &preset) const
{
        if (preset.skeletonTemplate.GetLength() && !fileExists(preset.skeletonTemplate))
                return false;

        if (preset.meshGenerator.GetLength() && !fileExists(preset.meshGenerator))
                return false;

        if (preset.latMapping.GetLength() && !fileExists(preset.latMapping))
                return false;

        if (preset.shaderTemplate.GetLength() && !fileExists(preset.shaderTemplate))
                return false;

        for (std::vector<CString>::const_iterator it = preset.recommendedAnimations.begin(); it != preset.recommendedAnimations.end(); ++it)
        {
                if ((*it).GetLength() && !fileExists(*it))
                        return false;
        }

        return true;
}

// ----------------------------------------------------------------------

CString ContentCreationAssistantDialog::formatStringList(const std::vector<CString> &values, const CString &separator) const
{
        if (values.empty())
                return CString();

        CString joined;

        for (std::vector<CString>::const_iterator it = values.begin(); it != values.end(); ++it)
        {
                if (it != values.begin())
                        joined += separator;

                joined += *it;
        }

        return joined;
}

// ----------------------------------------------------------------------

const ViewerContentPreset *ContentCreationAssistantDialog::getSelectedPreset() const
{
        const int selection = m_presetList.GetCurSel();
        if (selection == LB_ERR)
                return 0;

        if (selection < 0 || selection >= static_cast<int>(m_filteredIndices.size()))
                return 0;

        const int presetIndex = m_filteredIndices[static_cast<size_t>(selection)];
        if (presetIndex < 0 || presetIndex >= static_cast<int>(m_presets.size()))
                return 0;

        return &m_presets[static_cast<size_t>(presetIndex)];
}

// ----------------------------------------------------------------------

bool ContentCreationAssistantDialog::ensurePresetApplied(const ViewerContentPreset &preset, CString &statusText)
{
        if (!m_viewerDoc)
        {
                statusText = _T("Open a model or create a new document before applying presets.");
                return false;
        }

        if (m_viewerDoc->applyContentPreset(preset, statusText))
        {
                // Remember which preset was applied so we can auto-select it next time.
                for (size_t i = 0; i < m_presets.size(); ++i)
                {
                        if (&m_presets[i] == &preset)
                        {
                                m_lastAppliedPresetIndex = static_cast<int>(i);
                                break;
                        }
                }

                return true;
        }

        return false;
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::copyTextToClipboard(const CString &text)
{
        if (!OpenClipboard())
                return;

        if (!EmptyClipboard())
        {
                CloseClipboard();
                return;
        }

#ifdef _UNICODE
        const size_t bytesRequired = (text.GetLength() + 1) * sizeof(wchar_t);
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytesRequired);
        if (!handle)
        {
                CloseClipboard();
                return;
        }

        wchar_t *buffer = static_cast<wchar_t*>(GlobalLock(handle));
        if (!buffer)
        {
                GlobalFree(handle);
                CloseClipboard();
                return;
        }

        memcpy(buffer, text.GetString(), bytesRequired);
        GlobalUnlock(handle);

        SetClipboardData(CF_UNICODETEXT, handle);
#else
        const size_t bytesRequired = text.GetLength() + 1;
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytesRequired);
        if (!handle)
        {
                CloseClipboard();
                return;
        }

        char *buffer = static_cast<char*>(GlobalLock(handle));
        if (!buffer)
        {
                GlobalFree(handle);
                CloseClipboard();
                return;
        }

        memcpy(buffer, text.GetString(), bytesRequired);
        GlobalUnlock(handle);

        SetClipboardData(CF_TEXT, handle);
#endif

        CloseClipboard();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnFilterChanged()
{
        refreshPresetList();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnCategoryChanged()
{
        refreshPresetList();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnToggleShowReady()
{
        refreshPresetList();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnPresetSelectionChanged()
{
        updatePresetDetails();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnCreateWorkspace()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset to apply."));
                return;
        }

        CString statusText;
        if (ensurePresetApplied(*preset, statusText))
        {
                pushStatus(statusText);

                if (m_autoQueueAnimations && !preset->recommendedAnimations.empty() && m_viewerDoc)
                {
                        m_viewerDoc->queuePresetAnimations(preset->recommendedAnimations, true);
                        pushStatus(statusText + _T(" Animations queued."));
                }

                if (m_promptToSaveWorkspace)
                        OnSaveWorkspace();
        }
        else
        {
                pushStatus(statusText);
        }

        updatePresetDetails();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnPreviewSkeleton()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("No preset selected."));
                return;
        }

        if (!preset->skeletonTemplate.GetLength())
        {
                pushStatus(_T("Preset does not define a skeleton template."));
                return;
        }

        if (!m_viewerDoc)
        {
                pushStatus(_T("Viewer document unavailable."));
                return;
        }

        CStringA fileName(preset->skeletonTemplate);

        if (!TreeFile::exists(fileName.GetString()))
        {
                pushStatus(_T("Skeleton template not found in the active tree."));
                return;
        }

        if (m_viewerDoc->previewSkeletonTemplate(fileName))
                pushStatus(_T("Skeleton preview loaded."));
        else
                pushStatus(_T("Failed to load skeleton preview."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnPreviewMesh()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("No preset selected."));
                return;
        }

        if (!preset->meshGenerator.GetLength())
        {
                pushStatus(_T("Preset does not define a mesh generator."));
                return;
        }

        if (!m_viewerDoc)
        {
                pushStatus(_T("Viewer document unavailable."));
                return;
        }

        CStringA fileName(preset->meshGenerator);
        if (!TreeFile::exists(fileName.GetString()))
        {
                pushStatus(_T("Mesh generator not found in the active tree."));
                return;
        }

        if (m_viewerDoc->previewMeshGenerator(fileName))
                pushStatus(_T("Mesh preview loaded."));
        else
                pushStatus(_T("Failed to load mesh preview."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnQueueAnimations()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("No preset selected."));
                return;
        }

        if (!m_viewerDoc)
        {
                pushStatus(_T("Viewer document unavailable."));
                return;
        }

        if (preset->recommendedAnimations.empty())
        {
                pushStatus(_T("Preset does not declare recommended animations."));
                return;
        }

        m_viewerDoc->queuePresetAnimations(preset->recommendedAnimations, false);
        pushStatus(_T("Recommended animations queued."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnAutomateBuild()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset before running automation."));
                return;
        }

        CString statusText;
        if (!ensurePresetApplied(*preset, statusText))
        {
                pushStatus(statusText);
                return;
        }

        CString combinedStatus(statusText);

        if (m_viewerDoc && !preset->recommendedAnimations.empty())
        {
                m_viewerDoc->queuePresetAnimations(preset->recommendedAnimations, false);
                combinedStatus += _T(" Animations queued.");
        }

        if (m_viewerDoc && preset->shaderTemplate.GetLength())
        {
                CString shaderStatus;
                m_viewerDoc->previewPresetShader(preset->shaderTemplate, shaderStatus);
                combinedStatus += _T(" ");
                combinedStatus += shaderStatus;
        }

        if (m_promptToSaveWorkspace)
        {
                pushStatus(combinedStatus);
                OnSaveWorkspace();
        }
        else
        {
                pushStatus(combinedStatus);
        }

        updatePresetDetails();
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnCopySummary()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("No preset selected."));
                return;
        }

        copyTextToClipboard(buildSummary(*preset));
        pushStatus(_T("Preset summary copied to clipboard."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnCopyAvailability()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset before copying availability."));
                return;
        }

        copyTextToClipboard(buildAvailabilityReport(*preset));
        pushStatus(_T("Availability report copied to clipboard."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnSaveWorkspace()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset before saving a workspace."));
                return;
        }

        CString statusText;
        if (!ensurePresetApplied(*preset, statusText))
        {
                pushStatus(statusText);
                return;
        }

        CString defaultName = preset->defaultWorkspaceName;
        if (defaultName.IsEmpty())
        {
                defaultName = preset->name;
                defaultName.Replace(_T(' '), _T('_'));
                defaultName.MakeLower();
                defaultName += _T(".saws");
        }

        CFileDialog dlg(FALSE, _T("saws"), defaultName, OFN_OVERWRITEPROMPT, _T("Skeletal Appearance Workspace (*.saws)|*.saws||"));
        if (dlg.DoModal() != IDOK)
                return;

        if (m_viewerDoc->saveSkeletalAppearanceWorkspace(dlg.GetPathName()))
                pushStatus(_T("Workspace saved."));
        else
                pushStatus(_T("Failed to save workspace."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnSaveTemplate()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset before saving a template."));
                return;
        }

        CString statusText;
        if (!ensurePresetApplied(*preset, statusText))
        {
                pushStatus(statusText);
                return;
        }

        CString defaultName = preset->name;
        defaultName.Replace(_T(' '), _T('_'));
        defaultName.MakeLower();
        defaultName += _T(".sat");

        CFileDialog dlg(FALSE, _T("sat"), defaultName, OFN_OVERWRITEPROMPT, _T("Skeletal Appearance Template (*.sat)|*.sat||"));
        if (dlg.DoModal() != IDOK)
                return;

        CStringA filename(dlg.GetPathName());
        m_viewerDoc->saveSkeletalAppearanceTemplate(filename);
        pushStatus(_T("Template saved."));
}

// ----------------------------------------------------------------------

void ContentCreationAssistantDialog::OnApplyShader()
{
        const ViewerContentPreset *preset = getSelectedPreset();
        if (!preset)
        {
                pushStatus(_T("Select a preset to preview its shader."));
                return;
        }

        if (!m_viewerDoc)
        {
                pushStatus(_T("Viewer document unavailable."));
                return;
        }

        CString statusText;
        if (m_viewerDoc->previewPresetShader(preset->shaderTemplate, statusText))
                pushStatus(statusText);
        else
                pushStatus(statusText);
}

// ======================================================================
