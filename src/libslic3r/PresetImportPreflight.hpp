#pragma once

#include "Preset.hpp"

#include <string>
#include <vector>

namespace Slic3r {

class PresetBundle;

enum class PresetImportIssueKind
{
    InvalidFile,
    DuplicatePreset,
    MissingParent,
    AmbiguousParent,
    InheritanceCycle,
    MissingCompatibilityReference,
    ProtectedPresetConflict
};

struct PresetImportCandidate
{
    std::string  file;
    std::string  name;
    Preset::Type type { Preset::TYPE_INVALID };
    std::string  parent;
    std::string  version;
    bool         replaces_user_preset { false };
};

struct PresetImportIssue
{
    PresetImportIssueKind kind;
    std::string           file;
    std::string           preset_name;
    Preset::Type          preset_type { Preset::TYPE_INVALID };
    std::string           dependency;
    std::string           reason;
};

struct PresetImportPreflight
{
    std::vector<PresetImportCandidate> candidates;
    std::vector<std::string>       ordered_files;
    std::vector<PresetImportIssue> issues;
    std::vector<PresetImportIssue> warnings;

    bool can_import() const { return issues.empty(); }
};

struct PresetMigrationSource
{
    std::string label;
    std::string folder;
};

PresetImportPreflight preflight_preset_imports(
    const PresetBundle                         &bundle,
    const std::vector<std::string>              &files,
    ForwardCompatibilitySubstitutionRule         substitution_rule,
    bool                                         inspect_installed_name_conflicts = true);

PresetImportPreflight preflight_preset_archive(
    const PresetBundle                         &bundle,
    const std::string                           &archive_file,
    ForwardCompatibilitySubstitutionRule         substitution_rule);

// Finds user preset JSON files in an OrcaSlicer data/backup tree or in a folder
// containing copied preset files. Vendor/system and runtime folders are excluded.
std::vector<std::string> discover_preset_migration_files(const std::string &root_folder);
std::vector<PresetMigrationSource> discover_preset_migration_sources(const std::string &root_folder);

std::string format_preset_import_preflight(const PresetImportPreflight &preflight);
std::string format_preset_import_review(const PresetImportPreflight &preflight);
std::string format_preset_import_warnings(const std::vector<PresetImportIssue> &warnings);

} // namespace Slic3r
