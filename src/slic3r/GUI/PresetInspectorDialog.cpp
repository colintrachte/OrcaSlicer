#include "PresetInspectorDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace Slic3r::GUI {

namespace {

enum class RecoveryTreeRole { CurrentEntry, Orphan, Candidate };

class RecoveryTreeData final : public wxTreeItemData
{
public:
    RecoveryTreeData(RecoveryTreeRole role, size_t entry_index, std::string value = {})
        : role(role), entry_index(entry_index), value(std::move(value))
    {}

    RecoveryTreeRole role;
    size_t           entry_index;
    std::string      value;
};

} // namespace

PresetInspectorDialog::PresetInspectorDialog(
    wxWindow *parent, const std::vector<Entry> &entries, size_t initial_index)
    : DPIDialog(parent, wxID_ANY, _L("Preset Relationships and Migration"), wxDefaultPosition,
                wxSize(760, 640), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    m_entries = entries;
    SetSize(FromDIP(wxSize(760, 640)));
    SetFont(wxGetApp().normal_font());

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    auto *splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3D);
    auto *current_panel = new wxPanel(splitter);
    auto *repair_panel  = new wxPanel(splitter);

    auto *current_sizer = new wxBoxSizer(wxVERTICAL);
    current_sizer->Add(new wxStaticText(current_panel, wxID_ANY, _L("Current relationships")), 0,
                       wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_current_tree = new wxTreeCtrl(current_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxBORDER_SIMPLE);
    current_sizer->Add(m_current_tree, 1, wxEXPAND | wxALL, FromDIP(8));
    current_panel->SetSizer(current_sizer);

    auto *repair_sizer = new wxBoxSizer(wxVERTICAL);
    repair_sizer->Add(new wxStaticText(repair_panel, wxID_ANY, _L("What OrcaSlicer found")), 0,
                      wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_report_text = new wxTextCtrl(repair_panel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
    repair_sizer->Add(m_report_text, 1, wxEXPAND | wxALL, FromDIP(8));
    m_repair_hint = new wxStaticText(repair_panel, wxID_ANY, wxString());
    repair_sizer->Add(m_repair_hint, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
    m_repair_tree = new wxTreeCtrl(repair_panel, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 190)),
                                   wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE | wxBORDER_SIMPLE);
    repair_sizer->Add(m_repair_tree, 0, wxEXPAND | wxALL, FromDIP(8));

    auto *repair_buttons = new wxBoxSizer(wxHORIZONTAL);
    m_action_button = new wxButton(repair_panel, wxID_ANY);
    m_action_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { run_action(); });
    repair_buttons->Add(m_action_button, 0, wxRIGHT, FromDIP(8));
    m_use_parent_button = new wxButton(repair_panel, wxID_ANY, _L("Use Selected Base"));
    m_use_parent_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const wxTreeItemId selected = m_repair_tree->GetSelection();
        auto *data = selected.IsOk() ? dynamic_cast<RecoveryTreeData *>(m_repair_tree->GetItemData(selected)) : nullptr;
        if (data != nullptr && data->role == RecoveryTreeRole::Candidate)
            run_parent_action(data->value);
    });
    repair_buttons->Add(m_use_parent_button);
    repair_sizer->Add(repair_buttons, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    repair_panel->SetSizer(repair_sizer);

    splitter->SplitVertically(current_panel, repair_panel, FromDIP(280));
    splitter->SetMinimumPaneSize(FromDIP(190));
    sizer->Add(splitter, 1, wxEXPAND | wxALL, FromDIP(8));

    const wxTreeItemId root = m_current_tree->AddRoot(_L("Preset relationships"));
    std::string current_category;
    wxTreeItemId category_item;
    wxTreeItemId first_entry;
    wxTreeItemId initial_entry;
    for (size_t index = 0; index < entries.size(); ++ index) {
        const Entry &entry = entries[index];
        m_report_generators.push_back(entry.report);
        m_reports.emplace_back();
        m_actions.push_back(entry.action);
        m_action_labels.push_back(entry.action_label);
        const size_t separator = entry.label.find(':');
        const std::string category = separator == std::string::npos ? std::string("Presets")
                                                                    : entry.label.substr(0, separator);
        if (! category_item.IsOk() || category != current_category) {
            current_category = category;
            category_item = m_current_tree->AppendItem(root, from_u8(category));
        }
        wxTreeItemId parent = category_item;
        const std::vector<std::string> path = entry.current_path.empty()
            ? std::vector<std::string>{ entry.label }
            : entry.current_path;
        for (size_t path_index = 0; path_index < path.size(); ++ path_index) {
            const bool leaf = path_index + 1 == path.size();
            parent = m_current_tree->AppendItem(
                parent, from_u8(path[path_index]), -1, -1,
                leaf ? new RecoveryTreeData(RecoveryTreeRole::CurrentEntry, index) : nullptr);
        }
        if (! first_entry.IsOk())
            first_entry = parent;
        if (index == initial_index)
            initial_entry = parent;
    }
    m_current_tree->Expand(root);
    wxTreeItemIdValue cookie;
    for (wxTreeItemId child = m_current_tree->GetFirstChild(root, cookie);
         child.IsOk(); child = m_current_tree->GetNextChild(root, cookie))
        m_current_tree->Expand(child);

    m_current_tree->Bind(wxEVT_TREE_SEL_CHANGED, [this](wxTreeEvent &event) {
        auto *data = dynamic_cast<RecoveryTreeData *>(m_current_tree->GetItemData(event.GetItem()));
        if (data != nullptr && data->role == RecoveryTreeRole::CurrentEntry)
            show_report(data->entry_index);
    });

    m_repair_tree->Bind(wxEVT_TREE_SEL_CHANGED, [this](wxTreeEvent &event) {
        auto *data = dynamic_cast<RecoveryTreeData *>(m_repair_tree->GetItemData(event.GetItem()));
        m_use_parent_button->Enable(data != nullptr && data->role == RecoveryTreeRole::Candidate);
    });
    m_repair_tree->Bind(wxEVT_TREE_BEGIN_DRAG, [this](wxTreeEvent &event) {
        auto *data = dynamic_cast<RecoveryTreeData *>(m_repair_tree->GetItemData(event.GetItem()));
        if (data != nullptr && data->role == RecoveryTreeRole::Orphan) {
            m_drag_source = event.GetItem();
            event.Allow();
        }
    });
    m_repair_tree->Bind(wxEVT_TREE_END_DRAG, [this](wxTreeEvent &event) {
        auto *data = dynamic_cast<RecoveryTreeData *>(m_repair_tree->GetItemData(event.GetItem()));
        if (m_drag_source.IsOk() && data != nullptr && data->role == RecoveryTreeRole::Candidate)
            run_parent_action(data->value);
        m_drag_source = wxTreeItemId();
    });

    auto *button_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *import_files = new wxButton(this, wxID_OPEN, _L("Import Preset Files") + dots);
    import_files->SetToolTip(_L("Bring in selected printer, process, or filament preset files."));
    import_files->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OPEN); });
    button_sizer->Add(import_files, 0, wxRIGHT, FromDIP(8));
    auto *import_folder = new wxButton(this, wxID_MORE, _L("Find Presets in Folder") + dots);
    import_folder->SetToolTip(_L("Scan another OrcaSlicer data folder, version backup, or copied preset folder."));
    import_folder->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_MORE); });
    button_sizer->Add(import_folder);
    button_sizer->AddStretchSpacer();
    auto *close = new wxButton(this, wxID_CLOSE, _L("Close"));
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CLOSE); });
    button_sizer->Add(close);
    sizer->Add(button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    SetSizer(sizer);
    wxGetApp().UpdateDarkUI(this);
    if (first_entry.IsOk()) {
        const wxTreeItemId selected = initial_entry.IsOk() ? initial_entry : first_entry;
        m_current_tree->SelectItem(selected);
        show_report(initial_entry.IsOk() ? initial_index : 0);
    }
    CentreOnParent();
}

void PresetInspectorDialog::show_report(size_t index)
{
    if (index >= m_reports.size())
        return;
    if (m_reports[index].empty())
        m_reports[index] = m_report_generators[index]();
    m_report_text->SetValue(from_u8(m_reports[index]));
    m_selected = index;
    const bool has_action = index < m_actions.size() && static_cast<bool>(m_actions[index]);
    m_action_button->Show(has_action);
    if (has_action)
        m_action_button->SetLabel(from_u8(m_action_labels[index]));
    rebuild_repair_tree(index);
    Layout();
}

void PresetInspectorDialog::rebuild_repair_tree(size_t index)
{
    m_repair_tree->DeleteAllItems();
    m_use_parent_button->Hide();
    if (index >= m_entries.size())
        return;
    const Entry &entry = m_entries[index];
    const wxTreeItemId root = m_repair_tree->AddRoot(_L("Repair workspace"));
    if (! entry.suggested_parents.empty() && entry.parent_action) {
        m_repair_hint->SetLabel(_L("Drag the unavailable preset onto the base it should use, or select a base to repair the existing preset."));
        m_repair_tree->AppendItem(root, from_u8(entry.label + "  (drag me)"), -1, -1,
                                  new RecoveryTreeData(RecoveryTreeRole::Orphan, index));
        const wxTreeItemId candidates = m_repair_tree->AppendItem(root, _L("Likely base presets"));
        for (const std::string &parent : entry.suggested_parents)
            m_repair_tree->AppendItem(candidates, from_u8(parent), -1, -1,
                                      new RecoveryTreeData(RecoveryTreeRole::Candidate, index, parent));
        m_repair_tree->Expand(root);
        m_repair_tree->Expand(candidates);
        m_use_parent_button->Show();
        m_use_parent_button->Enable(false);
    } else if (entry.action) {
        m_repair_hint->SetLabel(entry.action_hint.empty()
            ? _L("OrcaSlicer can preserve the original and create a repaired standalone copy.")
            : from_u8(entry.action_hint));
        m_repair_tree->AppendItem(root, entry.action_before_label.empty()
            ? _L("Original preset (unchanged)") : from_u8(entry.action_before_label));
        m_repair_tree->AppendItem(root, entry.action_after_label.empty()
            ? _L("Recovered standalone copy") : from_u8(entry.action_after_label));
        m_repair_tree->Expand(root);
    } else {
        m_repair_hint->SetLabel(_L("No safe automatic repair is available for this item."));
        m_repair_tree->AppendItem(root, _L("Manual review required"));
        m_repair_tree->Expand(root);
    }
    m_repair_hint->Wrap(FromDIP(430));
    Layout();
}

void PresetInspectorDialog::run_action()
{
    if (m_selected >= m_actions.size() || ! m_actions[m_selected])
        return;
    show_action_result(m_actions[m_selected]());
}

void PresetInspectorDialog::run_parent_action(const std::string &parent_name)
{
    if (m_selected >= m_entries.size() || ! m_entries[m_selected].parent_action)
        return;
    MessageDialog confirmation(
        this,
        wxString::Format(_L("Use '%s' as this preset's base?\n\n"
                            "OrcaSlicer will repair the existing preset, keep its own settings and version, and save an untouched backup. No duplicate preset will be created."),
                         from_u8(parent_name)),
        _L("Confirm Preset Relationship"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    if (confirmation.ShowModal() != wxID_YES)
        return;
    show_action_result(m_entries[m_selected].parent_action(parent_name));
}

void PresetInspectorDialog::show_action_result(const std::pair<bool, std::string> &result_value)
{
    const auto &[succeeded, message] = result_value;
    if (message.empty())
        return;
    MessageDialog result(this, from_u8(message),
                         succeeded ? _L("Preset Updated") : _L("Preset Update Failed"), wxOK);
    result.ShowModal();
    if (succeeded)
        EndModal(wxID_OK);
}

void PresetInspectorDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    Layout();
    Refresh();
}

} // namespace Slic3r::GUI
