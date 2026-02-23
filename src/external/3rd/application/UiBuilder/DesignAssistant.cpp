#include "FirstUiBuilder.h"
#include "DesignAssistant.h"

#include "ObjectEditor.h"
#include "UIBaseObject.h"
#include "UIDirect3DPrimaryCanvas.h"
#include "UIObjectSet.h"
#include "UIPage.h"
#include "UIImage.h"
#include "UIImageStyle.h"
#include "UIText.h"
#include "UITextStyle.h"
#include "UIWidget.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <sstream>

namespace
{
	inline long absoluteValue(long value)
	{
	        return (value >= 0) ? value : -value;
	}

	inline std::string makeSizeText(long width, long height)
	{
	        std::ostringstream stream;
	        stream << width << "x" << height << " px";
	        return stream.str();
	}

	inline std::string formatRange(long minimum, long maximum, const char *suffix)
	{
	        std::ostringstream stream;
	        stream << minimum << "-" << maximum << ' ' << suffix;
	        return stream.str();
	}

        inline void addDetail(std::vector<std::string> &details, const std::string &value, std::size_t limit = 8)
        {
                if (!value.empty() && details.size() < limit)
                {
                        details.push_back(value);
                }
        }

        inline UIColor makeColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
        {
                return UIColor(r, g, b, a);
        }

        inline float computeHdScale(long dimension, long baseline)
        {
                if (baseline <= 0)
                {
                        return 1.0f;
                }

                const float scale = static_cast<float>(dimension) / static_cast<float>(baseline);
                return (scale < 1.0f) ? 1.0f : scale;
        }

        inline long computeHdTarget(long baseline, float scale)
        {
                const float multiplier = (scale < 1.0f) ? 1.0f : scale;
                const float scaled = static_cast<float>(baseline) * multiplier;
                return static_cast<long>(scaled + 0.5f);
        }

        inline std::string formatPercent(float value)
        {
                std::ostringstream stream;
                const float clamped = value < 0.0f ? 0.0f : value;
                stream << static_cast<int>(clamped * 100.0f + 0.5f) << '%';
                return stream.str();
        }
}

//-----------------------------------------------------------------

DesignAssistant::DesignAssistant() :
        m_editor(0),
        m_dirty(true),
        m_headline("Assistant: Ready"),
        m_statusText(m_headline),
        m_hasAutoLayoutPlan(false),
        m_recommendedLayoutColumns(0),
        m_recommendedLayoutRows(0),
        m_layoutDensityScore(0.0f),
        m_layoutDensityWarning(false)
{
        reset();
}

//-----------------------------------------------------------------

void DesignAssistant::install(ObjectEditor &editor)
{
	if (m_editor == &editor)
	{
	        return;
	}

	if (m_editor)
	{
	        remove(*m_editor);
	}

	m_editor = &editor;
	m_editor->addMonitor(*this);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::remove(ObjectEditor &editor)
{
	if (m_editor != &editor)
	{
	        return;
	}

	m_editor->removeMonitor(*this);
	m_editor = 0;
	reset();
}

//-----------------------------------------------------------------

void DesignAssistant::update(const ObjectEditor &editor, const UIPage &rootPage)
{
	rebuild(editor, rootPage);
}

//-----------------------------------------------------------------

void DesignAssistant::render(UIDirect3DPrimaryCanvas &canvas) const
{
	for (std::vector<VisualRegion>::const_iterator regionIt = m_regions.begin(); regionIt != m_regions.end(); ++regionIt)
	{
	        if (regionIt->color.a == 0)
	        {
	                continue;
	        }

	        const float opacity = static_cast<float>(regionIt->color.a) / 255.0f;
	        canvas.SetOpacity(opacity);
	        canvas.ClearTo(regionIt->color, regionIt->rect);
	}

	for (std::vector<VisualGuideLine>::const_iterator lineIt = m_lines.begin(); lineIt != m_lines.end(); ++lineIt)
	{
	        if (lineIt->color.a == 0)
	        {
	                continue;
	        }

	        const float opacity = static_cast<float>(lineIt->color.a) / 255.0f;
	        canvas.SetOpacity(opacity);

	        const UIScalar minX = std::min(lineIt->start.x, lineIt->end.x);
	        const UIScalar maxX = std::max(lineIt->start.x, lineIt->end.x);
	        const UIScalar minY = std::min(lineIt->start.y, lineIt->end.y);
	        const UIScalar maxY = std::max(lineIt->start.y, lineIt->end.y);
	        const int thickness = std::max(1, lineIt->thickness);

	        if (lineIt->start.x == lineIt->end.x)
	        {
	                const UIScalar half = thickness / 2;
	                const UIScalar remainder = thickness % 2;
	                const UIRect rect(minX - half, minY, minX + half + remainder, maxY);
	                canvas.ClearTo(lineIt->color, rect);
	        }
	        else if (lineIt->start.y == lineIt->end.y)
	        {
	                const UIScalar half = thickness / 2;
	                const UIScalar remainder = thickness % 2;
	                const UIRect rect(minX, minY - half, maxX, minY + half + remainder);
	                canvas.ClearTo(lineIt->color, rect);
	        }
	        else
	        {
	                const UIRect rect(minX, minY, maxX + thickness, maxY + thickness);
	                canvas.ClearTo(lineIt->color, rect);
	        }
	}

	canvas.SetOpacity(1.0f);
}

//-----------------------------------------------------------------

const std::string &DesignAssistant::getStatusText() const
{
        return m_statusText;
}

//-----------------------------------------------------------------

const std::vector<std::string> &DesignAssistant::getDetails() const
{
        return m_details;
}

//-----------------------------------------------------------------

void DesignAssistant::reset()
{
        m_headline = "Assistant: Ready";
        m_details.clear();
        m_lines.clear();
        m_regions.clear();
        m_hasAutoLayoutPlan = false;
        m_recommendedLayoutColumns = 0;
        m_recommendedLayoutRows = 0;
        m_layoutDensityScore = 0.0f;
        m_layoutDensityWarning = false;
        m_statusText = m_headline;
        m_contentScan.widgetCount = 0;
        m_contentScan.textCount = 0;
        m_contentScan.imageCount = 0;
        m_contentScan.unnamedCount = 0;
        m_contentScan.emptyTextCount = 0;
        m_contentScan.unstyledTextCount = 0;
        m_contentScan.imagelessCount = 0;
        m_dirty = true;
}

//-----------------------------------------------------------------

void DesignAssistant::onEditReset()
{
	reset();
}

//-----------------------------------------------------------------

void DesignAssistant::onEditInsertSubtree(UIBaseObject &subTree, UIBaseObject *previousSibling)
{
	UNREF(subTree);
	UNREF(previousSibling);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::onEditRemoveSubtree(UIBaseObject &subTree)
{
	UNREF(subTree);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::onEditMoveSubtree(UIBaseObject &subTree, UIBaseObject *previousSibling, UIBaseObject *oldParent)
{
	UNREF(subTree);
	UNREF(previousSibling);
	UNREF(oldParent);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::onEditSetObjectProperty(UIBaseObject &object, const char *i_propertyName)
{
	UNREF(object);
	UNREF(i_propertyName);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::onSelect(UIBaseObject &object, bool isSelected)
{
	UNREF(object);
	UNREF(isSelected);
	markDirty();
}

//-----------------------------------------------------------------

void DesignAssistant::markDirty()
{
	m_dirty = true;
}

//-----------------------------------------------------------------

void DesignAssistant::rebuild(const ObjectEditor &editor, const UIPage &rootPage)
{
	if (!m_dirty && m_editor == 0)
	{
	        return;
	}

        m_dirty = false;
        m_details.clear();
        m_lines.clear();
        m_regions.clear();
        m_hasAutoLayoutPlan = false;
        m_recommendedLayoutColumns = 0;
        m_recommendedLayoutRows = 0;
        m_layoutDensityScore = 0.0f;
        m_layoutDensityWarning = false;

        collectContentInsights(rootPage);

        const UIObjectSet &selections = editor.getSelections();
        std::vector<const UIWidget *> widgets;
	widgets.reserve(static_cast<std::size_t>(selections.size()));

	for (UIObjectSet::const_iterator it = selections.begin(); it != selections.end(); ++it)
	{
	        UIBaseObject *object = *it;
	        if (!object)
	        {
	                continue;
	        }

	        if (!object->IsA(TUIWidget))
	        {
	                continue;
	        }

	        const UIWidget *widget = UI_ASOBJECT(UIWidget, object);
	        widgets.push_back(widget);
	}

        if (widgets.empty())
        {
                m_headline = "Assistant: Ready";

                UIRect rootRect;
                rootPage.GetWorldRect(rootRect);

                std::ostringstream workspaceSize;
                workspaceSize << "Workspace " << rootRect.Width() << "x" << rootRect.Height() << " px";
                if (rootRect.Width() >= 3840 || rootRect.Height() >= 2160)
                {
                        workspaceSize << " (4K)";
                        addDetail(m_details, workspaceSize.str());
                        addDetail(m_details, "HD Tip: Scale hero art ≥ 128 px to stay sharp on UHD displays.");
                }
                else if (rootRect.Width() >= 1920 || rootRect.Height() >= 1080)
                {
                        workspaceSize << " (HD)";
                        addDetail(m_details, workspaceSize.str());
                        addDetail(m_details, "HD Tip: Aim for 64 px icons to maintain crispness.");
                }
                else
                {
                        addDetail(m_details, workspaceSize.str());
                }

                const UIPoint rootCenter = rootRect.GetCenter();
                const UIColor guideColor = makeColor(80, 200, 255, 48);

                m_lines.push_back(VisualGuideLine());
	        m_lines.back().start = UIPoint(rootCenter.x, rootRect.top);
	        m_lines.back().end = UIPoint(rootCenter.x, rootRect.bottom);
	        m_lines.back().color = guideColor;
	        m_lines.back().thickness = 1;

	        m_lines.push_back(VisualGuideLine());
                m_lines.back().start = UIPoint(rootRect.left, rootCenter.y);
                m_lines.back().end = UIPoint(rootRect.right, rootCenter.y);
                m_lines.back().color = guideColor;
                m_lines.back().thickness = 1;

                addDetail(m_details, "Select a widget to see contextual layout guidance.");
                appendContentInsights();
                composeStatusText();
                return;
        }

	if (widgets.size() == 1)
	{
	        buildForSingleWidget(editor, rootPage, *widgets.front());
	}
        else
        {
                buildForMultipleWidgets(editor, rootPage, widgets);
        }

        appendContentInsights();
        composeStatusText();
}

//-----------------------------------------------------------------

void DesignAssistant::collectContentInsights(const UIPage &rootPage)
{
        m_contentScan.widgetCount = 0;
        m_contentScan.textCount = 0;
        m_contentScan.imageCount = 0;
        m_contentScan.unnamedCount = 0;
        m_contentScan.emptyTextCount = 0;
        m_contentScan.unstyledTextCount = 0;
        m_contentScan.imagelessCount = 0;

        std::vector<UIBaseObject *> stack;
        stack.reserve(64);
        stack.push_back(const_cast<UIPage *>(&rootPage));

        while (!stack.empty())
        {
                UIBaseObject *current = stack.back();
                stack.pop_back();

                if (current->IsA(TUIWidget))
                {
                        ++m_contentScan.widgetCount;
                        const UINarrowString &name = current->GetName();
                        if (name.empty())
                        {
                                ++m_contentScan.unnamedCount;
                        }
                }

                if (current->IsA(TUIText))
                {
                        ++m_contentScan.textCount;
                        const UIText *text = UI_ASOBJECT(UIText, current);
                        if (text)
                        {
                                if (text->IsEmpty())
                                {
                                        ++m_contentScan.emptyTextCount;
                                }

                                if (text->GetTextStyle() == 0)
                                {
                                        ++m_contentScan.unstyledTextCount;
                                }
                        }
                }

                if (current->IsA(TUIImage))
                {
                        ++m_contentScan.imageCount;
                        const UIImage *image = UI_ASOBJECT(UIImage, current);
                        if (image && image->GetImageStyle() == 0)
                        {
                                ++m_contentScan.imagelessCount;
                        }
                }

                UIBaseObject::UIObjectList children;
                current->GetChildren(children);

                for (UIBaseObject::UIObjectList::iterator childIt = children.begin(); childIt != children.end(); ++childIt)
                {
                        if (*childIt)
                        {
                                stack.push_back(*childIt);
                        }
                }
        }
}

//-----------------------------------------------------------------

void DesignAssistant::appendContentInsights()
{
        if (m_contentScan.widgetCount > 0)
        {
                std::ostringstream summary;
                summary << "Content scan: "
                        << m_contentScan.widgetCount << " widgets, "
                        << m_contentScan.textCount << " text, "
                        << m_contentScan.imageCount << " images.";
                addDetail(m_details, summary.str());
        }

        if (m_contentScan.unnamedCount > 0)
        {
                std::ostringstream stream;
                stream << m_contentScan.unnamedCount << " unnamed widget"
                        << (m_contentScan.unnamedCount == 1 ? "" : "s")
                        << " — name controls to improve script hooks.";
                addDetail(m_details, stream.str());
        }

        if (m_contentScan.emptyTextCount > 0)
        {
                std::ostringstream stream;
                stream << m_contentScan.emptyTextCount << " empty text field"
                        << (m_contentScan.emptyTextCount == 1 ? "" : "s")
                        << " — add copy or hide placeholders.";
                addDetail(m_details, stream.str());
        }

        if (m_contentScan.unstyledTextCount > 0)
        {
                std::ostringstream stream;
                stream << m_contentScan.unstyledTextCount << " text widget"
                        << (m_contentScan.unstyledTextCount == 1 ? " missing style" : "s missing styles");
                addDetail(m_details, stream.str());
        }

        if (m_contentScan.imagelessCount > 0)
        {
                std::ostringstream stream;
                stream << m_contentScan.imagelessCount << " image widget"
                        << (m_contentScan.imagelessCount == 1 ? " missing art" : "s missing art")
                        << " — assign UIImageStyles or canvases.";
                addDetail(m_details, stream.str());
        }
}

//-----------------------------------------------------------------

void DesignAssistant::buildForSingleWidget(const ObjectEditor &editor, const UIPage &rootPage, const UIWidget &widget)
{
	UIRect widgetRect;
	widget.GetWorldRect(widgetRect);

	m_regions.push_back(VisualRegion());
	m_regions.back().rect = widgetRect;
	m_regions.back().color = makeColor(64, 160, 255, 32);

	const UIBaseObject *parentObject = widget.GetParent();
	const UIWidget *parentWidget = 0;
	if (parentObject && parentObject->IsA(TUIWidget))
	{
	        parentWidget = UI_ASOBJECT(UIWidget, parentObject);
	}
	if (!parentWidget)
	{
	        parentWidget = &rootPage;
	}

	UIRect parentRect;
	parentWidget->GetWorldRect(parentRect);

	const UIPoint widgetCenter = widgetRect.GetCenter();
	const UIPoint parentCenter = parentRect.GetCenter();

	const UIColor guideColor = makeColor(90, 210, 255, 72);
	m_lines.push_back(VisualGuideLine());
	m_lines.back().start = UIPoint(parentCenter.x, parentRect.top);
	m_lines.back().end = UIPoint(parentCenter.x, parentRect.bottom);
	m_lines.back().color = guideColor;
	m_lines.back().thickness = 1;

	m_lines.push_back(VisualGuideLine());
	m_lines.back().start = UIPoint(parentRect.left, parentCenter.y);
	m_lines.back().end = UIPoint(parentRect.right, parentCenter.y);
	m_lines.back().color = guideColor;
	m_lines.back().thickness = 1;

	const UINarrowString &name = widget.GetName();
	if (name.empty())
	{
	        m_headline = "Assistant: Unnamed widget";
	}
	else
	{
	        m_headline = "Assistant: ";
	        m_headline += name;
	}

        addDetail(m_details, makeSizeText(widgetRect.Width(), widgetRect.Height()));

        buildHdInsightsForWidget(rootPage, widget, widgetRect);

        const long deltaX = widgetCenter.x - parentCenter.x;
        const long deltaY = widgetCenter.y - parentCenter.y;

	{
	        std::ostringstream stream;
	        if (absoluteValue(deltaX) <= 1)
	        {
	                stream << "Centered horizontally";
	        }
	        else
	        {
	                stream << absoluteValue(deltaX) << " px " << ((deltaX > 0) ? "right" : "left") << " of center";
	        }
	        addDetail(m_details, stream.str());
	}

	{
	        std::ostringstream stream;
	        if (absoluteValue(deltaY) <= 1)
	        {
	                stream << "Centered vertically";
	        }
	        else
	        {
	                stream << absoluteValue(deltaY) << " px " << ((deltaY > 0) ? "below" : "above") << " center";
	        }
	        addDetail(m_details, stream.str());
	}

	const long leftMargin = widgetRect.left - parentRect.left;
	const long rightMargin = parentRect.right - widgetRect.right;
	const long topMargin = widgetRect.top - parentRect.top;
	const long bottomMargin = parentRect.bottom - widgetRect.bottom;

	{
	        std::ostringstream stream;
	        if (absoluteValue(leftMargin - rightMargin) <= 1)
	        {
	                stream << "Balanced horizontal padding " << ((leftMargin + rightMargin) / 2) << " px";
	        }
	        else
	        {
	                stream << "Padding L" << leftMargin << " / R" << rightMargin << " px";
	        }
	        addDetail(m_details, stream.str());
	}

        {
                std::ostringstream stream;
                if (absoluteValue(topMargin - bottomMargin) <= 1)
                {
                        stream << "Balanced vertical padding " << ((topMargin + bottomMargin) / 2) << " px";
                }
                else
                {
                        stream << "Padding T" << topMargin << " / B" << bottomMargin << " px";
                }
                addDetail(m_details, stream.str());
        }

        if (name.empty())
        {
                addDetail(m_details, "Name this widget to improve script hooks.");
        }

        {
                const double parentArea = static_cast<double>(std::max<long>(1, parentRect.Width())) * static_cast<double>(std::max<long>(1, parentRect.Height()));
                const double widgetArea = static_cast<double>(std::max<long>(1, widgetRect.Width())) * static_cast<double>(std::max<long>(1, widgetRect.Height()));

                if (parentArea > 0.0 && widgetArea > 0.0)
                {
                        const double density = widgetArea / parentArea;
                        m_layoutDensityScore = static_cast<float>(std::min(1.0, density));
                        m_layoutDensityWarning = (m_layoutDensityScore >= 0.85f);

                        std::ostringstream densityMessage;
                        densityMessage << "Density " << formatPercent(m_layoutDensityScore);

                        if (m_layoutDensityWarning)
                        {
                                densityMessage << " - add breathing room.";
                        }
                        else if (m_layoutDensityScore <= 0.25f)
                        {
                                densityMessage << " - consider enlarging or grouping.";
                        }
                        else
                        {
                                densityMessage << " - balanced.";
                        }

                        addDetail(m_details, densityMessage.str());
                }
        }

        const UIPoint &gridSteps = editor.getGridSteps();
        if (editor.getSnapToGrid() && gridSteps.x > 0 && gridSteps.y > 0)
        {
                const long modX = widgetRect.left % gridSteps.x;
                const long modY = widgetRect.top % gridSteps.y;

	        if (modX == 0 && modY == 0)
	        {
	                std::ostringstream stream;
	                stream << "Snapped to grid " << gridSteps.x << "x" << gridSteps.y;
	                addDetail(m_details, stream.str());
	        }
	        else
	        {
	                std::ostringstream stream;
	                stream << "Off grid by " << absoluteValue(modX) << "x" << absoluteValue(modY) << " px";
	                addDetail(m_details, stream.str());
	        }
        }
        else
        {
                addDetail(m_details, "Snap to grid is off (press G).");
        }

        const UIColor marginGuideColor = makeColor(180, 220, 255, 64);
        if (leftMargin > 0)
        {
                VisualGuideLine marginLine;
                marginLine.start = UIPoint(parentRect.left, widgetRect.top);
                marginLine.end = UIPoint(widgetRect.left, widgetRect.top);
                marginLine.color = marginGuideColor;
                marginLine.thickness = 1;
                m_lines.push_back(marginLine);
        }

        if (rightMargin > 0)
        {
                VisualGuideLine marginLine;
                marginLine.start = UIPoint(widgetRect.right, widgetRect.top);
                marginLine.end = UIPoint(parentRect.right, widgetRect.top);
                marginLine.color = marginGuideColor;
                marginLine.thickness = 1;
                m_lines.push_back(marginLine);
        }

        if (topMargin > 0)
        {
                VisualGuideLine marginLine;
                marginLine.start = UIPoint(widgetRect.left, parentRect.top);
                marginLine.end = UIPoint(widgetRect.left, widgetRect.top);
                marginLine.color = marginGuideColor;
                marginLine.thickness = 1;
                m_lines.push_back(marginLine);
        }

        if (bottomMargin > 0)
        {
                VisualGuideLine marginLine;
                marginLine.start = UIPoint(widgetRect.left, widgetRect.bottom);
                marginLine.end = UIPoint(widgetRect.left, parentRect.bottom);
                marginLine.color = marginGuideColor;
                marginLine.thickness = 1;
                m_lines.push_back(marginLine);
        }
}

//-----------------------------------------------------------------

void DesignAssistant::buildForMultipleWidgets(const ObjectEditor &editor, const UIPage &rootPage, const std::vector<const UIWidget *> &widgets)
{
        UNREF(editor);

        std::vector<UIRect> rects;
        rects.reserve(widgets.size());
        for (std::vector<const UIWidget *>::const_iterator it = widgets.begin(); it != widgets.end(); ++it)
        {
                UIRect rect;
                (*it)->GetWorldRect(rect);
                rects.push_back(rect);
        }

        UIRect bounding = rects.front();
        for (std::size_t i = 1; i < rects.size(); ++i)
        {
                bounding.Extend(rects[i]);
        }

	m_regions.push_back(VisualRegion());
	m_regions.back().rect = bounding;
        m_regions.back().color = makeColor(120, 255, 200, 28);

        const UIColor frameColor = makeColor(120, 255, 200, 80);
        m_lines.push_back(VisualGuideLine());
        m_lines.back().start = UIPoint(bounding.left, bounding.top);
	m_lines.back().end = UIPoint(bounding.right, bounding.top);
	m_lines.back().color = frameColor;
	m_lines.back().thickness = 1;

	m_lines.push_back(VisualGuideLine());
	m_lines.back().start = UIPoint(bounding.left, bounding.bottom);
	m_lines.back().end = UIPoint(bounding.right, bounding.bottom);
	m_lines.back().color = frameColor;
	m_lines.back().thickness = 1;

	m_lines.push_back(VisualGuideLine());
	m_lines.back().start = UIPoint(bounding.left, bounding.top);
	m_lines.back().end = UIPoint(bounding.left, bounding.bottom);
	m_lines.back().color = frameColor;
	m_lines.back().thickness = 1;

        m_lines.push_back(VisualGuideLine());
        m_lines.back().start = UIPoint(bounding.right, bounding.top);
        m_lines.back().end = UIPoint(bounding.right, bounding.bottom);
        m_lines.back().color = frameColor;
        m_lines.back().thickness = 1;

        buildHdInsightsForGroup(rootPage, widgets, rects, bounding);

        {
            std::ostringstream headline;
            headline << "Assistant: " << widgets.size() << " widgets selected";
            m_headline = headline.str();
        }

        addDetail(m_details, "Group " + makeSizeText(bounding.Width(), bounding.Height()));

	long minWidth = rects.front().Width();
	long maxWidth = rects.front().Width();
	long minHeight = rects.front().Height();
	long maxHeight = rects.front().Height();

        for (std::size_t i = 1; i < rects.size(); ++i)
        {
                minWidth = std::min(minWidth, rects[i].Width());
                maxWidth = std::max(maxWidth, rects[i].Width());
                minHeight = std::min(minHeight, rects[i].Height());
                maxHeight = std::max(maxHeight, rects[i].Height());
        }

        {
                double totalArea = 0.0;
                for (std::size_t i = 0; i < rects.size(); ++i)
                {
                        totalArea += static_cast<double>(std::max<long>(1, rects[i].Width())) * static_cast<double>(std::max<long>(1, rects[i].Height()));
                }

                const double boundingArea = static_cast<double>(std::max<long>(1, bounding.Width())) * static_cast<double>(std::max<long>(1, bounding.Height()));

                if (boundingArea > 0.0 && totalArea > 0.0)
                {
                        m_layoutDensityScore = static_cast<float>(std::min(1.0, totalArea / boundingArea));
                        m_layoutDensityWarning = (m_layoutDensityScore >= 0.90f);

                        std::ostringstream densityMessage;
                        densityMessage << "Density " << formatPercent(m_layoutDensityScore);

                        if (m_layoutDensityWarning)
                        {
                                densityMessage << " - reduce overlap risk.";
                        }
                        else if (m_layoutDensityScore <= 0.35f)
                        {
                                densityMessage << " - tighten grouping for cohesion.";
                        }
                        else
                        {
                                densityMessage << " - balanced grouping.";
                        }

                        addDetail(m_details, densityMessage.str());
                }
        }

        if (minWidth == maxWidth)
        {
                addDetail(m_details, "Uniform widths");
        }
        else
	{
	        addDetail(m_details, std::string("Widths ") + formatRange(minWidth, maxWidth, "px"));
	}

	if (minHeight == maxHeight)
	{
	        addDetail(m_details, "Uniform heights");
	}
	else
	{
	        addDetail(m_details, std::string("Heights ") + formatRange(minHeight, maxHeight, "px"));
	}

	const UIPoint firstCenter = rects.front().GetCenter();
	bool shareCenterX = true;
	bool shareCenterY = true;
	bool shareTop = true;
	bool shareBottom = true;

	for (std::size_t i = 1; i < rects.size(); ++i)
	{
	        const UIPoint center = rects[i].GetCenter();
	        if (absoluteValue(center.x - firstCenter.x) > 1)
	        {
	                shareCenterX = false;
	        }
	        if (absoluteValue(center.y - firstCenter.y) > 1)
	        {
	                shareCenterY = false;
	        }
	        if (absoluteValue(rects[i].top - rects.front().top) > 1)
	        {
	                shareTop = false;
	        }
	        if (absoluteValue(rects[i].bottom - rects.front().bottom) > 1)
	        {
	                shareBottom = false;
	        }
	}

	if (shareTop)
	{
	        addDetail(m_details, "Top edges aligned");
	}
	else if (shareBottom)
	{
	        addDetail(m_details, "Bottom edges aligned");
	}

	if (shareCenterX)
	{
	        addDetail(m_details, "Vertically centered as a group");
	}
	else if (shareCenterY)
	{
	        addDetail(m_details, "Horizontally centered as a group");
	}

	if (widgets.size() >= 3)
	{
	        std::vector<UIRect> horizontalOrder(rects);
	        std::sort(horizontalOrder.begin(), horizontalOrder.end(), [](const UIRect &lhs, const UIRect &rhs)
	        {
	                if (lhs.left == rhs.left)
	                {
	                        return lhs.top < rhs.top;
	                }
	                return lhs.left < rhs.left;
	        });

	        std::vector<long> gaps;
	        for (std::size_t i = 1; i < horizontalOrder.size(); ++i)
	        {
	                gaps.push_back(horizontalOrder[i].left - horizontalOrder[i - 1].right);
	        }
	        if (!gaps.empty())
	        {
	                const long averageGap = std::accumulate(gaps.begin(), gaps.end(), 0L) / static_cast<long>(gaps.size());
	                long maxDeviation = 0;
	                for (std::size_t i = 0; i < gaps.size(); ++i)
	                {
	                        maxDeviation = std::max(maxDeviation, absoluteValue(gaps[i] - averageGap));
	                }
	                if (maxDeviation <= 2)
	                {
	                        std::ostringstream stream;
	                        stream << "Even horizontal spacing (" << averageGap << " px)";
	                        addDetail(m_details, stream.str());
	                }
	                else
	                {
	                        std::ostringstream stream;
	                        stream << "Horizontal gaps vary ±" << maxDeviation << " px";
	                        addDetail(m_details, stream.str());
	                }
	        }

	        std::vector<UIRect> verticalOrder(rects);
	        std::sort(verticalOrder.begin(), verticalOrder.end(), [](const UIRect &lhs, const UIRect &rhs)
	        {
	                if (lhs.top == rhs.top)
	                {
	                        return lhs.left < rhs.left;
	                }
	                return lhs.top < rhs.top;
	        });

	        gaps.clear();
	        for (std::size_t i = 1; i < verticalOrder.size(); ++i)
	        {
	                gaps.push_back(verticalOrder[i].top - verticalOrder[i - 1].bottom);
	        }
	        if (!gaps.empty())
	        {
	                const long averageGap = std::accumulate(gaps.begin(), gaps.end(), 0L) / static_cast<long>(gaps.size());
	                long maxDeviation = 0;
	                for (std::size_t i = 0; i < gaps.size(); ++i)
	                {
	                        maxDeviation = std::max(maxDeviation, absoluteValue(gaps[i] - averageGap));
	                }
	                if (maxDeviation <= 2)
	                {
	                        std::ostringstream stream;
	                        stream << "Even vertical spacing (" << averageGap << " px)";
	                        addDetail(m_details, stream.str());
	                }
	                else
	                {
	                        std::ostringstream stream;
	                        stream << "Vertical gaps vary ±" << maxDeviation << " px";
	                        addDetail(m_details, stream.str());
	                }
	        }
	}

        std::set<const UIBaseObject *> parents;
        for (std::size_t i = 0; i < widgets.size(); ++i)
        {
                parents.insert(widgets[i]->GetParent());
        }

        if (parents.size() == 1)
        {
                const int widgetCount = static_cast<int>(widgets.size());
                int recommendedColumns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(widgetCount))));
                recommendedColumns = std::max(1, std::min(widgetCount, recommendedColumns));

                if (bounding.Width() > 0 && bounding.Height() > 0)
                {
                        const float aspect = static_cast<float>(bounding.Width()) / static_cast<float>(std::max(1L, bounding.Height()));
                        float bestScore = 1000000.0f;
                        int bestColumns = recommendedColumns;

                        for (int candidate = 1; candidate <= widgetCount; ++candidate)
                        {
                                const int candidateRows = (widgetCount + candidate - 1) / candidate;
                                const float gridAspect = static_cast<float>(candidate) / static_cast<float>(std::max(1, candidateRows));
                                const float score = std::fabs(gridAspect - aspect);

                                if (score < bestScore)
                                {
                                        bestScore = score;
                                        bestColumns = candidate;
                                }
                        }

                        recommendedColumns = bestColumns;
                }

                const int recommendedRows = (widgetCount + recommendedColumns - 1) / recommendedColumns;
                m_hasAutoLayoutPlan = recommendedColumns > 0 && recommendedRows > 0;
                m_recommendedLayoutColumns = m_hasAutoLayoutPlan ? recommendedColumns : 0;
                m_recommendedLayoutRows = m_hasAutoLayoutPlan ? recommendedRows : 0;

                if (m_hasAutoLayoutPlan)
                {
                        std::ostringstream stream;
                        stream << "Suggested grid " << m_recommendedLayoutColumns << " x " << m_recommendedLayoutRows;
                        addDetail(m_details, stream.str());

                        const UIColor gridColor = makeColor(255, 180, 120, 72);
                        const long cellWidth = (m_recommendedLayoutColumns > 0) ? (bounding.Width() / m_recommendedLayoutColumns) : bounding.Width();
                        const long cellHeight = (m_recommendedLayoutRows > 0) ? (bounding.Height() / m_recommendedLayoutRows) : bounding.Height();

                        for (int column = 1; column < m_recommendedLayoutColumns; ++column)
                        {
                                const long x = bounding.left + column * cellWidth;
                                VisualGuideLine guide;
                                guide.start = UIPoint(x, bounding.top);
                                guide.end = UIPoint(x, bounding.bottom);
                                guide.color = gridColor;
                                guide.thickness = 1;
                                m_lines.push_back(guide);
                        }

                        for (int row = 1; row < m_recommendedLayoutRows; ++row)
                        {
                                const long y = bounding.top + row * cellHeight;
                                VisualGuideLine guide;
                                guide.start = UIPoint(bounding.left, y);
                                guide.end = UIPoint(bounding.right, y);
                                guide.color = gridColor;
                                guide.thickness = 1;
                                m_lines.push_back(guide);
                        }
                }
                else
                {
                        m_recommendedLayoutColumns = 0;
                        m_recommendedLayoutRows = 0;
                }

                const UIBaseObject *parent = *parents.begin();
                if (parent)
                {
                        const UINarrowString &name = parent->GetName();
                        if (!name.empty())
	                {
	                        addDetail(m_details, std::string("Parent: ") + name);
	                }
	                else
	                {
	                        addDetail(m_details, "Parent: (unnamed container)");
	                }
	        }
        }
        else if (parents.size() > 1)
        {
                m_hasAutoLayoutPlan = false;
                m_recommendedLayoutColumns = 0;
                m_recommendedLayoutRows = 0;
                std::ostringstream stream;
                stream << "Across " << parents.size() << " containers";
                addDetail(m_details, stream.str());
        }
        else
        {
                m_hasAutoLayoutPlan = false;
                m_recommendedLayoutColumns = 0;
                m_recommendedLayoutRows = 0;
        }
}

//-----------------------------------------------------------------

void DesignAssistant::buildHdInsightsForWidget(const UIPage &rootPage, const UIWidget &widget, const UIRect &widgetRect)
{
        UIRect rootRect;
        rootPage.GetWorldRect(rootRect);

        const long rootWidth = std::max(1L, rootRect.Width());
        const long rootHeight = std::max(1L, rootRect.Height());
        const float layoutScaleX = computeHdScale(rootWidth, 1024L);
        const float layoutScaleY = computeHdScale(rootHeight, 768L);

        const long recommendedWidth = computeHdTarget(64L, layoutScaleX);
        const long recommendedHeight = computeHdTarget(48L, layoutScaleY);

        bool hdOpportunity = false;

        if (widgetRect.Width() < recommendedWidth || widgetRect.Height() < recommendedHeight)
        {
                hdOpportunity = true;

                VisualRegion recommendedRegion;
                recommendedRegion.rect = UIRect(widgetRect.left, widgetRect.top, widgetRect.left + recommendedWidth, widgetRect.top + recommendedHeight);
                recommendedRegion.color = makeColor(255, 200, 80, 40);
                m_regions.push_back(recommendedRegion);

                std::ostringstream message;
                message << "HD Tip: " << widgetRect.Width() << "x" << widgetRect.Height() << " px < target "
                        << recommendedWidth << "x" << recommendedHeight << " px.";
                addDetail(m_details, message.str(), 8);
        }

        const UIImage *image = UI_ASOBJECT(UIImage, &widget);
        if (image)
        {
                const UIImageStyle *style = image->GetImageStyle();
                if (style)
                {
                        const long nativeWidth = style->GetWidth();
                        const long nativeHeight = style->GetHeight();

                        if (nativeWidth > 0 && nativeHeight > 0)
                        {
                                const float scaleX = static_cast<float>(widgetRect.Width()) / static_cast<float>(nativeWidth);
                                const float scaleY = static_cast<float>(widgetRect.Height()) / static_cast<float>(nativeHeight);
                                const float maxScale = std::max(scaleX, scaleY);
                                const float minScale = std::min(scaleX, scaleY);

                                if (maxScale > 1.25f)
                                {
                                        hdOpportunity = true;

                                        std::ostringstream message;
                                        message << "HD Tip: art stretched " << formatPercent(maxScale) << " of native "
                                                << nativeWidth << "x" << nativeHeight << " px.";
                                        addDetail(m_details, message.str(), 8);

                                        VisualRegion nativeRegion;
                                        nativeRegion.rect = UIRect(widgetRect.left, widgetRect.top, widgetRect.left + nativeWidth, widgetRect.top + nativeHeight);
                                        nativeRegion.color = makeColor(255, 96, 96, 32);
                                        m_regions.push_back(nativeRegion);
                                }
                                else if (minScale < 0.85f)
                                {
                                        hdOpportunity = true;

                                        std::ostringstream message;
                                        message << "HD Tip: art scaled to " << formatPercent(minScale) << "; verify clarity.";
                                        addDetail(m_details, message.str(), 8);
                                }
                        }
                }
                else
                {
                        hdOpportunity = true;
                        addDetail(m_details, "HD Tip: Assign an image style to use high-resolution art.", 8);
                }
        }

        const UIText *text = UI_ASOBJECT(UIText, &widget);
        if (text)
        {
                const long leading = text->GetMaximumCharHeight();
                if (leading > 0)
                {
                        const long recommendedLeading = computeHdTarget(20L, layoutScaleY);
                        if (leading < recommendedLeading)
                        {
                                hdOpportunity = true;

                                std::ostringstream message;
                                message << "HD Tip: text leading " << leading << " px; aim for " << recommendedLeading << " px.";
                                addDetail(m_details, message.str(), 8);
                        }
                }
        }

        if (!hdOpportunity)
        {
                addDetail(m_details, "HD ready: sizing meets high-density targets.", 8);
        }
}

//-----------------------------------------------------------------

void DesignAssistant::buildHdInsightsForGroup(const UIPage &rootPage, const std::vector<const UIWidget *> &widgets, const std::vector<UIRect> &rects, const UIRect &bounding)
{
        if (widgets.empty())
        {
                return;
        }

        UIRect rootRect;
        rootPage.GetWorldRect(rootRect);

        const long rootWidth = std::max(1L, rootRect.Width());
        const long rootHeight = std::max(1L, rootRect.Height());
        const float layoutScaleX = computeHdScale(rootWidth, 1024L);
        const float layoutScaleY = computeHdScale(rootHeight, 768L);

        const long recommendedWidth = computeHdTarget(64L, layoutScaleX);
        const long recommendedHeight = computeHdTarget(48L, layoutScaleY);

        std::size_t undersized = 0;
        for (std::size_t i = 0; i < rects.size(); ++i)
        {
                if (rects[i].Width() < recommendedWidth || rects[i].Height() < recommendedHeight)
                {
                        ++undersized;
                }
        }

        if (undersized > 0)
        {
                VisualRegion hdRegion;
                hdRegion.rect = bounding;
                hdRegion.color = makeColor(255, 180, 80, 24);
                m_regions.push_back(hdRegion);

                std::ostringstream message;
                message << "HD Tip: " << undersized << '/' << rects.size() << " widgets are below "
                        << recommendedWidth << 'x' << recommendedHeight << " px.";
                addDetail(m_details, message.str(), 8);
        }
        else
        {
                addDetail(m_details, "HD ready group sizing.", 8);
        }

        if (rootWidth > 0 && rootHeight > 0)
        {
                const double coverageNumerator = static_cast<double>(bounding.Width()) * static_cast<double>(bounding.Height());
                const double coverageDenominator = static_cast<double>(rootWidth) * static_cast<double>(rootHeight);
                if (coverageDenominator > 0.0)
                {
                        const double coverage = coverageNumerator / coverageDenominator;
                        if (coverage < 0.05)
                        {
                                std::ostringstream message;
                                message << "HD Tip: group occupies " << static_cast<int>(coverage * 100.0 + 0.5)
                                        << "% of canvas; consider scaling up.";
                                addDetail(m_details, message.str(), 8);
                        }
                }
        }
}

//-----------------------------------------------------------------

void DesignAssistant::composeStatusText()
{
        std::ostringstream builder;
        bool firstSegment = true;

        if (!m_headline.empty())
        {
                builder << m_headline;
                firstSegment = false;
        }

        bool densityDetailPresent = false;

        for (std::vector<std::string>::const_iterator it = m_details.begin(); it != m_details.end(); ++it)
        {
                if (it->empty())
                {
                        continue;
                }

                if (!densityDetailPresent && it->find("Density ") == 0)
                {
                        densityDetailPresent = true;
                }

                if (!firstSegment)
                {
                        builder << "  |  ";
                }
                builder << *it;
                firstSegment = false;
        }

        if (m_layoutDensityScore > 0.0f && !densityDetailPresent)
        {
                if (!firstSegment)
                {
                        builder << "  |  ";
                }

                builder << "Density " << formatPercent(m_layoutDensityScore);

                if (m_layoutDensityWarning)
                {
                        builder << " (tight)";
                }

                firstSegment = false;
        }

        if (firstSegment)
        {
                builder << "Assistant: Ready";
        }

        m_statusText = builder.str();
}

//-----------------------------------------------------------------

bool DesignAssistant::hasAutoLayoutRecommendation() const
{
        return m_hasAutoLayoutPlan;
}

//-----------------------------------------------------------------

int DesignAssistant::getRecommendedLayoutColumns() const
{
        return m_hasAutoLayoutPlan ? m_recommendedLayoutColumns : 0;
}

//-----------------------------------------------------------------

int DesignAssistant::getRecommendedLayoutRows() const
{
        return m_hasAutoLayoutPlan ? m_recommendedLayoutRows : 0;
}

//-----------------------------------------------------------------

float DesignAssistant::getLayoutDensityScore() const
{
        return m_layoutDensityScore;
}

//-----------------------------------------------------------------

bool DesignAssistant::hasLayoutDensityWarning() const
{
        return m_layoutDensityWarning;
}

//-----------------------------------------------------------------
