// ======================================================================
//
// ContentCreationPreset.h
// Modern content creation helpers for the SWG Viewer tool.
//
// This header exposes data structures that are shared between the
// content assistant dialog and the viewer document so that presets can
// be exchanged without either component having to know about the other.
//
// ======================================================================

#ifndef INCLUDED_ContentCreationPreset_H
#define INCLUDED_ContentCreationPreset_H

#include <afxstr.h>
#include <vector>

// ----------------------------------------------------------------------

struct ViewerContentPreset
{
    CString                        name;
    CString                        description;
    CString                        category;
    CString                        skeletonTemplate;
    CString                        latMapping;
    CString                        meshGenerator;
    CString                        shaderTemplate;
    CString                        defaultWorkspaceName;
    CString                        quickTips;
    CString                        workflowNotes;
    std::vector<CString>           recommendedAnimations;
    std::vector<CString>           tags;
    std::vector<CString>           automationSteps;

    ViewerContentPreset();
};

// ----------------------------------------------------------------------

inline ViewerContentPreset::ViewerContentPreset()
:       name(),
        description(),
        category(),
        skeletonTemplate(),
        latMapping(),
        meshGenerator(),
        shaderTemplate(),
        defaultWorkspaceName(),
        quickTips(),
        workflowNotes(),
        recommendedAnimations(),
        tags(),
        automationSteps()
{
}

// ----------------------------------------------------------------------

#endif // INCLUDED_ContentCreationPreset_H

// ======================================================================
