// ======================================================================
//
// ContentCreationAssistantDialog.h
// Super modern content creation workflow driver for the SWG Viewer tool.
//
// ======================================================================

#ifndef INCLUDED_ContentCreationAssistantDialog_H
#define INCLUDED_ContentCreationAssistantDialog_H

#include "ContentCreationPreset.h"
#include "resource.h"

#include <vector>

class CViewerDoc;

// ----------------------------------------------------------------------

class ContentCreationAssistantDialog : public CDialog
{
public:
    explicit ContentCreationAssistantDialog(CViewerDoc *viewerDoc, CWnd *parent = 0);

    enum { IDD = IDD_CONTENT_CREATION_ASSISTANT };

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnFilterChanged();
    afx_msg void OnPresetSelectionChanged();
    afx_msg void OnCreateWorkspace();
    afx_msg void OnPreviewSkeleton();
    afx_msg void OnPreviewMesh();
    afx_msg void OnQueueAnimations();
    afx_msg void OnCopySummary();
    afx_msg void OnSaveWorkspace();
    afx_msg void OnSaveTemplate();
    afx_msg void OnApplyShader();
    afx_msg void OnCategoryChanged();
    afx_msg void OnToggleShowReady();
    afx_msg void OnAutomateBuild();
    afx_msg void OnCopyAvailability();

    DECLARE_MESSAGE_MAP()

private:
    typedef std::vector<ViewerContentPreset> PresetVector;
    typedef std::vector<int>                 IndexVector;
    typedef std::vector<CString>             CategoryVector;

private:
    void populatePresets();
    void populateCategories();
    void refreshPresetList();
    void updatePresetDetails();
    void pushStatus(const CString &statusText);
    CString buildSummary(const ViewerContentPreset &preset) const;
    CString buildAvailabilityReport(const ViewerContentPreset &preset) const;
    CString buildAutomationPlan(const ViewerContentPreset &preset) const;
    const ViewerContentPreset *getSelectedPreset() const;
    bool ensurePresetApplied(const ViewerContentPreset &preset, CString &statusText);
    void copyTextToClipboard(const CString &text);
    bool isPresetReady(const ViewerContentPreset &preset) const;
    CString formatStringList(const std::vector<CString> &values, const CString &separator) const;

private:
    CViewerDoc      *m_viewerDoc;
    CListBox        m_presetList;
    CComboBox       m_categoryCombo;
    CString         m_filterText;
    CString         m_selectedCategory;
    BOOL            m_autoQueueAnimations;
    BOOL            m_promptToSaveWorkspace;
    BOOL            m_showReadyOnly;
    PresetVector    m_presets;
    IndexVector     m_filteredIndices;
    CategoryVector  m_categories;
    int             m_lastAppliedPresetIndex;
};

// ----------------------------------------------------------------------

#endif // INCLUDED_ContentCreationAssistantDialog_H

// ======================================================================
