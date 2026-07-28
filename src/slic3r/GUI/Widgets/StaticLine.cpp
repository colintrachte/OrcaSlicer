#include "StaticLine.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "../I18N.hpp"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>

#include <utility>

BEGIN_EVENT_TABLE(StaticLine, wxWindow)

// catch paint events
EVT_PAINT(StaticLine::paintEvent)

END_EVENT_TABLE()

StaticLine::StaticLine(wxWindow *parent, bool vertical, const wxString &label, const wxString &icon)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , vertical(vertical)
{
    wxWindow::SetBackgroundColour(parent->GetBackgroundColour());
    this->lineColor = wxColour("#EEEEEE");
    DisableFocusFromKeyboard();
    SetFont(Label::Body_14);
    wxWindow::SetLabel(label);
    SetIcon(icon);
    Bind(wxEVT_LEFT_UP, &StaticLine::mouseReleased, this);
}

void StaticLine::SetLabel(const wxString& label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

void StaticLine::SetIcon(const wxString &icon)
{
    this->icon = icon.IsEmpty() ? ScalableBitmap() 
        : ScalableBitmap(this, icon.ToStdString(), 18);
    messureSize();
    Refresh();
}

void StaticLine::SetLineColour(wxColour color)
{
    this->lineColor = color;
}

void StaticLine::SetCollapsible(bool collapsible, bool collapsed, std::function<void(bool)> on_toggle)
{
    m_collapsible = collapsible;
    m_collapsed   = collapsed;
    m_on_toggle   = std::move(on_toggle);
    SetCursor(m_collapsible ? wxCursor(wxCURSOR_HAND) : wxNullCursor);
    SetToolTip(m_collapsible ? (m_collapsed ? _L("Expand section") : _L("Collapse section")) : wxString());
    messureSize();
    Refresh();
}

void StaticLine::SetCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;

    m_collapsed = collapsed;
    if (m_collapsible)
        SetToolTip(m_collapsed ? _L("Expand section") : _L("Collapse section"));
    Refresh();
}

void StaticLine::Rescale()
{
    if (this->icon.bmp().IsOk())
        this->icon.msw_rescale();
    messureSize();
}

void StaticLine::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

void StaticLine::mouseReleased(wxMouseEvent& evt)
{
    if (m_collapsible && GetClientRect().Contains(evt.GetPosition())) {
        SetCollapsed(!m_collapsed);
        if (m_on_toggle)
            m_on_toggle(m_collapsed);
    }
    evt.Skip();
}

void StaticLine::messureSize()
{
    wxClientDC dc(this);
    wxSize textSize = dc.GetTextExtent(GetLabel());
    wxSize szContent = textSize;
    if (m_collapsible)
        szContent.x += FromDIP(16);
    if (this->icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            // BBS norrow size between text and icon
            szContent.x += 5;
        }
        wxSize szIcon = this->icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y) szContent.y = szIcon.y;
    }
    if (vertical)
        szContent.y += 10;
    else
        szContent.x += 10;
    SetMinSize(szContent);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void StaticLine::render(wxDC& dc)
{
    wxSize size = GetSize();
    wxSize textSize;
    auto   label = GetLabel();
    if (!label.IsEmpty()) textSize = dc.GetTextExtent(label);
    wxRect titleRect{{0, 0}, size};
    titleRect.height = wxMax(icon.GetBmpHeight(), textSize.GetHeight());
    int contentWidth = icon.GetBmpWidth() + ((icon.bmp().IsOk() && textSize.GetWidth() > 0) ? 5 : 0) +
                textSize.GetWidth();
    if (m_collapsible)
        contentWidth += FromDIP(16);
    if (vertical) titleRect.Deflate((size.GetWidth() - contentWidth) / 2, 0);
    if (m_collapsible) {
        const int arrow_size = FromDIP(5);
        const int arrow_x = titleRect.x + FromDIP(2);
        const int arrow_y = size.y / 2;
        wxPoint arrow[3];
        if (m_collapsed) {
            arrow[0] = {arrow_x, arrow_y - arrow_size};
            arrow[1] = {arrow_x, arrow_y + arrow_size};
            arrow[2] = {arrow_x + arrow_size, arrow_y};
        } else {
            arrow[0] = {arrow_x, arrow_y - arrow_size / 2};
            arrow[1] = {arrow_x + 2 * arrow_size, arrow_y - arrow_size / 2};
            arrow[2] = {arrow_x + arrow_size, arrow_y + arrow_size / 2};
        }
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::darkModeColorFor(GetForegroundColour())));
        dc.DrawPolygon(3, arrow);
        titleRect.x += FromDIP(16);
    }
    if (icon.bmp().IsOk()) {
        dc.DrawBitmap(icon.bmp(), {titleRect.x, (size.y - icon.GetBmpHeight()) / 2});
        titleRect.x += icon.GetBmpWidth() + 5;
    }
    if (!label.IsEmpty()) {
        dc.SetTextForeground(StateColor::darkModeColorFor(GetForegroundColour()));
        dc.DrawText(label, titleRect.x, (size.GetHeight() - textSize.GetHeight()) / 2);
        titleRect.x += textSize.GetWidth() + 5;
    }
    dc.SetPen(wxPen(StateColor::darkModeColorFor(lineColor)));
    if (vertical) {
        size.x /= 2;
        if (titleRect.y > 0) titleRect.y += 5;
        dc.DrawLine(size.x, titleRect.y, size.x, size.y);
    } else {
        size.y /= 2;
        dc.DrawLine(titleRect.x, size.y, size.x, size.y);
    }
}
