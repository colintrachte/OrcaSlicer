#pragma once

#include "GUI_Utils.hpp"

#include <wx/treectrl.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

class wxButton;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI {

class PresetInspectorDialog : public DPIDialog
{
public:
    using ReportGenerator = std::function<std::string()>;
    using RecoveryAction  = std::function<std::pair<bool, std::string>()>;
    using ParentRecoveryAction = std::function<std::pair<bool, std::string>(const std::string &)>;

    struct Entry
    {
        std::string     label;
        ReportGenerator report;
        std::string     action_label;
        RecoveryAction  action;
        std::vector<std::string> current_path;
        std::vector<std::string> suggested_parents;
        ParentRecoveryAction     parent_action;
        std::string              action_hint;
        std::string              action_before_label;
        std::string              action_after_label;
    };

    PresetInspectorDialog(wxWindow *parent, const std::vector<Entry> &entries, size_t initial_index = 0);

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void show_report(size_t index);
    void run_action();
    void run_parent_action(const std::string &parent_name);
    void show_action_result(const std::pair<bool, std::string> &result);
    void rebuild_repair_tree(size_t index);

    std::vector<Entry>           m_entries;
    std::vector<ReportGenerator> m_report_generators;
    std::vector<std::string>     m_reports;
    std::vector<RecoveryAction>  m_actions;
    std::vector<std::string>     m_action_labels;
    wxTextCtrl                  *m_report_text { nullptr };
    wxButton                    *m_action_button { nullptr };
    wxButton                    *m_use_parent_button { nullptr };
    wxStaticText                *m_repair_hint { nullptr };
    wxTreeCtrl                  *m_current_tree { nullptr };
    wxTreeCtrl                  *m_repair_tree { nullptr };
    wxTreeItemId                 m_drag_source;
    size_t                       m_selected { 0 };
};

} // namespace Slic3r::GUI
