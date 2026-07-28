#ifndef slic3r_GUI_StaticLine_hpp_
#define slic3r_GUI_StaticLine_hpp_

#include "../wxExtensions.hpp"
#include "wx/window.h"

#include <functional>

class StaticLine : public wxWindow
{
public:
    StaticLine(wxWindow *parent, bool vertical = false, const wxString &label = {}, const wxString &icon = {});

public:
    void SetLabel(const wxString& label) override;

    void SetIcon(const wxString& icon);

    void SetLineColour(wxColour color);

    void SetCollapsible(bool collapsible, bool collapsed, std::function<void(bool)> on_toggle = {});
    void SetCollapsed(bool collapsed);
    
    void Rescale();

private:
    wxColour       lineColor;
    bool vertical;
    ScalableBitmap icon;
    bool m_collapsible {false};
    bool m_collapsed {false};
    std::function<void(bool)> m_on_toggle;

private:
    void paintEvent(wxPaintEvent& evt);
    void mouseReleased(wxMouseEvent& evt);

    void messureSize();

    void render(wxDC &dc);

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticLine_hpp_
