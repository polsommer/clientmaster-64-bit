// PropertiesPage.cpp : implementation file
//

#include "FirstUiBuilder.h"
#include "PropertiesPage.h"
#include "EditUtils.h"
#include "ObjectEditor.h"
#include "EditorMonitor.h"
#include "PropertyListControl.h"
#include "UIBaseObject.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>

namespace
{
        std::set<std::string> &favoriteStorage()
        {
                static std::set<std::string> s_favorites;
                return s_favorites;
        }

        bool &favoritesLoaded()
        {
                static bool s_loaded = false;
                return s_loaded;
        }

        void ensureFavoritesLoaded()
        {
                if (favoritesLoaded())
                        return;

                favoritesLoaded() = true;

                CWinApp *const app = AfxGetApp();
                if (!app)
                        return;

                CString raw = app->GetProfileString(_T("PropertyBrowser"), _T("Favorites"), _T(""));
                CStringA rawAnsi(raw);
                std::istringstream stream(rawAnsi.GetString());
                std::string line;

                while (std::getline(stream, line))
                {
                        if (!line.empty())
                                favoriteStorage().insert(line);
                }
        }

        void persistFavorites()
        {
                CWinApp *const app = AfxGetApp();
                if (!app)
                        return;

                const std::set<std::string> &favorites = favoriteStorage();
                std::ostringstream stream;
                bool first = true;
                for (std::set<std::string>::const_iterator iter = favorites.begin(); iter != favorites.end(); ++iter)
                {
                        if (!first)
                                stream << '\n';

                        stream << *iter;
                        first = false;
                }

                const std::string serialized = stream.str();
                CString stored(serialized.c_str());
                app->WriteProfileString(_T("PropertyBrowser"), _T("Favorites"), stored);
        }
}

namespace PropertiesPageNamespace {

	// ================================================================

	typedef UIBaseObject::UIPropertyGroupVector  UIPropertyGroupVector;

	typedef UIBaseObject::UIObjectVector         UIObjectVector;

	// ================================================================

	class PropertyList;

	class Property : public PropertyListControl::Property
	{
	public:

		typedef PropertyListControl::Property baseclass;

		// ---------------------------------------

		Property(
			PropertyListControl        &ownerControl, 
			const UIPropertyDescriptor &i_descriptor, 
			PropertyList               &ownerList
		)
		:	PropertyListControlProperty(ownerControl, i_descriptor),
			m_ownerList(ownerList)
		{}

		// ---------------------------------------

		ObjectEditor         &getEditor();
		const UIObjectVector &getObjects() const;
		UIBaseObject         *getAnchor();

		// ---------------------------------------

                virtual bool setValue(const CString &i_newValue);
                virtual void onFollowLink();
                virtual CString getDisplayName() const;
                virtual bool canCopyIdentifier() const;
                virtual CString getIdentifierForCopy() const;
                virtual bool supportsFavorites() const;
                virtual bool isFavorite() const;
                virtual void toggleFavorite();

                void updateValue(const UIBaseObject &anchorSelection);
                std::string getIdentifier() const;

                // ---------------------------------------

                PropertyList                &m_ownerList;
	};

	// ================================================================


	// ================================================================

	class PropertyList : public ObjectPropertiesEditor::PropertyList
	{
	public:

                enum { PAD = 4, HorizontalMargin = 4, FontHeight = 20 };

		PropertyList(const UIPropertyGroup &i_propertyGroup, PropertiesPage &owner, int topY);
		~PropertyList();

		PropertiesPage &getOwner() { return *static_cast<PropertiesPage *>(&m_owner); }

		// -----------------------------------------------

		Property   *getProperty(int i) const { return m_dialogProperties[i]; }

		int getTopY()    const { return m_topY; }
		int getBottomY() const { return m_bottomY; }
		int getHeight()  const { return getBottomY() - getTopY(); }

		// -----------------------------------------------

                void createControls(int topY);

                void setTopY(int newTopY);
                void onOwnerResized();

                void applyFilter(const CString &filterText, bool matchValues, bool caseSensitive, bool favoritesOnly, bool hideReadOnly);
                void refreshDisplayNames();
                int  getVisiblePropertyCount() const { return m_visiblePropertyCount; }
                int  getVisibleFavoriteCount() const { return m_visibleFavoriteCount; }
                int  getTotalFavoriteCount() const { return m_totalFavoriteCount; }
                int  getDisplayablePropertyCount() const { return m_displayablePropertyCount; }

		// -----------------------------------------------
		virtual void clear();
		virtual void onPropertyGroupChanged();
		virtual void addObject(UIBaseObject &o);
		virtual bool removeObject(UIBaseObject &o);
		virtual void onSetValue(UIBaseObject &o, const char *i_propertyName);
		// -----------------------------------------------

		UIBaseObject *getAnchor() { if (empty()) return 0; else return _getAnchor(); }
		UIBaseObject *_getAnchor() { return m_objects.back(); }

		void _initializeListControl(const CRect *listRect=0);
		void _resizeListControl(const CRect *listRect=0);
		void _updateValues(UIBaseObject &o);
		void _retextLabel();

		// -----------------------------------------------

		Property              **m_dialogProperties;

		CButton                *m_label;
                PropertyListControl    *m_listControl;
                int                     m_topY;
                int                     m_listY;
                int                     m_bottomY;
                int                     m_visiblePropertyCount;
                int                     m_visibleFavoriteCount;
                int                     m_totalFavoriteCount;
                int                     m_displayablePropertyCount;
        };

	PropertyList::PropertyList(const UIPropertyGroup &i_propertyGroup, PropertiesPage &owner, int topY)
	:	ObjectPropertiesEditor::PropertyList(i_propertyGroup, owner),
		m_dialogProperties(0),
		m_label(0),
		m_listControl(0),
		m_topY(topY),
                m_listY(0),
                m_bottomY(0),
                m_visiblePropertyCount(0),
                m_visibleFavoriteCount(0),
                m_totalFavoriteCount(0),
                m_displayablePropertyCount(0)
	{
		if (owner.m_hWnd)
		{
			createControls(topY);
		}
	}

	// -------------------------------------------------------------------

	PropertyList::~PropertyList()
	{
		delete m_label;
		delete m_listControl;
		delete [] m_dialogProperties;
	}

	// -------------------------------------------------------------------

	void PropertyList::createControls(int topY)
	{
		// --------------------------
		// set new topY value
		if (m_listControl)
		{
			if (topY != getTopY())
			{
				setTopY(topY);
			}
			return;
		}
		m_topY=topY;
		// --------------------------

                const int fontHeight = FontHeight;

		CRect pageRect;
		getOwner().GetClientRect(&pageRect);

		// -------------------------------------------------------------------
		// Create group label.
		CRect staticRect;
                staticRect.left = pageRect.left + HorizontalMargin;
                staticRect.right = pageRect.right - HorizontalMargin;
		staticRect.top = m_topY;
		staticRect.bottom = m_topY + fontHeight;
		m_label = new CButton();
		m_label->Create(getGroupName(), BS_LEFTTEXT | WS_CHILD | WS_VISIBLE, staticRect, &getOwner(), -1);
		_retextLabel();
		// -------------------------------------------------------------------

		// -------------------------------------------------------------------
		// Create property list control
		m_listY = staticRect.bottom;

		CRect listRect;
                listRect.left = pageRect.left + HorizontalMargin;
                listRect.right = pageRect.right - HorizontalMargin;
		listRect.top = m_listY;
		listRect.bottom = listRect.top + getPropertyCount()*fontHeight;

		m_listControl = new PropertyListControl();
		m_listControl->Create(
			LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | WS_BORDER | LVS_NOCOLUMNHEADER, 
			listRect, 
			&getOwner(), 
			-1
		);
		// -------------------------------------------------------------------

		// -------------------------------------------------------------------
		// Initialize and show the list control
		_initializeListControl(&listRect);

		m_listControl->ShowWindow(SW_SHOW);
		// -------------------------------------------------------------------

		// -------------------------------------------------------------------
		if (!m_objects.empty())
		{
			_updateValues(*_getAnchor());
		}
	}

	// -------------------------------------------------------------------

	void PropertyList::setTopY(int newTopY)
	{
		if (newTopY == m_topY)
		{
			return;
		}

		m_topY = newTopY;
		onOwnerResized();
	}

void PropertyList::onOwnerResized()
{
        CRect pageRect;
        getOwner().GetClientRect(&pageRect);

        const int labelTop = m_topY;
        const int labelBottom = labelTop + FontHeight;

        if (m_label)
        {
                CRect labelRect(pageRect.left + HorizontalMargin, labelTop, pageRect.right - HorizontalMargin, labelBottom);
                m_label->MoveWindow(&labelRect);
                m_listY = labelRect.bottom;
        }
        else
        {
                m_listY = labelBottom;
        }

        if (m_listControl)
        {
                _resizeListControl();
        }
        else
        {
                m_bottomY = m_listY;
        }
}

void PropertyList::applyFilter(const CString &filterText, bool matchValues, bool caseSensitive, bool favoritesOnly, bool hideReadOnly)
{
        if (!m_listControl || !m_dialogProperties)
        {
                return;
        }

        CString trimmed(filterText);
        trimmed.Trim();
        CString compareFilter(trimmed);
        if (!caseSensitive)
                compareFilter.MakeLower();

        const bool hasFilter = !compareFilter.IsEmpty();

        UIBaseObject *anchor = getAnchor();
        if (anchor)
        {
                for (int i = 0; i < getPropertyCount(); ++i)
                        m_dialogProperties[i]->updateValue(*anchor);
        }

        m_listControl->SetRedraw(FALSE);
        m_listControl->clear();

        int visibleCount = 0;
        int visibleFavorites = 0;
        int totalFavorites = 0;
        int displayableCount = 0;

        for (int i = 0; i < getPropertyCount(); ++i)
        {
                Property *property = m_dialogProperties[i];
                const bool favorite = getOwner().isFavoriteProperty(*property);
                const bool readOnly = property->m_descriptor.isReadOnly();

                bool displayable = true;
                if (hideReadOnly && readOnly)
                        displayable = false;
                if (favoritesOnly && !favorite)
                        displayable = false;

                if (displayable)
                {
                        ++displayableCount;
                        if (favorite)
                                ++totalFavorites;
                }

                bool include = displayable;

                if (include && hasFilter)
                {
                        CString nameCandidate(property->m_descriptor.m_name ? property->m_descriptor.m_name : "");
                        if (!caseSensitive)
                                nameCandidate.MakeLower();

                        include = nameCandidate.Find(compareFilter) != -1;

                        if (!include && matchValues)
                        {
                                CString valueCandidate(property->getValueForCopy());
                                if (!caseSensitive)
                                        valueCandidate.MakeLower();

                                include = valueCandidate.Find(compareFilter) != -1;
                        }
                }

                if (include)
                {
                        m_listControl->addProperty(*property);
                        ++visibleCount;
                        if (favorite)
                                ++visibleFavorites;
                }
        }

        m_visiblePropertyCount = visibleCount;
        m_visibleFavoriteCount = visibleFavorites;
        m_totalFavoriteCount = totalFavorites;
        m_displayablePropertyCount = displayableCount;

        _resizeListControl();
        refreshDisplayNames();

        m_listControl->SetRedraw(TRUE);
        m_listControl->Invalidate();

        _retextLabel();
}

void PropertyList::refreshDisplayNames()
{
        if (!m_listControl)
                return;

        const int count = m_listControl->GetItemCount();
        for (int i = 0; i < count; ++i)
        {
                Property *property = reinterpret_cast<Property *>(m_listControl->GetItemData(i));
                if (!property)
                        continue;

                const CString displayName = property->getDisplayName();
                m_listControl->SetItemText(i, 0, displayName);
        }
}

// -------------------------------------------------------------------------------

        void PropertyList::clear()
        {
                m_objects.clear();
                if (m_dialogProperties)
                {
                        delete [] m_dialogProperties;
                        m_dialogProperties=0;
                }
                if (m_listControl)
                {
                        m_listControl->clear();
                        _resizeListControl();
                }
                m_visiblePropertyCount = 0;
                m_visibleFavoriteCount = 0;
                m_totalFavoriteCount = 0;
                m_displayablePropertyCount = 0;
        }

	// -------------------------------------------------------------------------------

        void PropertyList::onPropertyGroupChanged()
        {
                if (m_listControl)
                {
                        m_listControl->clear();
                }
                if (m_dialogProperties)
                {
                        delete [] m_dialogProperties;
                        m_dialogProperties=0;
                }

                m_visiblePropertyCount = 0;
                m_visibleFavoriteCount = 0;
                m_totalFavoriteCount = 0;

                _initializeListControl();
                // TODO - notify parent to re-pack dialog??
        }

	// -------------------------------------------------------------------------------

        void PropertyList::addObject(UIBaseObject &o)
        {
                m_objects.push_back(&o);
                _updateValues(o);
                _retextLabel();
                getOwner().onPropertyContentChanged(*this);
        }

	// -------------------------------------------------------------------------------

        bool PropertyList::removeObject(UIBaseObject &o)
        {
                std::vector<UIBaseObject *>::iterator oi = std::find(m_objects.begin(), m_objects.end(), &o);
                if (oi==m_objects.end())
		{
			return false;
		}

		const bool wasAnchor = (oi==m_objects.begin());

		m_objects.erase(oi);

                if (wasAnchor && !m_objects.empty())
                {
                        _updateValues(*_getAnchor());
                }

                getOwner().onPropertyContentChanged(*this);
                return true;
        }

	// -------------------------------------------------------------------------------

        void PropertyList::onSetValue(UIBaseObject &o, const char *i_propertyName)
        {
                if (!m_objects.empty() && &o==_getAnchor())
                {
                        _updateValues(o);
                }
                getOwner().onPropertyContentChanged(*this);
        }

	// -------------------------------------------------------------------------------

	void PropertyList::_initializeListControl(const CRect *i_listRect)
	{
		assert(!m_dialogProperties);
		if (!m_listControl)
		{
			return;
		}
		assert(m_listControl->empty());

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

		const UIPropertyDescriptor *descs = m_propertyGroup.fields;
		const int fieldCount = getPropertyCount();

		m_dialogProperties = new Property *[fieldCount];
                for (int i=0;i<fieldCount;i++)
                {
                        const UIPropertyDescriptor &pd = descs[i];
                        Property *p = new Property(*m_listControl, pd, *this);
                        m_dialogProperties[i] = p;
                        m_listControl->addProperty(*p);
                }

                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

                _resizeListControl(i_listRect);

                m_visiblePropertyCount = getPropertyCount();
                m_visibleFavoriteCount = 0;
                m_totalFavoriteCount = 0;

                for (int i=0; i<getPropertyCount(); ++i)
                {
                        if (getOwner().isFavoriteProperty(*m_dialogProperties[i]))
                                ++m_totalFavoriteCount;
                }

                refreshDisplayNames();
        }

	// -------------------------------------------------------------------------------

        void PropertyList::_resizeListControl(const CRect *i_listRect)
        {
                if (!m_listControl)
                        return;

                CRect listRect;
                if (i_listRect)
                {
                        listRect=*i_listRect;
                }
                else
                {
                        getOwner().GetClientRect(&listRect);
                }
                listRect.left += HorizontalMargin;
                listRect.right -= HorizontalMargin;

                if (listRect.right < listRect.left)
                        listRect.right = listRect.left;

                const int itemCount = m_listControl->GetItemCount();
                const int approximateCount = itemCount > 0 ? itemCount : 1;
                const CSize listSize = m_listControl->ApproximateViewRect(CSize(listRect.Width(), -1), approximateCount);
                listRect.top = m_listY;
                listRect.bottom = listRect.top + listSize.cy;
                m_listControl->MoveWindow(listRect);
                m_bottomY = listRect.bottom + PAD;
        }

	// -------------------------------------------------------------------------------

	void PropertyList::_updateValues(UIBaseObject &o)
	{
		if (!m_listControl)
		{
			return;
		}
		for (int i=0;i<getPropertyCount();i++)
		{
			Property *p = m_dialogProperties[i];
			p->updateValue(o);
		}
	}

	// -------------------------------------------------------------------------------

        void PropertyList::_retextLabel()
        {
                if (!m_label)
                {
                        return;
                }
                CString labelText;
                labelText.Format(_T("%s (%d)"), getGroupName(), static_cast<int>(m_objects.size()));

                if (getOwner().isFilterActive())
                {
                        CString filterInfo;
                        filterInfo.Format(_T(" - %d/%d properties"), m_visiblePropertyCount, getPropertyCount());
                        labelText += filterInfo;

                        if (m_totalFavoriteCount > 0)
                        {
                                CString favoriteInfo;
                                favoriteInfo.Format(_T(" (%d favorite%s)"),
                                        m_visibleFavoriteCount,
                                        m_visibleFavoriteCount == 1 ? _T("") : _T("s"));
                                labelText += favoriteInfo;
                        }
                }
                else if (m_totalFavoriteCount > 0)
                {
                        CString favoriteInfo;
                        favoriteInfo.Format(_T(" (%d favorite%s)"),
                                m_totalFavoriteCount,
                                m_totalFavoriteCount == 1 ? _T("") : _T("s"));
                        labelText += favoriteInfo;
                }

                m_label->SetWindowText(labelText);
        }

	// ================================================================

	inline ObjectEditor         &Property::getEditor()        { return m_ownerList.getOwner().getEditor(); }
	inline const UIObjectVector &Property::getObjects() const { return m_ownerList.getObjects(); }
	inline UIBaseObject         *Property::getAnchor()        { return m_ownerList.getAnchor(); }

	// -------------------------------------------------------------------------------

	void Property::updateValue(const UIBaseObject &anchorSelection)
	{
		UILowerString name(m_descriptor.m_name); // TODO OPTIMIZE
		anchorSelection.GetPropertyNarrow(name, m_narrowValue);
		baseclass::refreshItemValue();
	}

        CString Property::getDisplayName() const
        {
                return m_ownerList.getOwner().formatDisplayName(*this);
        }

        bool Property::canCopyIdentifier() const
        {
                return true;
        }

        CString Property::getIdentifierForCopy() const
        {
                return CString(getIdentifier().c_str());
        }

        bool Property::supportsFavorites() const
        {
                return true;
        }

	bool Property::isFavorite() const
	{
		return m_ownerList.getOwner().isFavoriteProperty(*this);
	}

	void Property::toggleFavorite()
	{
		m_ownerList.getOwner().toggleFavorite(*this);
	}

	std::string Property::getIdentifier() const
	{
		std::string identifier;
		if (m_ownerList.getGroupName())
			identifier += m_ownerList.getGroupName();

		identifier += "::";

		if (m_descriptor.m_name)
			identifier += m_descriptor.m_name;

		return identifier;
	}

	// -------------------------------------------------------------------------------

	bool Property::setValue(const CString &i_newValue)
	{
		// Do not call base-class implementation.
		// Send a request to the editor class
		UILowerString name(m_descriptor.m_name);
		UINarrowString nValue(i_newValue);

		UIString value(UIUnicode::narrowToWide(nValue));

		return getEditor().setObjectProperty(getObjects(), name, value);
	}

	// -------------------------------------------------------------------------------

	void Property::onFollowLink()
	{
		UIBaseObject *o = getAnchor();
		if (o)
		{
			UIBaseObject *const linkedObject = o->GetObjectFromPath(m_narrowValue.c_str());

			if (linkedObject)
			{
				// NOTE: is is possible that this object will get delected inside
				// the selection call.
				getEditor().select(*linkedObject, ObjectEditor::SEL_ASSIGN);
			}
		}
	}

	// -------------------------------------------------------------------------------

}
using namespace PropertiesPageNamespace;

/////////////////////////////////////////////////////////////////////////////
// PropertiesPage property page
PropertiesPage::PropertiesPage(ObjectEditor &i_editor, UIPropertyCategories::Category i_category)
:	CPropertyPage(PropertiesPage::IDD),
	ObjectPropertiesEditor::PropertyCategory(i_editor, i_category),
	m_filterUpdateTimer(0),
	m_contentTop(0),
        m_filterControlsInitialized(false),
        m_isUpdatingFilterText(false)
{
	m_psp.dwFlags |= PSP_USETITLE;

	char tabText[256];
	getLabelText(tabText, sizeof(tabText));
	m_strCaption=tabText;
	m_psp.pszTitle = m_strCaption;
	//{{AFX_DATA_INIT(PropertiesPage)
	//}}AFX_DATA_INIT
}

PropertiesPage::~PropertiesPage()
{
}

/////////////////////////////////////////////////////////////////////////////

// ==========================================================================

ObjectPropertiesEditor::PropertyList *PropertiesPage::_newPropertyList(
	const UIPropertyGroup &propertyGroup, 
	const PropertyListMap::iterator &insertionPoint
)
{
	int topY=0;
	if (insertionPoint!=m_propertyLists.begin())
	{
		PropertyListMap::iterator prev = insertionPoint;
		--prev;
		PropertyList *pl = static_cast<PropertyList *>(prev->second);
		topY = pl->getBottomY();
	}

	// ----------------------------------------------------------------

	PropertyList *pl = new PropertyList(propertyGroup, *this, topY);
	std::pair<const PropertyListMap::key_type, PropertyList *> value(&propertyGroup, pl);
	m_propertyLists.insert(insertionPoint, value);

	// ----------------------------------------------------------------

	{
		int newY = pl->getBottomY();
		for (PropertyListMap::iterator shiftIter = insertionPoint; shiftIter!=m_propertyLists.end(); ++shiftIter)
		{
			PropertyList *shiftProp = static_cast<PropertyList *>(shiftIter->second);
			shiftProp->setTopY(newY);
			newY = shiftProp->getBottomY();
		}
	}

	return pl;
}

// ==========================================================================

void PropertiesPage::_freePropertyList(
	ObjectPropertiesEditor::PropertyList *i_pl,
	const PropertyListMap::iterator &listIter
)
{
	PropertyList *pl = static_cast<PropertyList *>(i_pl);

	int topY = pl->getTopY();

	delete pl;

	PropertyListMap::iterator shiftIter = listIter;
	++shiftIter;
	m_propertyLists.erase(listIter);
	while (shiftIter!=m_propertyLists.end())
	{
		PropertyList *const shiftProperty = static_cast<PropertyList *>(shiftIter->second);
		shiftProperty->setTopY(topY);
		++shiftIter;
	}
}

/////////////////////////////////////////////////////////////////////////////

void PropertiesPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(PropertiesPage)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(PropertiesPage, CPropertyPage)
        //{{AFX_MSG_MAP(PropertiesPage)
        ON_WM_SIZE()
        //}}AFX_MSG_MAP
        ON_WM_DESTROY()
        ON_WM_TIMER()
        ON_CBN_EDITCHANGE(IDC_PROPERTY_FILTER, OnFilterTextChanged)
        ON_CBN_SELENDOK(IDC_PROPERTY_FILTER, OnFilterHistorySelected)
        ON_BN_CLICKED(IDC_PROPERTY_FILTER_CLEAR, OnClearFilter)
        ON_BN_CLICKED(IDC_PROPERTY_FILTER_MATCH_VALUES, OnFilterOptionClicked)
        ON_BN_CLICKED(IDC_PROPERTY_FILTER_CASE, OnFilterOptionClicked)
        ON_BN_CLICKED(IDC_PROPERTY_FILTER_FAVORITES, OnFilterOptionClicked)
        ON_BN_CLICKED(IDC_PROPERTY_FILTER_HIDE_READ_ONLY, OnFilterOptionClicked)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// PropertiesPage message handlers

BOOL PropertiesPage::OnInitDialog()
{
        CPropertyPage::OnInitDialog();

        char tabText[256];
        getLabelText(tabText, sizeof(tabText));
        SetWindowText(tabText);

        m_filterCombo.SubclassDlgItem(IDC_PROPERTY_FILTER, this);
        m_filterClearButton.SubclassDlgItem(IDC_PROPERTY_FILTER_CLEAR, this);
        m_matchValuesCheck.SubclassDlgItem(IDC_PROPERTY_FILTER_MATCH_VALUES, this);
        m_caseSensitiveCheck.SubclassDlgItem(IDC_PROPERTY_FILTER_CASE, this);
        m_favoritesOnlyCheck.SubclassDlgItem(IDC_PROPERTY_FILTER_FAVORITES, this);
        m_hideReadOnlyCheck.SubclassDlgItem(IDC_PROPERTY_FILTER_HIDE_READ_ONLY, this);
        m_filterSummary.SubclassDlgItem(IDC_PROPERTY_FILTER_SUMMARY, this);

        m_filterControlsInitialized = true;

        loadFilterPreferences();
        loadFilterHistory();
        refreshFilterHistoryCombo();

        setFilterText(m_pendingFilterText);
        CString initTrim(m_pendingFilterText);
        initTrim.Trim();
        m_filterClearButton.EnableWindow(!initTrim.IsEmpty());

        repositionPropertyLists();

        int topY = m_contentTop;
        for (PropertyListMap::iterator pli = m_propertyLists.begin(); pli != m_propertyLists.end(); ++pli)
        {
                PropertyList *pl = static_cast<PropertyList *>(pli->second);
                pl->createControls(topY);
                pl->onOwnerResized();
                topY = pl->getBottomY();
        }

        applyFilter();
        updateFilterSummary();

        return TRUE;
}

void PropertiesPage::OnSize(UINT nType, int cx, int cy)
{
        CPropertyPage::OnSize(nType, cx, cy);

        if (!m_filterControlsInitialized)
                return;

        repositionPropertyLists();
}

void PropertiesPage::OnDestroy()
{
        if (m_filterUpdateTimer != 0)
        {
                KillTimer(m_filterUpdateTimer);
                m_filterUpdateTimer = 0;
        }

        saveFilterPreferences();

        CPropertyPage::OnDestroy();
}

void PropertiesPage::OnTimer(UINT_PTR nIDEvent)
{
        if (nIDEvent == FilterUpdateTimerId)
        {
                if (m_filterUpdateTimer != 0)
                {
                        KillTimer(m_filterUpdateTimer);
                        m_filterUpdateTimer = 0;
                }

                applyFilter();
                saveFilterPreferences();
                return;
        }

        CPropertyPage::OnTimer(nIDEvent);
}

void PropertiesPage::OnFilterTextChanged()
{
        if (!m_filterControlsInitialized || m_isUpdatingFilterText)
                return;

        m_filterCombo.GetWindowText(m_pendingFilterText);
        scheduleFilterUpdate();
        CString trimmed(m_pendingFilterText);
        trimmed.Trim();
        m_filterClearButton.EnableWindow(!trimmed.IsEmpty());
}

void PropertiesPage::OnFilterHistorySelected()
{
        if (!m_filterControlsInitialized)
                return;

        const int selection = m_filterCombo.GetCurSel();
        if (selection == CB_ERR)
                return;

        CString choice;
        m_filterCombo.GetLBText(selection, choice);

        setFilterText(choice);
        
        CString trimmed(choice);
        trimmed.Trim();
        m_filterClearButton.EnableWindow(!trimmed.IsEmpty());

        if (m_filterUpdateTimer != 0)
        {
                KillTimer(m_filterUpdateTimer);
                m_filterUpdateTimer = 0;
        }

        applyFilter();
        saveFilterPreferences();
}

void PropertiesPage::OnClearFilter()
{
        if (!m_filterControlsInitialized)
                return;

        setFilterText(_T(""));
        m_filterCombo.SetCurSel(-1);

        if (m_filterUpdateTimer != 0)
        {
                KillTimer(m_filterUpdateTimer);
                m_filterUpdateTimer = 0;
        }

        applyFilter();
        saveFilterPreferences();
}

void PropertiesPage::OnFilterOptionClicked()
{
        if (!m_filterControlsInitialized)
                return;

        applyFilter();
        saveFilterPreferences();
}

void PropertiesPage::scheduleFilterUpdate()
{
        if (!m_filterControlsInitialized)
                return;

        if (m_filterUpdateTimer != 0)
        {
                KillTimer(m_filterUpdateTimer);
                m_filterUpdateTimer = 0;
        }

        m_filterUpdateTimer = SetTimer(FilterUpdateTimerId, 200, 0);
}

void PropertiesPage::applyFilter()
{
        if (!m_filterControlsInitialized)
                return;

        ensureFavoritesLoaded();

        m_filterCombo.GetWindowText(m_pendingFilterText);

        CString filterText(m_pendingFilterText);
        CString trimmed(filterText);
        trimmed.Trim();

        const bool matchValues = (m_matchValuesCheck.GetCheck() == BST_CHECKED);
        const bool caseSensitive = (m_caseSensitiveCheck.GetCheck() == BST_CHECKED);
        const bool favoritesOnly = (m_favoritesOnlyCheck.GetCheck() == BST_CHECKED);
        const bool hideReadOnly = (m_hideReadOnlyCheck.GetCheck() == BST_CHECKED);

        m_filterClearButton.EnableWindow(!trimmed.IsEmpty());

        for (PropertyListMap::iterator iter = m_propertyLists.begin(); iter != m_propertyLists.end(); ++iter)
        {
                PropertyList *pl = static_cast<PropertyList *>(iter->second);
                pl->applyFilter(filterText, matchValues, caseSensitive, favoritesOnly, hideReadOnly);
        }

        if (!trimmed.IsEmpty())
                updateFilterHistory(trimmed);

        updateFilterSummary();
}

void PropertiesPage::saveFilterPreferences() const
{
        CWinApp *const app = AfxGetApp();
        if (!app)
                return;

        app->WriteProfileInt(_T("PropertyBrowser"), _T("MatchValues"), m_matchValuesCheck.GetCheck() == BST_CHECKED);
        app->WriteProfileInt(_T("PropertyBrowser"), _T("CaseSensitive"), m_caseSensitiveCheck.GetCheck() == BST_CHECKED);
        app->WriteProfileInt(_T("PropertyBrowser"), _T("FavoritesOnly"), m_favoritesOnlyCheck.GetCheck() == BST_CHECKED);
        app->WriteProfileInt(_T("PropertyBrowser"), _T("HideReadOnly"), m_hideReadOnlyCheck.GetCheck() == BST_CHECKED);
        app->WriteProfileString(_T("PropertyBrowser"), _T("FilterText"), m_pendingFilterText);

        saveFilterHistory();
}

void PropertiesPage::loadFilterPreferences()
{
        CWinApp *const app = AfxGetApp();
        if (!app)
                return;

        ensureFavoritesLoaded();

        const BOOL matchValues = app->GetProfileInt(_T("PropertyBrowser"), _T("MatchValues"), FALSE);
        const BOOL caseSensitive = app->GetProfileInt(_T("PropertyBrowser"), _T("CaseSensitive"), FALSE);
        const BOOL favoritesOnly = app->GetProfileInt(_T("PropertyBrowser"), _T("FavoritesOnly"), FALSE);
        const BOOL hideReadOnly = app->GetProfileInt(_T("PropertyBrowser"), _T("HideReadOnly"), FALSE);
        m_pendingFilterText = app->GetProfileString(_T("PropertyBrowser"), _T("FilterText"), _T(""));

        m_matchValuesCheck.SetCheck(matchValues ? BST_CHECKED : BST_UNCHECKED);
        m_caseSensitiveCheck.SetCheck(caseSensitive ? BST_CHECKED : BST_UNCHECKED);
        m_favoritesOnlyCheck.SetCheck(favoritesOnly ? BST_CHECKED : BST_UNCHECKED);
        m_hideReadOnlyCheck.SetCheck(hideReadOnly ? BST_CHECKED : BST_UNCHECKED);
}

void PropertiesPage::saveFilterHistory() const
{
        CWinApp *const app = AfxGetApp();
        if (!app)
                return;

        std::ostringstream stream;
        bool first = true;
        for (std::vector<CString>::const_iterator it = m_filterHistory.begin(); it != m_filterHistory.end(); ++it)
        {
                CStringA entryAnsi(*it);
                if (entryAnsi.IsEmpty())
                        continue;

                if (!first)
                        stream << '\n';

                stream << entryAnsi.GetString();
                first = false;
        }

        const std::string serialized = stream.str();
        app->WriteProfileString(_T("PropertyBrowser"), _T("FilterHistory"), CString(serialized.c_str()));
}

void PropertiesPage::loadFilterHistory()
{
        m_filterHistory.clear();

        CWinApp *const app = AfxGetApp();
        if (!app)
                return;

        CString raw = app->GetProfileString(_T("PropertyBrowser"), _T("FilterHistory"), _T(""));
        CStringA rawAnsi(raw);
        std::istringstream stream(rawAnsi.GetString());
        std::string line;

        while (std::getline(stream, line))
        {
                if (line.empty())
                        continue;

                CString entry(line.c_str());
                entry.Trim();
                if (entry.IsEmpty())
                        continue;

                m_filterHistory.push_back(entry);
                if (m_filterHistory.size() >= MaxFilterHistory)
                        break;
        }
}

void PropertiesPage::updateFilterHistory(const CString &filterText)
{
        CString trimmed(filterText);
        trimmed.Trim();
        if (trimmed.IsEmpty())
                return;

        for (std::vector<CString>::iterator it = m_filterHistory.begin(); it != m_filterHistory.end(); ++it)
        {
                if (it->CompareNoCase(trimmed) == 0)
                {
                        if (it == m_filterHistory.begin())
                                return;

                        CString existing(*it);
                        m_filterHistory.erase(it);
                        m_filterHistory.insert(m_filterHistory.begin(), existing);
                        refreshFilterHistoryCombo();
                        return;
                }
        }

        if (m_filterHistory.size() >= MaxFilterHistory)
                m_filterHistory.pop_back();

        m_filterHistory.insert(m_filterHistory.begin(), trimmed);
        refreshFilterHistoryCombo();
}

void PropertiesPage::refreshFilterHistoryCombo()
{
        if (!m_filterCombo.GetSafeHwnd())
                return;

        CString currentText;
        m_filterCombo.GetWindowText(currentText);

        int selStart = 0;
        int selEnd = 0;
        if (m_filterCombo.GetEditSel(selStart, selEnd) == CB_ERR)
        {
                selStart = currentText.GetLength();
                selEnd = selStart;
        }

        const bool wasUpdating = m_isUpdatingFilterText;
        m_isUpdatingFilterText = true;

        m_filterCombo.ResetContent();
        for (std::vector<CString>::const_iterator it = m_filterHistory.begin(); it != m_filterHistory.end(); ++it)
                m_filterCombo.AddString(*it);

        m_filterCombo.SetWindowText(currentText);
        m_filterCombo.SetCurSel(-1);
        m_filterCombo.SetEditSel(selStart, selEnd);

        m_isUpdatingFilterText = wasUpdating;
}

void PropertiesPage::setFilterText(const CString &text)
{
        m_pendingFilterText = text;

        if (!m_filterCombo.GetSafeHwnd())
                return;

        const bool wasUpdating = m_isUpdatingFilterText;
        m_isUpdatingFilterText = true;

        m_filterCombo.SetWindowText(text);
        m_filterCombo.SetCurSel(-1);
        const int caret = text.GetLength();
        m_filterCombo.SetEditSel(caret, caret);

        m_isUpdatingFilterText = wasUpdating;
}

void PropertiesPage::updateFilterSummary()
{
        if (!m_filterControlsInitialized)
                return;

        int totalVisible = 0;
        int totalProperties = 0;
        int visibleFavorites = 0;
        int totalFavorites = 0;

        for (PropertyListMap::const_iterator iter = m_propertyLists.begin(); iter != m_propertyLists.end(); ++iter)
        {
                const PropertyList *pl = static_cast<PropertyList *>(iter->second);
                totalVisible += pl->getVisiblePropertyCount();
                totalProperties += pl->getDisplayablePropertyCount();
                visibleFavorites += pl->getVisibleFavoriteCount();
                totalFavorites += pl->getTotalFavoriteCount();
        }

        CString summary;
        CString trimmed(m_pendingFilterText);
        trimmed.Trim();
        const bool favoritesOnly = (m_filterControlsInitialized && m_favoritesOnlyCheck.GetCheck() == BST_CHECKED);
        const bool hideReadOnly = (m_filterControlsInitialized && m_hideReadOnlyCheck.GetCheck() == BST_CHECKED);

        if (!trimmed.IsEmpty() || favoritesOnly)
        {
                summary.Format(_T("Showing %d of %d properties"), totalVisible, totalProperties);

                if (visibleFavorites > 0 || favoritesOnly)
                {
                        CString favInfo;
                        favInfo.Format(_T(" (%d favorite%s)"),
                                visibleFavorites,
                                visibleFavorites == 1 ? _T("") : _T("s"));
                        summary += favInfo;
                }
        }
        else
        {
                summary.Format(_T("Showing all %d properties"), totalProperties);

                if (totalFavorites > 0)
                {
                        CString favInfo;
                        favInfo.Format(_T(" (%d favorite%s)"),
                                totalFavorites,
                                totalFavorites == 1 ? _T("") : _T("s"));
                        summary += favInfo;
                }
        }

        if (hideReadOnly)
        {
                if (!summary.IsEmpty())
                        summary += _T(" ");

                summary += _T("(hiding read-only properties)");
        }

        m_filterSummary.SetWindowText(summary);
}

void PropertiesPage::repositionPropertyLists()
{
        if (!m_filterControlsInitialized)
                return;

        CRect clientRect;
        GetClientRect(&clientRect);

        const int margin = PropertiesPageNamespace::PropertyList::HorizontalMargin;
        const int topMargin = 4;
        const int controlSpacing = 4;
        const int buttonWidth = 46;
        const int editHeight = 14;

        CRect comboRect(clientRect.left + margin, clientRect.top + topMargin,
                clientRect.right - margin - buttonWidth - controlSpacing, clientRect.top + topMargin + editHeight);
        if (comboRect.right < comboRect.left + 60)
                comboRect.right = comboRect.left + 60;

        if (m_filterCombo.GetSafeHwnd())
                m_filterCombo.MoveWindow(comboRect);

        if (m_filterClearButton.GetSafeHwnd())
        {
                CRect clearRect(comboRect.right + controlSpacing, comboRect.top,
                        clientRect.right - margin, comboRect.bottom);
                m_filterClearButton.MoveWindow(clearRect);
        }

        int checkboxTop = comboRect.bottom + controlSpacing;
        const int checkboxHeight = 12;
        const int checkboxWidth = 120;

        if (m_matchValuesCheck.GetSafeHwnd())
        {
                CRect rect(clientRect.left + margin, checkboxTop,
                        clientRect.left + margin + checkboxWidth, checkboxTop + checkboxHeight);
                m_matchValuesCheck.MoveWindow(rect);
        }

        if (m_caseSensitiveCheck.GetSafeHwnd())
        {
                CRect rect(clientRect.left + margin + checkboxWidth + controlSpacing, checkboxTop,
                        clientRect.left + margin + checkboxWidth * 2 + controlSpacing, checkboxTop + checkboxHeight);
                m_caseSensitiveCheck.MoveWindow(rect);
        }

        const int secondRowTop = checkboxTop + checkboxHeight + 2;
        if (m_favoritesOnlyCheck.GetSafeHwnd())
        {
                CRect rect(clientRect.left + margin, secondRowTop,
                        clientRect.left + margin + checkboxWidth + 20, secondRowTop + checkboxHeight);
                m_favoritesOnlyCheck.MoveWindow(rect);
        }

        if (m_hideReadOnlyCheck.GetSafeHwnd())
        {
                CRect rect(clientRect.left + margin + checkboxWidth + controlSpacing, secondRowTop,
                        clientRect.left + margin + checkboxWidth * 2 + controlSpacing + 20, secondRowTop + checkboxHeight);
                m_hideReadOnlyCheck.MoveWindow(rect);
        }

        int summaryTop = secondRowTop + checkboxHeight + 4;
        if (m_filterSummary.GetSafeHwnd())
        {
                CRect summaryRect(clientRect.left + margin, summaryTop,
                        clientRect.right - margin, summaryTop + 12);
                m_filterSummary.MoveWindow(summaryRect);
                summaryTop = summaryRect.bottom;
        }

        m_contentTop = summaryTop + controlSpacing;

        int topY = m_contentTop;
        for (PropertyListMap::iterator iter = m_propertyLists.begin(); iter != m_propertyLists.end(); ++iter)
        {
                PropertyList *pl = static_cast<PropertyList *>(iter->second);
                pl->setTopY(topY);
                pl->onOwnerResized();
                topY = pl->getBottomY();
        }
}

bool PropertiesPage::isFavoriteProperty(const PropertiesPageNamespace::Property &property) const
{
        ensureFavoritesLoaded();
        return favoriteStorage().find(property.getIdentifier()) != favoriteStorage().end();
}

void PropertiesPage::toggleFavorite(PropertiesPageNamespace::Property &property)
{
        ensureFavoritesLoaded();

        std::set<std::string> &favorites = favoriteStorage();
        const std::string key = property.getIdentifier();

        if (favorites.count(key))
                favorites.erase(key);
        else
                favorites.insert(key);

        persistFavorites();
        onFavoritesChanged();
}

CString PropertiesPage::formatDisplayName(const PropertiesPageNamespace::Property &property) const
{
        CString name(property.m_descriptor.m_name ? property.m_descriptor.m_name : "");

        if (isFavoriteProperty(property))
        {
#ifdef UNICODE
                const wchar_t prefixChars[] = { 0x2605, L' ', 0 };
                CString prefix(prefixChars);
#else
                CString prefix(_T("* "));
#endif
                name = prefix + name;
        }

        return name;
}

void PropertiesPage::onFavoritesChanged()
{
        for (PropertyListMap::iterator iter = m_propertyLists.begin(); iter != m_propertyLists.end(); ++iter)
        {
                PropertyList *pl = static_cast<PropertyList *>(iter->second);
                pl->refreshDisplayNames();
        }

        applyFilter();
        updateFilterSummary();
}

bool PropertiesPage::isFilterActive() const
{
        CString trimmed(m_pendingFilterText);
        trimmed.Trim();
        const bool favoritesOnly = m_filterControlsInitialized && (m_favoritesOnlyCheck.GetCheck() == BST_CHECKED);
        const bool hideReadOnly = m_filterControlsInitialized && (m_hideReadOnlyCheck.GetCheck() == BST_CHECKED);
        return !trimmed.IsEmpty() || favoritesOnly || hideReadOnly;
}

void PropertiesPage::onPropertyContentChanged(PropertiesPageNamespace::PropertyList &list)
{
        if (!m_filterControlsInitialized)
                return;

        if (isFilterActive())
        {
                const bool matchValues = (m_matchValuesCheck.GetCheck() == BST_CHECKED);
                const bool caseSensitive = (m_caseSensitiveCheck.GetCheck() == BST_CHECKED);
                const bool favoritesOnly = (m_favoritesOnlyCheck.GetCheck() == BST_CHECKED);
                const bool hideReadOnly = (m_hideReadOnlyCheck.GetCheck() == BST_CHECKED);
                list.applyFilter(m_pendingFilterText, matchValues, caseSensitive, favoritesOnly, hideReadOnly);
        }
        else
        {
                list.refreshDisplayNames();
                list.onOwnerResized();
        }

        updateFilterSummary();
}
