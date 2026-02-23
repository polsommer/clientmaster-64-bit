#ifndef INCLUDED_DesignAssistant_H
#define INCLUDED_DesignAssistant_H

#include "EditorMonitor.h"
#include "UITypes.h"

#include <string>
#include <vector>

class ObjectEditor;
class UIBaseObject;
class UIDirect3DPrimaryCanvas;
class UIPage;
class UIWidget;

//---------------------------------------------------------------------------
// The DesignAssistant observes the editor state and provides contextual
// guidance along with lightweight visual overlays that help designers build
// professional looking user interfaces faster.
//---------------------------------------------------------------------------
class DesignAssistant : public EditorMonitor
{
public:
	DesignAssistant();

	void install(ObjectEditor &editor);
	void remove(ObjectEditor &editor);

	void update(const ObjectEditor &editor, const UIPage &rootPage);
        void render(UIDirect3DPrimaryCanvas &canvas) const;

        const std::string &getStatusText() const;
        const std::vector<std::string> &getDetails() const;
        void reset();

        bool hasAutoLayoutRecommendation() const;
        int getRecommendedLayoutColumns() const;
        int getRecommendedLayoutRows() const;
        float getLayoutDensityScore() const;
        bool hasLayoutDensityWarning() const;

        // EditorMonitor
        virtual void onEditReset();
        virtual void onEditInsertSubtree(UIBaseObject &subTree, UIBaseObject *previousSibling);
	virtual void onEditRemoveSubtree(UIBaseObject &subTree);
	virtual void onEditMoveSubtree(UIBaseObject &subTree, UIBaseObject *previousSibling, UIBaseObject *oldParent);
	virtual void onEditSetObjectProperty(UIBaseObject &object, const char *i_propertyName);
	virtual void onSelect(UIBaseObject &object, bool isSelected);

private:
	struct VisualGuideLine
	{
	        UIPoint start;
	        UIPoint end;
	        UIColor color;
	        int thickness;
	};

	struct VisualRegion
	{
	        UIRect rect;
	        UIColor color;
	};

	void markDirty();
        void rebuild(const ObjectEditor &editor, const UIPage &rootPage);
        void buildForSingleWidget(const ObjectEditor &editor, const UIPage &rootPage, const UIWidget &widget);
        void buildForMultipleWidgets(const ObjectEditor &editor, const UIPage &rootPage, const std::vector<const UIWidget *> &widgets);
        void buildHdInsightsForWidget(const UIPage &rootPage, const UIWidget &widget, const UIRect &widgetRect);
        void buildHdInsightsForGroup(const UIPage &rootPage, const std::vector<const UIWidget *> &widgets, const std::vector<UIRect> &rects, const UIRect &bounding);
        void collectContentInsights(const UIPage &rootPage);
        void appendContentInsights();
        void composeStatusText();

	ObjectEditor *m_editor;
	bool m_dirty;

	std::string m_headline;
	std::vector<std::string> m_details;
        std::string m_statusText;
        std::vector<VisualGuideLine> m_lines;
        std::vector<VisualRegion> m_regions;
        bool m_hasAutoLayoutPlan;
        int m_recommendedLayoutColumns;
        int m_recommendedLayoutRows;
        float m_layoutDensityScore;
        bool m_layoutDensityWarning;

        struct ContentScan
        {
                int widgetCount;
                int textCount;
                int imageCount;
                int unnamedCount;
                int emptyTextCount;
                int unstyledTextCount;
                int imagelessCount;
        };

        ContentScan m_contentScan;
};

#endif // INCLUDED_DesignAssistant_H
