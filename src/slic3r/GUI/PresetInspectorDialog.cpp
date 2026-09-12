#include "PresetInspectorDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"

#include <boost/filesystem.hpp>
#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/filedlg.h>
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

    m_consolidate_button = new wxButton(repair_panel, wxID_ANY, _L("Consolidate..."));
    m_consolidate_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { run_consolidate(); });
    repair_buttons->Add(m_consolidate_button, 0, wxRIGHT, FromDIP(8));

    m_export_prusa_button = new wxButton(repair_panel, wxID_ANY, _L("Export Prusa .ini..."));
    m_export_prusa_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { run_export_prusa(); });
    repair_buttons->Add(m_export_prusa_button, 0, wxRIGHT, FromDIP(8));

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

    const bool has_consolidate = index < m_entries.size() && static_cast<bool>(m_entries[index].consolidate_action);
    m_consolidate_button->Show(has_consolidate);

    const bool has_export_prusa = index < m_entries.size() && static_cast<bool>(m_entries[index].export_prusa_action);
    m_export_prusa_button->Show(has_export_prusa);

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
    if (entry.parent_action) {
        m_repair_hint->SetLabel(_L("Drag the unavailable preset onto the base it should use, or select a base to repair the existing preset."));
        m_repair_tree->AppendItem(root, from_u8(entry.label + "  (drag me)"), -1, -1,
                                  new RecoveryTreeData(RecoveryTreeRole::Orphan, index));
        if (! entry.suggested_parents.empty()) {
            const wxTreeItemId candidates = m_repair_tree->AppendItem(root, _L("Likely base presets (Name match)"));
            for (const std::string &parent : entry.suggested_parents)
                m_repair_tree->AppendItem(candidates, from_u8(parent), -1, -1,
                                          new RecoveryTreeData(RecoveryTreeRole::Candidate, index, parent));
            m_repair_tree->Expand(candidates);
        }
        if (! entry.all_eligible_parents.empty()) {
            const wxTreeItemId all_parents = m_repair_tree->AppendItem(root, _L("Browse all base presets"));
            for (const std::string &parent : entry.all_eligible_parents)
                m_repair_tree->AppendItem(all_parents, from_u8(parent), -1, -1,
                                          new RecoveryTreeData(RecoveryTreeRole::Candidate, index, parent));
            if (entry.suggested_parents.empty())
                m_repair_tree->Expand(all_parents);
        }
        m_repair_tree->Expand(root);
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
    } else if (! entry.all_eligible_parents.empty()) {
        m_repair_hint->SetLabel(_L("Base presets available in this profile."));
        const wxTreeItemId all_parents = m_repair_tree->AppendItem(root, _L("Available base presets"));
        for (const std::string &parent : entry.all_eligible_parents)
            m_repair_tree->AppendItem(all_parents, from_u8(parent), -1, -1,
                                      new RecoveryTreeData(RecoveryTreeRole::Candidate, index, parent));
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

void PresetInspectorDialog::run_consolidate()
{
    if (m_selected >= m_entries.size())
        return;
    const Entry &current_entry = m_entries[m_selected];
    if (! current_entry.consolidate_action)
        return;

    wxArrayString choices;
    std::vector<std::string> candidate_names;

    // First add similar presets with match percentage
    for (const auto &sim : current_entry.similar_summary) {
        choices.Add(wxString::Format(_L("%s (%.1f%% match)"), from_u8(sim.first), sim.second));
        candidate_names.push_back(sim.first);
    }

    // Also add any other user presets in this dialog
    for (const Entry &other : m_entries) {
        if (other.is_user_preset && other.preset_name != current_entry.preset_name) {
            if (std::find(candidate_names.begin(), candidate_names.end(), other.preset_name) == candidate_names.end()) {
                choices.Add(from_u8(other.preset_name));
                candidate_names.push_back(other.preset_name);
            }
        }
    }

    if (choices.empty()) {
        MessageDialog msg(this, _L("No other user presets found in this category to consolidate with."),
                          _L("Consolidate Profiles"), wxOK | wxICON_INFORMATION);
        msg.ShowModal();
        return;
    }

    wxSingleChoiceDialog chooser(
        this,
        wxString::Format(_L("Select a profile to consolidate with '%s':"), from_u8(current_entry.preset_name)),
        _L("Consolidate Profiles"), choices);

    if (chooser.ShowModal() != wxID_OK)
        return;

    const int sel = chooser.GetSelection();
    if (sel < 0 || static_cast<size_t>(sel) >= candidate_names.size())
        return;

    const std::string target_name = candidate_names[sel];

    wxArrayString actions;
    actions.Add(wxString::Format(_L("Keep '%s' as master; reparent '%s' to inherit from it"),
                                 from_u8(current_entry.preset_name), from_u8(target_name)));
    actions.Add(wxString::Format(_L("Keep '%s' as master; DELETE duplicate '%s' (backed up to .json.bak)"),
                                 from_u8(current_entry.preset_name), from_u8(target_name)));
    actions.Add(wxString::Format(_L("Keep '%s' as master; reparent '%s' to inherit from it"),
                                 from_u8(target_name), from_u8(current_entry.preset_name)));

    wxSingleChoiceDialog action_chooser(
        this,
        _L("Choose consolidation strategy:"),
        _L("Consolidation Action"), actions);

    if (action_chooser.ShowModal() != wxID_OK)
        return;

    const int action_sel = action_chooser.GetSelection();
    std::string winner_name, loser_name;
    bool delete_loser = false;

    if (action_sel == 0) {
        winner_name = current_entry.preset_name;
        loser_name = target_name;
        delete_loser = false;
    } else if (action_sel == 1) {
        winner_name = current_entry.preset_name;
        loser_name = target_name;
        delete_loser = true;
    } else if (action_sel == 2) {
        winner_name = target_name;
        loser_name = current_entry.preset_name;
        delete_loser = false;
    } else {
        return;
    }

    MessageDialog confirmation(
        this,
        wxString::Format(_L("Consolidate '%s' and '%s'?\n\n"
                            "Master (Winner): %s\n"
                            "Secondary (Loser): %s\n"
                            "Action: %s\n\n"
                            "An untouched backup (.json.bak) will be saved before any changes."),
                         from_u8(current_entry.preset_name), from_u8(target_name),
                         from_u8(winner_name), from_u8(loser_name),
                         delete_loser ? _L("Delete secondary profile") : _L("Reparent secondary to inherit from master")),
        _L("Confirm Consolidation"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);

    if (confirmation.ShowModal() != wxID_YES)
        return;

    show_action_result(current_entry.consolidate_action(winner_name, loser_name, delete_loser));
}

void PresetInspectorDialog::run_export_prusa()
{
    if (m_selected >= m_entries.size())
        return;
    const Entry &entry = m_entries[m_selected];
    if (! entry.export_prusa_action)
        return;

    wxString default_name = from_u8(entry.preset_name) + ".ini";
    wxString default_dir = wxEmptyString;
    const char *appdata = getenv("APPDATA");
    if (appdata != nullptr) {
        boost::filesystem::path ps_dir = boost::filesystem::path(appdata) / "PrusaSlicer";
        if (boost::filesystem::exists(ps_dir))
            default_dir = from_u8(ps_dir.string());
    }

    wxFileDialog save_dialog(
        this,
        _L("Export Preset to PrusaSlicer INI"),
        default_dir,
        default_name,
        _L("PrusaSlicer Configuration (*.ini)|*.ini"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (save_dialog.ShowModal() != wxID_OK)
        return;

    const std::string path = into_u8(save_dialog.GetPath());
    const auto result = entry.export_prusa_action(path);
    show_action_result(result);
}

void PresetInspectorDialog::show_action_result(const std::pair<bool, std::string> &result_value)
{
    const auto &[succeeded, message] = result_value;
    if (message.empty())
        return;
    MessageDialog result(this, from_u8(message),
                         succeeded ? _L("Preset Operation Completed") : _L("Preset Operation Failed"), wxOK);
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
