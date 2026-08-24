#include "PresetImportPreflight.hpp"

#include "PresetBundle.hpp"
#include "Utils.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/nowide/cstdio.hpp>
#include <miniz/miniz.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace Slic3r {

namespace {

struct ImportCandidate : PresetImportCandidate
{
    std::vector<std::string> compatible_printers;
    std::vector<std::string> compatible_prints;
};

std::string candidate_key(Preset::Type type, const std::string &name)
{
    return std::to_string(static_cast<int>(type)) + "\n" + name;
}

const PresetCollection *collection_for(const PresetBundle &bundle, Preset::Type type)
{
    switch (type) {
    case Preset::TYPE_PRINT:    return &bundle.prints;
    case Preset::TYPE_FILAMENT: return &bundle.filaments;
    case Preset::TYPE_PRINTER:  return &bundle.printers;
    default:                    return nullptr;
    }
}

Preset::Type preset_type(const DynamicPrintConfig &config)
{
    if (config.has("printer_settings_id"))
        return Preset::TYPE_PRINTER;
    if (config.has("print_settings_id"))
        return Preset::TYPE_PRINT;
    if (config.has("filament_settings_id"))
        return Preset::TYPE_FILAMENT;
    return Preset::TYPE_INVALID;
}

void add_issue(PresetImportPreflight &result, PresetImportIssueKind kind,
               const ImportCandidate &candidate, std::string dependency, std::string reason)
{
    result.issues.push_back({ kind, candidate.file, candidate.name, candidate.type,
                              std::move(dependency), std::move(reason) });
}

void add_warning(PresetImportPreflight &result, const ImportCandidate &candidate,
                 std::string dependency, std::string reason)
{
    result.warnings.push_back({ PresetImportIssueKind::MissingCompatibilityReference,
                                candidate.file, candidate.name, candidate.type,
                                std::move(dependency), std::move(reason) });
}

} // namespace

std::vector<std::string> discover_preset_migration_files(const std::string &root_folder)
{
    namespace fs = boost::filesystem;

    const fs::path root(root_folder);
    if (! fs::exists(root) || ! fs::is_directory(root))
        throw fs::filesystem_error("Preset migration source is not a folder", root,
                                   boost::system::errc::make_error_code(boost::system::errc::not_a_directory));

    const auto is_excluded_directory = [](const fs::path &path) {
        const std::string name = boost::algorithm::to_lower_copy(path.filename().string());
        return name == "system" || name == "ota" || name == "log" || name == "printers" ||
               name == "temp" || name == ".git";
    };
    const auto is_preset_category = [](const fs::path &path) {
        const std::string name = boost::algorithm::to_lower_copy(path.filename().string());
        return name == PRESET_PRINTER_NAME || name == PRESET_PRINT_NAME ||
               name == PRESET_FILAMENT_NAME || name == PRESET_SLA_PRINT_NAME ||
               name == PRESET_SLA_MATERIALS_NAME;
    };

    std::vector<std::string> files;
    for (fs::recursive_directory_iterator iterator(root), end; iterator != end; ++ iterator) {
        const fs::path path = iterator->path();
        if (fs::is_directory(path)) {
            if (is_excluded_directory(path))
                iterator.no_push();
            continue;
        }
        if (! fs::is_regular_file(path) || ! boost::algorithm::iends_with(path.filename().string(), ".json"))
            continue;

        bool in_preset_category = false;
        for (fs::path parent = path.parent_path(); ! parent.empty(); parent = parent.parent_path()) {
            if (is_preset_category(parent)) {
                in_preset_category = true;
                break;
            }
            if (parent == root)
                break;
        }
        // A directly selected folder of copied JSON presets has no category directory.
        if (in_preset_category || path.parent_path() == root)
            files.push_back(path.string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<PresetMigrationSource> discover_preset_migration_sources(const std::string &root_folder)
{
    namespace fs = boost::filesystem;

    const fs::path root(root_folder);
    if (! fs::exists(root) || ! fs::is_directory(root))
        throw fs::filesystem_error("Preset migration source is not a folder", root,
                                   boost::system::errc::make_error_code(boost::system::errc::not_a_directory));

    std::vector<PresetMigrationSource> sources;
    const auto add_if_present = [&sources](const std::string &label, const fs::path &folder) {
        if (fs::is_directory(folder) && ! discover_preset_migration_files(folder.string()).empty())
            sources.push_back({ label, folder.string() });
    };

    add_if_present("User settings", root / PRESET_USER_DIR);
    for (fs::directory_iterator iterator(root), end; iterator != end; ++ iterator) {
        if (! fs::is_directory(iterator->path()))
            continue;
        const std::string name = iterator->path().filename().string();
        if (boost::algorithm::istarts_with(name, "user_backup-"))
            add_if_present("Backup " + name.substr(std::string("user_backup-").size()), iterator->path());
    }

    const fs::path versions = root / "versions";
    if (fs::is_directory(versions)) {
        for (fs::directory_iterator iterator(versions), end; iterator != end; ++ iterator) {
            if (! fs::is_directory(iterator->path()))
                continue;
            const std::string version = iterator->path().filename().string();
            add_if_present("Version " + version, iterator->path() / PRESET_USER_DIR);
            for (fs::directory_iterator child(iterator->path()), child_end; child != child_end; ++ child) {
                if (! fs::is_directory(child->path()))
                    continue;
                const std::string name = child->path().filename().string();
                if (boost::algorithm::istarts_with(name, "user_backup-"))
                    add_if_present("Version " + version + " backup " + name.substr(std::string("user_backup-").size()), child->path());
            }
        }
    }

    if (sources.empty() && ! discover_preset_migration_files(root.string()).empty())
        sources.push_back({ "Selected folder", root.string() });
    std::sort(sources.begin(), sources.end(), [](const PresetMigrationSource &left, const PresetMigrationSource &right) {
        return left.label > right.label;
    });
    return sources;
}

PresetImportPreflight preflight_preset_archive(
    const PresetBundle                    &bundle,
    const std::string                     &archive_file,
    ForwardCompatibilitySubstitutionRule    substitution_rule)
{
    namespace fs = boost::filesystem;

    PresetImportPreflight result;
    const fs::path temp_dir = fs::temp_directory_path() / fs::unique_path("orcaslicer-import-%%%%-%%%%-%%%%");
    boost::system::error_code ec;
    fs::create_directories(temp_dir, ec);
    if (ec) {
        result.issues.push_back({ PresetImportIssueKind::InvalidFile, archive_file, {}, Preset::TYPE_INVALID, {},
                                  "A temporary folder could not be created: " + ec.message() });
        return result;
    }
    ScopeGuard remove_temp([&temp_dir]() {
        boost::system::error_code remove_error;
        boost::filesystem::remove_all(temp_dir, remove_error);
    });

    FILE *zip_file = boost::nowide::fopen(archive_file.c_str(), "rb");
    if (zip_file == nullptr) {
        result.issues.push_back({ PresetImportIssueKind::InvalidFile, archive_file, {}, Preset::TYPE_INVALID, {},
                                  "The archive could not be opened." });
        return result;
    }
    ScopeGuard close_file([zip_file]() { fclose(zip_file); });

    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    if (! mz_zip_reader_init_cfile(&archive, zip_file, 0, MZ_ZIP_FLAG_CASE_SENSITIVE | MZ_ZIP_FLAG_IGNORE_PATH)) {
        result.issues.push_back({ PresetImportIssueKind::InvalidFile, archive_file, {}, Preset::TYPE_INVALID, {},
                                  "The file is not a readable preset archive." });
        return result;
    }
    ScopeGuard close_archive([&archive]() { mz_zip_reader_end(&archive); });

    std::vector<std::string>                     extracted_files;
    std::unordered_map<std::string, std::string> entry_by_file;
    bool                                         has_bundle_structure = false;
    const int num_files = mz_zip_reader_get_num_files(&archive);
    for (int index = 0; index < num_files; ++ index) {
        mz_zip_archive_file_stat file_stat;
        if (! mz_zip_reader_file_stat(&archive, index, &file_stat)) {
            result.issues.push_back({ PresetImportIssueKind::InvalidFile, archive_file, {}, Preset::TYPE_INVALID, {},
                                      "An archive entry could not be read." });
            return result;
        }

        const fs::path entry_path(file_stat.m_filename);
        const std::string entry_name = entry_path.filename().string();
        if (entry_name == BUNDLE_STRUCTURE_JSON_NAME) {
            has_bundle_structure = true;
            continue;
        }
        if (entry_name.empty() || entry_name == PRESET_BUNDLE_METADATA ||
            ! boost::iends_with(entry_name, ".json"))
            continue;

        const fs::path target = temp_dir / (std::to_string(index) + ".json");
        if (! mz_zip_reader_extract_to_file(&archive, index, encode_path(target.string().c_str()).c_str(), MZ_ZIP_FLAG_CASE_SENSITIVE)) {
            result.issues.push_back({ PresetImportIssueKind::InvalidFile,
                                      archive_file + "::" + file_stat.m_filename, {}, Preset::TYPE_INVALID, {},
                                      "This preset could not be extracted from the archive." });
            return result;
        }
        const std::string target_file = target.string();
        extracted_files.push_back(target_file);
        entry_by_file.emplace(target_file, file_stat.m_filename);
    }

    if (extracted_files.empty()) {
        result.issues.push_back({ PresetImportIssueKind::InvalidFile, archive_file, {}, Preset::TYPE_INVALID, {},
                                  "The archive does not contain any preset JSON files." });
        return result;
    }

    result = preflight_preset_imports(bundle, extracted_files, substitution_rule, ! has_bundle_structure);
    for (std::string &file : result.ordered_files)
        file = entry_by_file.at(file);
    for (PresetImportCandidate &candidate : result.candidates) {
        const auto entry = entry_by_file.find(candidate.file);
        if (entry != entry_by_file.end())
            candidate.file = archive_file + "::" + entry->second;
    }
    for (PresetImportIssue &issue : result.issues) {
        const auto entry = entry_by_file.find(issue.file);
        if (entry != entry_by_file.end())
            issue.file = archive_file + "::" + entry->second;
    }
    for (PresetImportIssue &warning : result.warnings) {
        const auto entry = entry_by_file.find(warning.file);
        if (entry != entry_by_file.end())
            warning.file = archive_file + "::" + entry->second;
    }
    return result;
}

PresetImportPreflight preflight_preset_imports(
    const PresetBundle                    &bundle,
    const std::vector<std::string>         &files,
    ForwardCompatibilitySubstitutionRule    substitution_rule,
    bool                                    inspect_installed_name_conflicts)
{
    PresetImportPreflight       result;
    std::vector<ImportCandidate> candidates;
    candidates.reserve(files.size());

    for (const std::string &file : files) {
        ImportCandidate candidate;
        candidate.file = file;
        try {
            DynamicPrintConfig config;
            std::map<std::string, std::string> headers;
            std::string reason;
            config.load_from_json(file, substitution_rule, headers, reason);
            candidate.name = headers[BBL_JSON_KEY_NAME];
            candidate.version = headers[BBL_JSON_KEY_VERSION];
            candidate.type = preset_type(config);
            if (const auto *inherits = config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS))
                candidate.parent = inherits->value;
            if (const auto *compatible = config.option<ConfigOptionStrings>("compatible_printers"))
                candidate.compatible_printers = compatible->values;
            if (const auto *compatible = config.option<ConfigOptionStrings>("compatible_prints"))
                candidate.compatible_prints = compatible->values;

            if (! reason.empty() || candidate.name.empty() || candidate.type == Preset::TYPE_INVALID) {
                const std::string detail = ! reason.empty() ? reason :
                    (candidate.name.empty() ? "The preset has no name." : "The preset type could not be determined.");
                add_issue(result, PresetImportIssueKind::InvalidFile, candidate, {}, detail);
            }
            if (inspect_installed_name_conflicts && ! candidate.name.empty() && candidate.type != Preset::TYPE_INVALID) {
                const PresetCollection *collection = collection_for(bundle, candidate.type);
                if (collection != nullptr) {
                    if (const Preset *installed = collection->find_preset(candidate.name, false)) {
                        if (installed->is_default || installed->is_system) {
                            add_issue(result, PresetImportIssueKind::ProtectedPresetConflict, candidate, candidate.name,
                                      "An installed system preset has this name and cannot be replaced. Rename the imported preset in its source application first.");
                        } else {
                            candidate.replaces_user_preset = true;
                        }
                    }
                }
            }
        } catch (const std::exception &error) {
            add_issue(result, PresetImportIssueKind::InvalidFile, candidate, {}, error.what());
        }
        result.candidates.push_back(candidate);
        candidates.push_back(std::move(candidate));
    }

    std::map<std::string, std::vector<size_t>> included;
    for (size_t index = 0; index < candidates.size(); ++ index)
        if (! candidates[index].name.empty() && candidates[index].type != Preset::TYPE_INVALID)
            included[candidate_key(candidates[index].type, candidates[index].name)].push_back(index);

    std::vector<std::vector<size_t>> children(candidates.size());
    std::vector<size_t>              parent_count(candidates.size(), 0);
    std::vector<bool>                blocked(candidates.size(), false);

    for (const auto &[key, matches] : included) {
        if (matches.size() < 2)
            continue;
        for (size_t index : matches) {
            blocked[index] = true;
            add_issue(result, PresetImportIssueKind::DuplicatePreset, candidates[index], candidates[index].name,
                      "More than one selected file defines this preset.");
        }
    }

    for (size_t index = 0; index < candidates.size(); ++ index) {
        const ImportCandidate &candidate = candidates[index];
        if (candidate.name.empty() || candidate.type == Preset::TYPE_INVALID || candidate.parent.empty() || blocked[index])
            continue;

        const auto included_parent = included.find(candidate_key(candidate.type, candidate.parent));
        if (included_parent != included.end()) {
            if (included_parent->second.size() == 1) {
                const size_t parent_index = included_parent->second.front();
                children[parent_index].push_back(index);
                ++ parent_count[index];
            } else {
                blocked[index] = true;
                add_issue(result, PresetImportIssueKind::AmbiguousParent, candidate, candidate.parent,
                          "Several selected files could be this preset's parent.");
            }
            continue;
        }

        const PresetCollection *collection = collection_for(bundle, candidate.type);
        if (collection == nullptr || collection->find_preset2(candidate.parent, true) == nullptr) {
            blocked[index] = true;
            add_issue(result, PresetImportIssueKind::MissingParent, candidate, candidate.parent,
                      "The required parent is neither installed nor included in the import.");
        }
    }

    auto has_target = [&](Preset::Type type, const std::string &name) {
        if (name.empty())
            return true;
        if (included.find(candidate_key(type, name)) != included.end())
            return true;
        const PresetCollection *collection = collection_for(bundle, type);
        return collection != nullptr && collection->find_preset(name, false) != nullptr;
    };

    for (const ImportCandidate &candidate : candidates) {
        if (candidate.name.empty() || candidate.type == Preset::TYPE_INVALID)
            continue;
        for (const std::string &printer : candidate.compatible_printers) {
            if (! has_target(Preset::TYPE_PRINTER, printer))
                add_warning(result, candidate, printer,
                            "Compatible printer is not installed or included. The preset may remain hidden until that printer is available.");
        }
        if (candidate.type == Preset::TYPE_FILAMENT) {
            for (const std::string &process : candidate.compatible_prints) {
                if (! has_target(Preset::TYPE_PRINT, process))
                    add_warning(result, candidate, process,
                                "Compatible process is not installed or included. The preset may remain hidden until that process is available.");
            }
        }
    }

    // Nodes already blocked for a distinct, already-reported reason (duplicate name, ambiguous
    // or missing parent) are never dequeued below, so their descendants' parent_count never
    // reaches zero either. Without accounting for that, those descendants would wrongly look
    // like cycle participants. Walk the dependency graph forward from every such node first so
    // its descendants can be told apart from genuine cycle members.
    std::vector<bool> blocked_by_dependency(candidates.size(), false);
    {
        std::vector<size_t> frontier;
        for (size_t index = 0; index < candidates.size(); ++ index)
            if (blocked[index])
                frontier.push_back(index);
        for (size_t cursor = 0; cursor < frontier.size(); ++ cursor) {
            for (size_t child : children[frontier[cursor]]) {
                if (! blocked[child] && ! blocked_by_dependency[child]) {
                    blocked_by_dependency[child] = true;
                    frontier.push_back(child);
                }
            }
        }
    }

    std::set<std::pair<std::string, size_t>> ready;
    for (size_t index = 0; index < candidates.size(); ++ index)
        if (! blocked[index] && parent_count[index] == 0)
            ready.emplace(candidate_key(candidates[index].type, candidates[index].name) + "\n" + candidates[index].file, index);

    std::vector<bool> emitted(candidates.size(), false);
    while (! ready.empty()) {
        const size_t index = ready.begin()->second;
        ready.erase(ready.begin());
        emitted[index] = true;
        result.ordered_files.push_back(candidates[index].file);
        for (size_t child : children[index]) {
            if (-- parent_count[child] == 0 && ! blocked[child])
                ready.emplace(candidate_key(candidates[child].type, candidates[child].name) + "\n" + candidates[child].file, child);
        }
    }

    for (size_t index = 0; index < candidates.size(); ++ index) {
        if (! blocked[index] && ! emitted[index]) {
            blocked[index] = true;
            if (blocked_by_dependency[index])
                // Its own dependency chain already carries a reported MissingParent/AmbiguousParent/
                // DuplicatePreset issue further up; that's the actionable cause, not a cycle.
                continue;
            add_issue(result, PresetImportIssueKind::InheritanceCycle, candidates[index], candidates[index].parent,
                      "The selected presets contain an inheritance cycle.");
        }
    }

    if (! result.issues.empty())
        result.ordered_files.clear();
    return result;
}

std::string format_preset_import_preflight(const PresetImportPreflight &preflight)
{
    if (preflight.issues.empty())
        return "The selected presets are ready to import.";

    std::ostringstream out;
    out << "OrcaSlicer found preset dependencies that need attention before import:\n\n";
    for (const PresetImportIssue &issue : preflight.issues) {
        out << "- " << (issue.preset_name.empty() ? boost::filesystem::path(issue.file).filename().string()
                                                    : issue.preset_name);
        if (! issue.dependency.empty())
            out << " -> " << issue.dependency;
        out << "\n  " << issue.reason << "\n";
    }
    out << "\nNo presets were imported.";
    return out.str();
}

std::string format_preset_import_review(const PresetImportPreflight &preflight)
{
    size_t printers = 0;
    size_t processes = 0;
    size_t filaments = 0;
    size_t replacements = 0;
    for (const PresetImportCandidate &candidate : preflight.candidates) {
        printers += candidate.type == Preset::TYPE_PRINTER;
        processes += candidate.type == Preset::TYPE_PRINT;
        filaments += candidate.type == Preset::TYPE_FILAMENT;
        replacements += candidate.replaces_user_preset;
    }

    std::ostringstream out;
    out << "Ready to migrate " << preflight.candidates.size() << " presets\n\n"
        << "Printer: " << printers << "    Process: " << processes << "    Filament: " << filaments << "\n";
    if (replacements > 0)
        out << "Existing user presets with the same name: " << replacements
            << " (you will choose Keep or Replace; replacements are backed up)\n";
    if (! preflight.warnings.empty())
        out << "Visibility links needing attention: " << preflight.warnings.size() << " (these do not block migration)\n";

    out << "\nPresets\n";
    const size_t visible_count = std::min<size_t>(preflight.candidates.size(), 12);
    for (size_t index = 0; index < visible_count; ++ index) {
        const PresetImportCandidate &candidate = preflight.candidates[index];
        const char *type = candidate.type == Preset::TYPE_PRINTER ? "Printer" :
                           candidate.type == Preset::TYPE_PRINT ? "Process" :
                           candidate.type == Preset::TYPE_FILAMENT ? "Filament" : "Unknown";
        out << "- " << type << ": " << candidate.name;
        if (! candidate.version.empty())
            out << "  [" << candidate.version << "]";
        if (! candidate.parent.empty())
            out << "  <- " << candidate.parent;
        if (candidate.replaces_user_preset)
            out << "  (name already exists)";
        out << '\n';
    }
    if (preflight.candidates.size() > visible_count)
        out << "- " << preflight.candidates.size() - visible_count << " more\n";
    out << "\nParents are installed before children. Continue to the conflict choices?";
    return out.str();
}

std::string format_preset_import_warnings(const std::vector<PresetImportIssue> &warnings)
{
    if (warnings.empty())
        return {};

    std::ostringstream out;
    out << "Compatibility notice:\n";
    const size_t visible_count = std::min<size_t>(warnings.size(), 5);
    for (size_t index = 0; index < visible_count; ++ index) {
        const PresetImportIssue &warning = warnings[index];
        out << "\n- " << (warning.preset_name.empty() ? boost::filesystem::path(warning.file).filename().string()
                                                        : warning.preset_name);
        if (! warning.dependency.empty())
            out << " -> " << warning.dependency;
        out << "\n  " << warning.reason;
    }
    if (warnings.size() > visible_count)
        out << "\n\n- " << (warnings.size() - visible_count) << " more compatibility references need attention.";
    out << "\n\nThese references did not block the import.";
    return out.str();
}

} // namespace Slic3r
