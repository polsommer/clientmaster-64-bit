#if !defined(AFX_PROPERTIESPAGE_H__1C7E5F78_6F0B_4AA1_8755_5DEDBCDB709B__INCLUDED_)
#define AFX_PROPERTIESPAGE_H__1C7E5F78_6F0B_4AA1_8755_5DEDBCDB709B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PropertiesPage.h : header file
//

#include "ObjectPropertiesEditor.h"

#include "UIPropertyDescriptor.h"

#include <map>
#include <vector>

class UIBaseObject;
class ObjectEditor;
class PropertyListControl;

namespace PropertiesPageNamespace {
        class Property;
        class PropertyList;
}

/////////////////////////////////////////////////////////////////////////////
// PropertiesPage dialog

class PropertiesPage : public CPropertyPage, public ObjectPropertiesEditor::PropertyCategory
{
// Construction
public:
	PropertiesPage(ObjectEditor &i_editor, UIPropertyCategories::Category i_category);
	~PropertiesPage();

// Dialog Data
	//{{AFX_DATA(PropertiesPage)
	enum { IDD = IDD_PROPERTIES_PAGE };
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(PropertiesPage)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation

protected:

	// ----------------------------------------------------------------

	virtual ObjectPropertiesEditor::PropertyList *_newPropertyList(
		const UIPropertyGroup &propertyGroup, 
		const PropertyListMap::iterator &insertionPoint
	);

        virtual void _freePropertyList(
                ObjectPropertiesEditor::PropertyList *pl,
                const PropertyListMap::iterator &listIter
        );

        // ----------------------------------------------------------------

        void scheduleFilterUpdate();
        void applyFilter();
        void saveFilterPreferences() const;
        void loadFilterPreferences();
        void saveFilterHistory() const;
        void loadFilterHistory();
        void updateFilterHistory(const CString &filterText);
        void refreshFilterHistoryCombo();
        void updateFilterSummary();
        void repositionPropertyLists();

        bool isFavoriteProperty(const PropertiesPageNamespace::Property &property) const;
        void toggleFavorite(PropertiesPageNamespace::Property &property);
        CString formatDisplayName(const PropertiesPageNamespace::Property &property) const;
        void onFavoritesChanged();
        bool isFilterActive() const;
        void onPropertyContentChanged(PropertiesPageNamespace::PropertyList &list);
        void setFilterText(const CString &text);

        // Generated message map functions
        //{{AFX_MSG(PropertiesPage)
        virtual BOOL OnInitDialog();
        afx_msg void OnSize(UINT nType, int cx, int cy);
        afx_msg void OnDestroy();
        afx_msg void OnTimer(UINT_PTR nIDEvent);
        afx_msg void OnFilterTextChanged();
        afx_msg void OnFilterHistorySelected();
        afx_msg void OnClearFilter();
        afx_msg void OnFilterOptionClicked();
        //}}AFX_MSG
        DECLARE_MESSAGE_MAP()

private:

        enum { FilterUpdateTimerId = 1 };

        enum { MaxFilterHistory = 10 };

        CComboBox m_filterCombo;
        CButton  m_filterClearButton;
        CButton  m_matchValuesCheck;
        CButton  m_caseSensitiveCheck;
        CButton  m_favoritesOnlyCheck;
        CButton  m_hideReadOnlyCheck;
        CStatic  m_filterSummary;

        CString  m_pendingFilterText;
        UINT_PTR m_filterUpdateTimer;
        int      m_contentTop;
        bool     m_filterControlsInitialized;
        bool     m_isUpdatingFilterText;
        std::vector<CString> m_filterHistory;

        friend class PropertiesPageNamespace::Property;
        friend class PropertiesPageNamespace::PropertyList;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTIESPAGE_H__1C7E5F78_6F0B_4AA1_8755_5DEDBCDB709B__INCLUDED_)
