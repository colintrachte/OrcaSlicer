#include "PresetInspector.hpp"
#include "PresetBundle.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

namespace Slic3r {

namespace {

std::string preset_provenance(const Preset &preset)
{
    if (preset.is_default)
        return "Default";
    if (preset.is_system)
        return "System";
    if (preset.is_project_embedded)
        return "Project embedded";
    if (preset.is_external)
        return "External";
    if (preset.is_from_bundle()) {
        if (preset.name.rfind("_local/", 0) == 0)
            return "Local bundle";
        if (preset.name.rfind("_subscribed/", 0) == 0)
            return "Subscribed bundle";
        return "Bundle";
    }
    return "User";
}

bool option_differs(const DynamicPrintConfig &lhs, const DynamicPrintConfig &rhs, const std::string &key)
{
    const ConfigOption *lhs_option = lhs.option(key);
    const ConfigOption *rhs_option = rhs.option(key);
    return lhs_option != nullptr && (rhs_option == nullptr || *lhs_option != *rhs_option);
}

std::vector<std::string> find_orphaned_sidecars(const PresetCollection &collection)
{
    namespace fs = boost::filesystem;

    std::set<fs::path> directories;
    if (! collection.directory_path().empty())
        directories.insert(fs::path(collection.directory_path()));
    for (const Preset &preset : collection.get_presets())
        if (! preset.file.empty())
            directories.insert(fs::path(preset.file).parent_path());

    std::vector<std::string> result;
    for (const fs::path &directory : directories) {
        try {
            if (! fs::is_directory(directory))
                continue;
            for (const fs::directory_entry &entry : fs::directory_iterator(directory)) {
                if (! fs::is_regular_file(entry.path()) || entry.path().extension() != ".info")
                    continue;
                fs::path preset_file = entry.path();
                preset_file.replace_extension(".json");
                if (! fs::exists(preset_file))
                    result.push_back(entry.path().string());
            }
        } catch (const fs::filesystem_error &) {
            // Diagnostics must remain read-only and best-effort if a directory disappears mid-scan.
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void append_list(std::ostringstream &out, const char *heading, const std::vector<std::string> &items)
{
    out << heading << ":\n";
    if (items.empty()) {
        out << "  None\n";
        return;
    }
    for (const std::string &item : items)
        out << "  - " << item << '\n';
}

std::vector<std::string> name_tokens(const std::string &name)
{
    std::vector<std::string> tokens;
    std::string token;
    for (const unsigned char character : get_preset_bare_name(name)) {
        if (std::isalnum(character)) {
            token.push_back(static_cast<char>(std::tolower(character)));
        } else if (! token.empty()) {
            tokens.push_back(std::move(token));
            token.clear();
        }
    }
    if (! token.empty())
        tokens.push_back(std::move(token));
    return tokens;
}

void set_recovered_identity(DynamicPrintConfig &config, Preset::Type type, const std::string &name)
{
    switch (type) {
    case Preset::TYPE_PRINT:
        config.option<ConfigOptionString>("print_settings_id", true)->value = name;
        break;
    case Preset::TYPE_SLA_PRINT:
        config.option<ConfigOptionString>("sla_print_settings_id", true)->value = name;
        break;
    case Preset::TYPE_FILAMENT:
        config.option<ConfigOptionStrings>("filament_settings_id", true)->values = { name };
        break;
    case Preset::TYPE_SLA_MATERIAL:
        config.option<ConfigOptionString>("sla_material_settings_id", true)->value = name;
        break;
    case Preset::TYPE_PRINTER:
        config.option<ConfigOptionString>("printer_settings_id", true)->value = name;
        break;
    default:
        break;
    }
}

std::string unique_recovery_name(const PresetCollection &collection, const std::string &source_name)
{
    const std::string base_name = get_preset_bare_name(source_name) + " (Recovered)";
    return find_unique_candidate(base_name, " ", [&collection](const std::string &candidate) {
        return collection.find_preset(candidate, false) != nullptr ||
               boost::filesystem::exists(collection.path_from_name(candidate, true)) ||
               boost::filesystem::exists(collection.path_from_name(candidate, false));
    });
}

std::string unique_duplicate_copy_name(const PresetCollection &collection, const std::string &source_name)
{
    const std::string base_name = get_preset_bare_name(source_name) + " (duplicate copy)";
    return find_unique_candidate(base_name, " ", [&collection](const std::string &candidate) {
        return collection.find_preset(candidate, false) != nullptr ||
               boost::filesystem::exists(collection.path_from_name(candidate, true)) ||
               boost::filesystem::exists(collection.path_from_name(candidate, false));
    });
}

} // namespace

PresetInspection inspect_preset(
    const PresetCollection        &collection,
    const Preset                  &preset,
    const PresetWithVendorProfile *active_printer,
    const PresetWithVendorProfile *active_print)
{
    PresetInspection result;
    result.name       = preset.name;
    result.type       = Preset::get_type_string(preset.type);
    result.provenance = preset_provenance(preset);
    result.vendor     = preset.vendor == nullptr ? std::string() : preset.vendor->name;
    result.file       = preset.file;
    result.visible    = preset.is_visible;
    result.visibility_reason = preset.is_visible ? "Visible" :
        (preset.vendor == nullptr ? "Hidden by the preset visibility state."
                                  : "Hidden because its vendor, printer model, or variant is not enabled.");

    if (! preset.setting_id.empty()) {
        result.stable_identity = preset.setting_id;
        result.identity_source = "setting_id";
    } else if (! preset.filament_id.empty() && preset.filament_id != "__unknown__") {
        result.stable_identity = preset.filament_id;
        result.identity_source = "filament_id";
    } else {
        result.stable_identity = preset.name;
        result.identity_source = "canonical name fallback";
    }

    std::vector<const Preset *> chain { &preset };
    std::unordered_set<const Preset *> visited { &preset };
    const Preset *cursor = &preset;
    while (! cursor->inherits().empty()) {
        const Preset *parent = collection.get_preset_parent(*cursor);
        if (parent == nullptr) {
            result.broken_parent = cursor->inherits();
            break;
        }
        if (! visited.insert(parent).second) {
            result.broken_parent = parent->name + " (inheritance cycle)";
            break;
        }
        result.parent_chain.push_back(parent->name);
        chain.push_back(parent);
        cursor = parent;
    }

    for (const Preset *child : collection.get_preset_children(preset))
        result.child_presets.push_back(child->name);
    std::sort(result.child_presets.begin(), result.child_presets.end());

    result.unknown_keys = preset.loading_unknown_keys;
    std::sort(result.unknown_keys.begin(), result.unknown_keys.end());
    result.unknown_keys.erase(std::unique(result.unknown_keys.begin(), result.unknown_keys.end()), result.unknown_keys.end());
    result.orphaned_sidecars = find_orphaned_sidecars(collection);
    result.load_issues = collection.load_issues();

    result.compatible = preset.is_compatible;
    if (preset.type == Preset::TYPE_PRINTER) {
        result.compatibility_reason = "Printer presets provide the active compatibility context.";
    } else if (active_printer != nullptr) {
        const PresetWithVendorProfile candidate = collection.get_preset_with_vendor_profile(preset);
        std::string reason;
        result.compatible = is_compatible_with_printer(candidate, *active_printer, &reason);
        if (result.compatible && active_print != nullptr &&
            (preset.type == Preset::TYPE_FILAMENT || preset.type == Preset::TYPE_SLA_MATERIAL))
            result.compatible = is_compatible_with_print(candidate, *active_print, *active_printer, &reason);
        result.compatibility_reason = result.compatible ?
            (reason.empty() ? "Compatible with the active presets." : reason) : reason;
    } else {
        result.compatibility_reason = result.compatible ? "Marked compatible."
                                                        : "Marked incompatible; no active printer was supplied for evaluation.";
    }

    std::vector<std::string> keys = preset.config.keys();
    std::sort(keys.begin(), keys.end());
    for (const std::string &key : keys) {
        std::string source = chain.back()->name;
        for (size_t index = chain.size(); index > 1; -- index) {
            const Preset &child  = *chain[index - 2];
            const Preset &parent = *chain[index - 1];
            if (option_differs(child.config, parent.config, key))
                source = child.name;
        }
        result.effective_values.push_back({ key, preset.config.opt_serialize(key), std::move(source) });
    }
    return result;
}

bool preset_needs_recovery(const PresetCollection &collection, const Preset &preset)
{
    // Disabled system/vendor presets are intentionally hidden and would keep a global warning
    // permanently lit. Recovery focuses on personal/imported presets and rejected user files.
    if (preset.is_default || preset.is_system || preset.is_project_embedded)
        return false;
    // Unknown keys remain available in diagnostics, but a successfully loaded preset should not
    // light the global recovery warning when its visibility and relationships are healthy. There
    // is no safe repair button for an unknown setting, and current development builds may preserve
    // forward-compatible fields written by the same application family.
    return ! preset.is_visible || ! preset.is_compatible ||
           (! preset.inherits().empty() && collection.get_preset_parent(preset) == nullptr);
}

PresetRecoveryActionResult create_standalone_recovery_copy(
    PresetCollection                &collection,
    const Preset                    &source,
    const PresetRecoveryCopyOptions &options)
{
    PresetRecoveryActionResult result;
    if (source.is_default || source.is_system || source.is_project_embedded) {
        result.error = "Only personal or imported presets can be recovered as standalone copies.";
        return result;
    }
    if (source.file.empty() && collection.directory_path().empty()) {
        result.error = "The preset collection does not have a user preset folder.";
        return result;
    }

    const std::string candidate = unique_recovery_name(collection, source.name);

    std::vector<std::string> failures;
    try {
        const bool copied = collection.clone_presets(
            { &source }, failures,
            [&candidate, &options](Preset &copy, Preset::Type &type) {
                copy.name = candidate;
                set_recovered_identity(copy.config, type, candidate);
                if (! options.compatible_printer.empty() && type != Preset::TYPE_PRINTER) {
                    copy.config.option<ConfigOptionStrings>("compatible_printers", true)->values = {
                        options.compatible_printer
                    };
                }
                if (! options.compatible_print.empty() &&
                    (type == Preset::TYPE_FILAMENT || type == Preset::TYPE_SLA_MATERIAL)) {
                    copy.config.option<ConfigOptionStrings>("compatible_prints", true)->values = {
                        options.compatible_print
                    };
                }
            });
        if (! copied) {
            result.error = failures.empty() ? "OrcaSlicer could not create the recovered copy."
                                            : "A preset with the recovered name already exists.";
            return result;
        }
    } catch (const std::exception &error) {
        result.error = error.what();
        return result;
    }

    result.succeeded = true;
    result.preset_name = candidate;
    return result;
}

std::string project_preset_install_name(const Preset &source)
{
    if (! source.is_project_embedded || source.file.empty())
        return source.name;
    const std::string suffix = "(" + boost::filesystem::path(source.file).filename().string() + ")";
    if (source.name.size() > suffix.size() &&
        source.name.compare(source.name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return source.name.substr(0, source.name.size() - suffix.size());
    return source.name;
}

PresetRecoveryActionResult install_project_preset(
    PresetCollection &collection, const Preset &source, const std::string &installed_name)
{
    PresetRecoveryActionResult result;
    if (! source.is_project_embedded) {
        result.error = "Only presets loaded temporarily from a project can be installed this way.";
        return result;
    }
    if (installed_name.empty()) {
        result.error = "The installed preset needs a name.";
        return result;
    }
    if (collection.find_preset(installed_name, false, true) != nullptr) {
        result.error = "A preset named '" + installed_name + "' is already installed. Choose a different name.";
        return result;
    }

    std::vector<std::string> failures;
    try {
        const bool copied = collection.clone_presets(
            { &source }, failures,
            [&installed_name](Preset &copy, Preset::Type &type) {
                copy.name = installed_name;
                set_recovered_identity(copy.config, type, installed_name);
            });
        if (! copied) {
            result.error = failures.empty() ? "OrcaSlicer could not install the project preset."
                                            : "A preset with that name is already installed.";
            return result;
        }
    } catch (const std::exception &error) {
        result.error = error.what();
        return result;
    }

    result.succeeded = true;
    result.preset_name = installed_name;
    return result;
}

PresetRecoveryActionResult replace_user_preset_from_project(
    PresetCollection &collection, const Preset &source, const std::string &installed_name)
{
    PresetRecoveryActionResult result;
    if (! source.is_project_embedded) {
        result.error = "Only presets loaded temporarily from a project can replace a user preset this way.";
        return result;
    }
    Preset *existing = collection.find_preset(installed_name, false, true);
    if (existing == nullptr) {
        result.error = "The preset to replace is no longer installed.";
        return result;
    }
    if (! existing->can_overwrite() || existing->is_project_embedded) {
        result.error = "The existing preset is read-only and cannot be replaced.";
        return result;
    }

    const Preset existing_snapshot = *existing;
    boost::filesystem::path backup_path;
    try {
        const boost::filesystem::path existing_path(existing->file);
        if (! existing->file.empty() && boost::filesystem::is_regular_file(existing_path)) {
            backup_path = boost::filesystem::path(find_unique_candidate(existing->file + ".project-import-backup", "-",
                [](const std::string &candidate) { return boost::filesystem::exists(candidate); }));
            boost::filesystem::copy_file(existing_path, backup_path);
        }

        std::vector<std::string> failures;
        const bool copied = collection.clone_presets(
            { &source }, failures,
            [&installed_name, &existing_snapshot](Preset &copy, Preset::Type &type) {
                copy.name = installed_name;
                set_recovered_identity(copy.config, type, installed_name);
                copy.setting_id = existing_snapshot.setting_id;
                copy.user_id = existing_snapshot.user_id;
                copy.sync_info = existing_snapshot.setting_id.empty() ? "create" : "update";
                copy.updated_time = existing_snapshot.updated_time;
            }, true);
        if (! copied) {
            result.error = "OrcaSlicer could not replace the existing preset.";
            return result;
        }
    } catch (const std::exception &error) {
        if (! backup_path.empty()) {
            boost::system::error_code restore_error;
            boost::filesystem::copy_file(backup_path, existing_snapshot.file,
                                         boost::filesystem::copy_option::overwrite_if_exists, restore_error);
        }
        if (Preset *changed = collection.find_preset(installed_name, false, true))
            *changed = existing_snapshot;
        result.error = error.what();
        return result;
    }

    result.succeeded = true;
    result.preset_name = installed_name;
    result.backup_file = backup_path.string();
    return result;
}

PresetRecoveryActionResult rename_user_preset(
    PresetCollection &collection, const std::string &old_name, const std::string &new_name,
    PresetBundle *bundle)
{
    PresetRecoveryActionResult result;
    Preset *existing = collection.find_preset(old_name, false, true);
    if (existing == nullptr) {
        result.error = "The preset to rename is no longer available.";
        return result;
    }
    if (! existing->can_overwrite() || existing->is_project_embedded || existing->is_from_bundle()) {
        result.error = "Only a writable user preset can be renamed in place.";
        return result;
    }
    if (new_name.empty() || new_name == old_name) {
        result.error = "Enter a different non-empty preset name.";
        return result;
    }
    if (collection.find_preset(new_name, false, true) != nullptr) {
        result.error = "A preset named '" + new_name + "' already exists.";
        return result;
    }

    const Preset source_snapshot = *existing;
    const std::string original_selection = collection.get_selected_preset_name();
    const bool source_was_selected = original_selection == old_name;
    const Preset edited_snapshot = collection.get_edited_preset();
    Preset rename_source = source_snapshot;
    if (source_was_selected)
        rename_source.config = collection.get_edited_preset().config;

    std::vector<Preset> children;
    for (const Preset *child : collection.get_preset_children(*existing)) {
        if (! child->can_overwrite() || child->is_project_embedded || child->is_from_bundle()) {
            result.error = "A read-only child preset depends on this name, so OrcaSlicer cannot rename it safely.";
            return result;
        }
        children.push_back(*child);
    }

    struct ReferenceSnapshot {
        PresetCollection  *collection;
        std::string        name;
        DynamicPrintConfig config;
        bool               selected;
        DynamicPrintConfig edited_config;
    };
    std::vector<ReferenceSnapshot> references;
    const auto has_reference = [&old_name](const DynamicPrintConfig &config, const char *key) {
        if (const auto *option = dynamic_cast<const ConfigOptionString *>(config.option(key)))
            return option->value == old_name;
        if (const auto *option = dynamic_cast<const ConfigOptionStrings *>(config.option(key)))
            return std::find(option->values.begin(), option->values.end(), old_name) != option->values.end();
        return false;
    };
    const auto collect_references = [&](PresetCollection &dependent_collection, const char *key) {
        for (Preset &preset : dependent_collection) {
            if (! has_reference(preset.config, key))
                continue;
            if (! preset.can_overwrite() && ! preset.is_project_embedded) {
                result.error = "A read-only preset named '" + preset.name + "' refers to this name, so OrcaSlicer cannot rename it safely.";
                return false;
            }
            const bool selected = dependent_collection.get_selected_preset_name() == preset.name;
            references.push_back({ &dependent_collection, preset.name, preset.config, selected,
                                   selected ? dependent_collection.get_edited_preset().config : DynamicPrintConfig() });
        }
        return true;
    };
    if (bundle != nullptr) {
        bool references_ok = true;
        switch (collection.type()) {
        case Preset::TYPE_PRINTER:
            references_ok = collect_references(bundle->prints, "compatible_printers") &&
                            collect_references(bundle->filaments, "compatible_printers");
            break;
        case Preset::TYPE_PRINT:
            references_ok = collect_references(bundle->filaments, "compatible_prints") &&
                            collect_references(bundle->printers, "default_print_profile");
            break;
        case Preset::TYPE_FILAMENT:
            references_ok = collect_references(bundle->printers, "default_filament_profile");
            break;
        default: break;
        }
        if (! references_ok)
            return result;
    }

    struct PhysicalPrinterSnapshot {
        PhysicalPrinter  *printer;
        std::set<std::string> preset_names;
        DynamicPrintConfig config;
    };
    std::vector<PhysicalPrinterSnapshot> physical_printers;
    if (bundle != nullptr && collection.type() == Preset::TYPE_PRINTER) {
        for (PhysicalPrinter &printer : bundle->physical_printers)
            if (printer.get_preset_names().find(old_name) != printer.get_preset_names().end())
                physical_printers.push_back({ &printer, printer.preset_names, printer.config });
    }

    struct FileBackup { boost::filesystem::path source; boost::filesystem::path backup; };
    std::vector<FileBackup> backups;
    const auto backup_file = [&backups](const std::string &file) {
        if (file.empty() || ! boost::filesystem::is_regular_file(file))
            return;
        const boost::filesystem::path backup(find_unique_candidate(file + ".rename-backup", "-",
            [](const std::string &candidate) { return boost::filesystem::exists(candidate); }));
        boost::filesystem::copy_file(file, backup);
        backups.push_back({ boost::filesystem::path(file), std::move(backup) });
    };

    try {
        backup_file(source_snapshot.file);
        for (const Preset &child : children)
            backup_file(child.file);
        for (const ReferenceSnapshot &reference : references) {
            const Preset *preset = reference.collection->find_preset(reference.name, false, true);
            if (preset != nullptr && preset->can_overwrite())
                backup_file(preset->file);
        }
        for (const PhysicalPrinterSnapshot &snapshot : physical_printers)
            backup_file(snapshot.printer->file);

        collection.save_current_preset(new_name, source_snapshot.inherits().empty(), false, &rename_source);
        Preset *renamed = collection.find_preset(new_name, false, true);
        if (renamed == nullptr)
            throw std::runtime_error("OrcaSlicer could not create the renamed preset.");
        renamed->setting_id = source_snapshot.setting_id;
        renamed->user_id = source_snapshot.user_id;
        renamed->sync_info = source_snapshot.setting_id.empty() ? "create" : "update";
        renamed->updated_time = source_snapshot.updated_time;
        renamed->renamed_from = source_snapshot.renamed_from;
        if (std::find(renamed->renamed_from.begin(), renamed->renamed_from.end(), old_name) == renamed->renamed_from.end())
            renamed->renamed_from.push_back(old_name);
        set_recovered_identity(renamed->config, collection.type(), new_name);
        Preset *renamed_parent = renamed->inherits().empty()
            ? nullptr : collection.find_preset2(renamed->inherits(), true);
        renamed->save(renamed_parent == nullptr ? nullptr : &renamed_parent->config);

        for (const Preset &child_snapshot : children) {
            Preset *child = collection.find_preset(child_snapshot.name, false, true);
            if (child == nullptr)
                throw std::runtime_error("A child preset changed while the rename was in progress.");
            child->inherits() = new_name;
            child->base_id = renamed->setting_id;
            child->sync_info = child->setting_id.empty() ? "create" : "update";
            child->save(&renamed->config);
        }

        const auto replace_reference = [&old_name, &new_name](DynamicPrintConfig &config, const char *key) {
            if (auto *option = dynamic_cast<ConfigOptionString *>(config.option(key))) {
                if (option->value == old_name)
                    option->value = new_name;
            } else if (auto *option = dynamic_cast<ConfigOptionStrings *>(config.option(key))) {
                std::replace(option->values.begin(), option->values.end(), old_name, new_name);
            }
        };
        for (ReferenceSnapshot &reference : references) {
            Preset *preset = reference.collection->find_preset(reference.name, false, true);
            if (preset == nullptr)
                throw std::runtime_error("A referenced preset changed while the rename was in progress.");
            const char *key = collection.type() == Preset::TYPE_PRINTER ? "compatible_printers" :
                              collection.type() == Preset::TYPE_PRINT && reference.collection == &bundle->filaments ? "compatible_prints" :
                              collection.type() == Preset::TYPE_PRINT ? "default_print_profile" : "default_filament_profile";
            replace_reference(preset->config, key);
            if (reference.selected)
                replace_reference(reference.collection->get_edited_preset().config, key);
            if (preset->can_overwrite() && ! preset->file.empty()) {
                Preset *parent = preset->inherits().empty() ? nullptr :
                    reference.collection->find_preset2(preset->inherits(), true);
                preset->save(parent == nullptr ? nullptr : &parent->config);
            }
        }
        for (PhysicalPrinterSnapshot &snapshot : physical_printers) {
            snapshot.printer->delete_preset(old_name);
            snapshot.printer->add_preset(new_name);
            snapshot.printer->update_preset_names_in_config();
            if (! snapshot.printer->file.empty())
                snapshot.printer->save(nullptr);
        }

        if (! collection.delete_preset(old_name, true))
            throw std::runtime_error("OrcaSlicer could not remove the old preset after saving the new name.");
        collection.select_preset_by_name(new_name, true);
    } catch (const std::exception &error) {
        if (collection.find_preset(new_name, false, true) != nullptr)
            collection.delete_preset(new_name, true);
        for (const FileBackup &backup : backups) {
            boost::system::error_code restore_error;
            boost::filesystem::copy_file(backup.backup, backup.source,
                                         boost::filesystem::copy_option::overwrite_if_exists, restore_error);
        }
        Preset *restored_source = collection.find_preset(old_name, false, true);
        if (restored_source == nullptr)
            restored_source = &collection.load_preset(source_snapshot.file, old_name, source_snapshot.config, false);
        *restored_source = source_snapshot;
        for (const Preset &child_snapshot : children)
            if (Preset *child = collection.find_preset(child_snapshot.name, false, true))
                *child = child_snapshot;
        for (const ReferenceSnapshot &reference : references) {
            if (Preset *preset = reference.collection->find_preset(reference.name, false, true))
                preset->config = reference.config;
            if (reference.selected)
                reference.collection->get_edited_preset().config = reference.edited_config;
        }
        for (const PhysicalPrinterSnapshot &snapshot : physical_printers) {
            snapshot.printer->preset_names = snapshot.preset_names;
            snapshot.printer->config = snapshot.config;
        }
        collection.select_preset_by_name(original_selection, true);
        if (source_was_selected)
            collection.get_edited_preset() = edited_snapshot;
        result.error = error.what();
        return result;
    }

    result.succeeded = true;
    result.preset_name = new_name;
    if (! backups.empty())
        result.backup_file = backups.front().backup.string();
    return result;
}

std::string missing_parent_from_load_issue(const PresetLoadIssue &issue)
{
    static const std::string prefix = "Parent preset \"";
    static const std::string suffix = "\" could not be found.";
    if (issue.reason.rfind(prefix, 0) != 0 || issue.reason.size() <= prefix.size() + suffix.size() ||
        issue.reason.compare(issue.reason.size() - suffix.size(), suffix.size(), suffix) != 0)
        return {};
    return issue.reason.substr(prefix.size(), issue.reason.size() - prefix.size() - suffix.size());
}

std::vector<std::string> recovery_parent_candidates(
    const PresetCollection &collection, const std::string &missing_parent, size_t maximum)
{
    struct Candidate { int score; std::string name; };
    const std::vector<std::string> missing_tokens = name_tokens(missing_parent);
    std::vector<Candidate> candidates;
    for (const Preset &preset : collection.get_presets()) {
        if (preset.is_default || preset.is_project_embedded ||
            (! preset.inherits().empty() && collection.get_preset_parent(preset) == nullptr))
            continue;
        const std::vector<std::string> tokens = name_tokens(preset.name);
        int score = preset.is_system ? 4 : 0;
        score += preset.is_visible ? 2 : 0;
        score += preset.is_compatible ? 1 : 0;
        for (const std::string &token : tokens)
            if (std::find(missing_tokens.begin(), missing_tokens.end(), token) != missing_tokens.end())
                score += 20;
        const std::string bare = boost::algorithm::to_lower_copy(get_preset_bare_name(preset.name));
        const std::string missing = boost::algorithm::to_lower_copy(get_preset_bare_name(missing_parent));
        if (bare.find(missing) != std::string::npos || missing.find(bare) != std::string::npos)
            score += 50;
        // A visibility/system baseline alone does not make an arbitrary preset a
        // plausible replacement. Require at least one name relationship before
        // inviting the user to reparent destructive configuration differences.
        if (score >= 20)
            candidates.push_back({ score, preset.name });
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &left, const Candidate &right) {
        return left.score != right.score ? left.score > right.score : left.name < right.name;
    });
    if (candidates.size() > maximum)
        candidates.resize(maximum);
    std::vector<std::string> result;
    result.reserve(candidates.size());
    for (Candidate &candidate : candidates)
        result.push_back(std::move(candidate.name));
    return result;
}

PresetRecoveryActionResult reparent_rejected_preset(
    PresetCollection &collection, const PresetLoadIssue &issue, const std::string &parent_name)
{
    PresetRecoveryActionResult result;
    const std::string source_file = issue.file;
    const std::string missing_parent = missing_parent_from_load_issue(issue);
    if (missing_parent.empty()) {
        result.error = "This load problem is not a missing-base problem.";
        return result;
    }
    const Preset *parent = collection.find_preset(parent_name, false, true);
    if (parent == nullptr) {
        result.error = "The selected base preset is no longer available.";
        return result;
    }
    const std::string selected_parent_name = parent->name;
    const std::string selected_parent_id   = parent->setting_id;

    try {
        DynamicPrintConfig config_diff;
        std::map<std::string, std::string> key_values;
        std::string reason;
        ConfigSubstitutionContext substitutions(ForwardCompatibilitySubstitutionRule::Enable);
        config_diff.load_from_json(source_file, substitutions, true, key_values, reason);
        if (! reason.empty()) {
            result.error = reason;
            return result;
        }
        const ConfigOptionString *inherits = config_diff.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS);
        if (inherits == nullptr || inherits->value != missing_parent) {
            result.error = "The source preset changed after OrcaSlicer reported the missing base.";
            return result;
        }

        std::string extruder_id_name, extruder_variant_name;
        std::set<std::string> *key_set1 = nullptr, *key_set2 = nullptr;
        Preset::get_extruder_names_and_keysets(collection.type(), extruder_id_name, extruder_variant_name,
                                               &key_set1, &key_set2);
        DynamicPrintConfig recovered_config = parent->config;
        extend_default_config_length(config_diff, false, {});
        recovered_config.update_diff_values_to_child_config(
            config_diff, extruder_id_name, extruder_variant_name, *key_set1, *key_set2);
        Preset::normalize(recovered_config);
        recovered_config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS, true)->value = selected_parent_name;

        set_recovered_identity(recovered_config, collection.type(), issue.preset_name);
        std::string filament_id = key_values[BBL_JSON_KEY_FILAMENT_ID];
        if (filament_id.empty())
            filament_id = parent->filament_id;
        if (! filament_id.empty())
            recovered_config.set_key_value(BBL_JSON_KEY_FILAMENT_ID, new ConfigOptionString(filament_id));

        boost::optional<Semver> version = Semver::parse(key_values[BBL_JSON_KEY_VERSION]);
        const Semver recovered_version = version ? *version : parent->version;
        const boost::filesystem::path source_path(source_file);
        if (! boost::filesystem::is_regular_file(source_path)) {
            result.error = "The source preset file is no longer available.";
            return result;
        }

        const boost::filesystem::path backup_path(source_file + ".recovery-backup");
        if (! boost::filesystem::exists(backup_path))
            boost::filesystem::copy_file(source_path, backup_path);

        nlohmann::json source_json;
        {
            boost::nowide::ifstream input(source_file);
            if (! input) {
                result.error = "OrcaSlicer could not read the source preset file.";
                return result;
            }
            input >> source_json;
        }
        const auto relationship = source_json.find(BBL_JSON_KEY_INHERITS);
        if (relationship == source_json.end() || ! relationship->is_string() ||
            relationship->get<std::string>() != missing_parent) {
            result.error = "The source preset changed after OrcaSlicer reported the missing base.";
            return result;
        }
        source_json[BBL_JSON_KEY_INHERITS] = selected_parent_name;

        const boost::filesystem::path temporary_path(source_file + ".repairing");
        {
            boost::nowide::ofstream output(temporary_path.string(), std::ios::out | std::ios::trunc);
            if (! output) {
                result.error = "OrcaSlicer could not prepare the repaired preset file.";
                return result;
            }
            output << source_json.dump(4) << '\n';
            if (! output.good()) {
                result.error = "OrcaSlicer could not finish writing the repaired preset file.";
                return result;
            }
        }
        boost::system::error_code replace_error;
        boost::filesystem::copy_file(temporary_path, source_path,
                                     boost::filesystem::copy_option::overwrite_if_exists, replace_error);
        boost::system::error_code cleanup_error;
        boost::filesystem::remove(temporary_path, cleanup_error);
        if (replace_error) {
            result.error = "OrcaSlicer could not replace the preset file. The original backup was preserved.";
            return result;
        }

        Preset &recovered = collection.load_preset(source_file, issue.preset_name, std::move(recovered_config), false,
                                                   recovered_version);
        recovered.is_visible = true;
        recovered.is_external = false;
        recovered.is_system = false;
        recovered.base_id = selected_parent_id;
        recovered.filament_id = filament_id;
        collection.clear_load_issue(source_file);

        result.succeeded = true;
        result.preset_name = issue.preset_name;
        result.backup_file = backup_path.string();
    } catch (const std::exception &error) {
        result.error = error.what();
    }
    return result;
}

bool is_duplicate_name_load_issue(const PresetLoadIssue &issue)
{
    static const std::string marker = "Another preset with the same canonical name is already loaded.";
    return issue.reason == marker;
}

PresetRecoveryActionResult resolve_duplicate_preset(PresetCollection &collection, const PresetLoadIssue &issue)
{
    PresetRecoveryActionResult result;
    if (! is_duplicate_name_load_issue(issue)) {
        result.error = "This load problem is not a duplicate-name problem.";
        return result;
    }
    const boost::filesystem::path source_path(issue.file);
    if (! boost::filesystem::is_regular_file(source_path)) {
        result.error = "The duplicate preset file is no longer available.";
        return result;
    }

    DynamicPrintConfig parsed_config;
    std::map<std::string, std::string> key_values;
    std::string reason;
    ConfigSubstitutionContext substitutions(ForwardCompatibilitySubstitutionRule::Enable);
    try {
        parsed_config.load_from_json(issue.file, substitutions, true, key_values, reason);
    } catch (const std::exception &error) {
        result.error = std::string("OrcaSlicer could not read the duplicate file: ") + error.what();
        return result;
    }
    if (! reason.empty()) {
        result.error = "OrcaSlicer could not read the duplicate file: " + reason;
        return result;
    }

    DynamicPrintConfig resolved_config;
    const ConfigOptionString *inherits = parsed_config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS);
    if (inherits == nullptr || inherits->value.empty()) {
        // A parentless (base) preset's file already holds its full config.
        resolved_config = std::move(parsed_config);
    } else {
        const Preset *parent = collection.find_preset2(inherits->value, true);
        if (parent == nullptr) {
            result.error = "The duplicate file inherits from '" + inherits->value +
                "', which is not available, so OrcaSlicer cannot resolve its full settings.";
            return result;
        }
        std::string extruder_id_name, extruder_variant_name;
        std::set<std::string> *key_set1 = nullptr, *key_set2 = nullptr;
        Preset::get_extruder_names_and_keysets(collection.type(), extruder_id_name, extruder_variant_name,
                                               &key_set1, &key_set2);
        resolved_config = parent->config;
        extend_default_config_length(parsed_config, false, {});
        resolved_config.update_diff_values_to_child_config(
            parsed_config, extruder_id_name, extruder_variant_name, *key_set1, *key_set2);
        Preset::normalize(resolved_config);
    }

    const std::string new_name = unique_duplicate_copy_name(collection, issue.preset_name);
    set_recovered_identity(resolved_config, collection.type(), new_name);
    resolved_config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS, true)->value.clear();

    // A detached standalone copy: reuses collection.clone_presets()'s tested insert-and-save
    // path (also used by install_project_preset) rather than reimplementing it here.
    Preset source_preset(collection.type(), new_name, false);
    source_preset.config = std::move(resolved_config);

    std::vector<std::string> failures;
    try {
        const bool copied = collection.clone_presets(
            { &source_preset }, failures,
            [&new_name](Preset &copy, Preset::Type &) { copy.name = new_name; });
        if (! copied) {
            result.error = failures.empty() ? "OrcaSlicer could not create a preset from the duplicate file."
                                            : "A preset with that name already exists.";
            return result;
        }
    } catch (const std::exception &error) {
        result.error = error.what();
        return result;
    }

    // Mark the original file resolved so it stops colliding on the next load. It is renamed,
    // not deleted - its bytes are preserved, just moved out of the .json scan.
    const boost::filesystem::path resolved_path(find_unique_candidate(
        issue.file + ".duplicate-resolved", "-",
        [](const std::string &candidate) { return boost::filesystem::exists(candidate); }));
    // If this rename fails (e.g. a permissions issue), the original file is untouched and the
    // same duplicate-name issue simply gets reported again on the next load - safe, not silent.
    boost::system::error_code rename_error;
    boost::filesystem::rename(source_path, resolved_path, rename_error);
    if (! rename_error)
        collection.clear_load_issue(issue.file);

    result.succeeded = true;
    result.preset_name = new_name;
    return result;
}

std::string format_preset_inspection(const PresetInspection &inspection)
{
    std::ostringstream out;
    out << "Preset\n"
        << "  Name: " << inspection.name << '\n'
        << "  Type: " << inspection.type << '\n'
        << "  Identity: " << inspection.stable_identity << " (" << inspection.identity_source << ")\n"
        << "  Provenance: " << inspection.provenance << '\n';
    if (! inspection.vendor.empty())
        out << "  Vendor: " << inspection.vendor << '\n';
    if (! inspection.file.empty())
        out << "  File: " << inspection.file << '\n';
    out << "  Visibility: " << inspection.visibility_reason << '\n'
        << "  Compatibility: " << inspection.compatibility_reason << "\n\n";

    append_list(out, "Parent chain", inspection.parent_chain);
    if (! inspection.broken_parent.empty())
        out << "Broken parent: " << inspection.broken_parent << '\n';
    append_list(out, "Dependent presets", inspection.child_presets);
    append_list(out, "Unknown keys", inspection.unknown_keys);
    append_list(out, "Orphaned sidecars", inspection.orphaned_sidecars);

    out << "Preset load issues:\n";
    if (inspection.load_issues.empty()) {
        out << "  None\n";
    } else {
        for (const PresetLoadIssue &issue : inspection.load_issues) {
            out << "  - " << issue.preset_name << ": " << issue.reason;
            if (! issue.file.empty())
                out << " [" << issue.file << "]";
            out << '\n';
        }
    }

    out << "Effective values:\n";
    for (const PresetEffectiveValue &value : inspection.effective_values)
        out << "  " << value.key << " = " << value.value << "  [from " << value.source_preset << "]\n";
    return out.str();
}

std::string format_preset_recovery(const PresetInspection &inspection)
{
    std::ostringstream out;
    out << inspection.name << "\n\n";

    const bool project_only = inspection.provenance == "Project embedded";
    const bool has_problem = ! inspection.visible || ! inspection.compatible ||
                             ! inspection.broken_parent.empty() || ! inspection.unknown_keys.empty();
    out << "Status: " << (has_problem ? "Needs attention" :
                          project_only ? "Available in this project only" : "Available") << "\n\n";

    if (project_only)
        out << "Why it is temporary\nThese settings were loaded from the open project. They will not appear in a new project unless you install them as a user preset.\n\n";

    if (! inspection.visible)
        out << "Why it is hidden\n" << inspection.visibility_reason << "\n\n";
    if (! inspection.compatible)
        out << "Why it does not appear for the current printer\n"
            << (inspection.compatibility_reason.empty() ? "The preset is not compatible with the active printer."
                                                        : inspection.compatibility_reason)
            << "\n\n";
    if (! inspection.broken_parent.empty())
        out << "Missing base preset\nThis preset depends on '" << inspection.broken_parent
            << "', but OrcaSlicer could not load that preset.\n\n";
    if (! inspection.unknown_keys.empty()) {
        out << "Preset file warnings\nOrcaSlicer did not recognize: ";
        const size_t count = std::min<size_t>(inspection.unknown_keys.size(), 5);
        for (size_t index = 0; index < count; ++ index) {
            if (index > 0)
                out << ", ";
            out << inspection.unknown_keys[index];
        }
        if (inspection.unknown_keys.size() > count)
            out << ", and " << inspection.unknown_keys.size() - count << " more";
        out << ".\n\n";
    }

    out << "What to try\n";
    if (project_only)
        out << "- Choose Install for Future Projects to keep these resolved settings without depending on a preset from the source OrcaSlicer fork.\n";
    if (! inspection.broken_parent.empty())
        out << "- Restore or reinstall the missing base preset, then restart OrcaSlicer.\n";
    if (! inspection.compatible)
        out << "- Select the printer this preset was created for, or update the preset's Dependencies to include the current printer.\n";
    if (! inspection.visible)
        out << "- Check that the preset's printer vendor, model, and nozzle variant are enabled.\n";
    if (! inspection.unknown_keys.empty())
        out << "- Open and save the preset in the OrcaSlicer version that created it, then import that saved copy.\n";
    if (! has_problem && ! project_only)
        out << "No visibility or compatibility problem was detected for this preset.\n";

    out << "\nRelationship\n";
    if (inspection.parent_chain.empty())
        out << "Base preset: none\n";
    else
        out << "Base preset: " << inspection.parent_chain.front() << "\n";
    if (! inspection.child_presets.empty())
        out << "Presets based on this one: " << inspection.child_presets.size() << "\n";
    out << "Source: " << inspection.provenance << "\n";
    return out.str();
}

std::string format_preset_load_issue(const PresetLoadIssue &issue)
{
    std::ostringstream out;
    out << issue.preset_name << "\n\nStatus: Could not be loaded\n\n"
        << "Why\n" << issue.reason << "\n\n"
        << "What to try\n"
        << "- Restore the base printer or preset named in the error, if one is missing.\n"
        << "- Re-export the preset from OrcaSlicer instead of copying a system preset file.\n"
        << "- Keep the original file as a backup before making changes.\n";
    if (issue.reason.rfind("Parent preset ", 0) == 0)
        out << "\nWhy there is no automatic repair\n"
            << "This file stores only changes from its missing base preset. OrcaSlicer cannot create "
               "the missing settings without guessing, so it leaves the file unchanged.\n";
    if (! issue.file.empty())
        out << "\nFile\n" << issue.file << "\n";
    return out.str();
}

} // namespace Slic3r
