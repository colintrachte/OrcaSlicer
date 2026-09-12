#pragma once

#include "Preset.hpp"

#include <string>
#include <vector>

namespace Slic3r {

class PresetBundle;

struct PresetEffectiveValue
{
    std::string key;
    std::string value;
    std::string source_preset;
};

struct PresetSimilarity
{
    std::string target_name;
    float       similarity_percent { 0.0f };
    size_t      identical_options_count { 0 };
    size_t      differing_options_count { 0 };
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> differences;
    std::string last_modified_time;
};

struct PresetInspection
{
    std::string name;
    std::string type;
    std::string stable_identity;
    std::string identity_source;
    std::string provenance;
    std::string vendor;
    std::string file;
    std::string file_last_modified;

    bool        visible { true };
    std::string visibility_reason;
    bool        compatible { true };
    std::string compatibility_reason;

    std::vector<std::string> parent_chain;
    std::string              broken_parent;
    std::vector<std::string> child_presets;
    std::vector<std::string> unknown_keys;
    std::vector<std::string> orphaned_sidecars;
    std::vector<PresetLoadIssue> load_issues;
    std::vector<PresetEffectiveValue> effective_values;
    std::vector<PresetSimilarity> similar_presets;
};

struct PresetRecoveryCopyOptions
{
    std::string compatible_printer;
    std::string compatible_print;
};

struct PresetRecoveryActionResult
{
    bool        succeeded { false };
    std::string preset_name;
    std::string backup_file;
    std::string error;
};

PresetInspection inspect_preset(
    const PresetCollection          &collection,
    const Preset                    &preset,
    const PresetWithVendorProfile   *active_printer = nullptr,
    const PresetWithVendorProfile   *active_print = nullptr);

bool preset_needs_recovery(const PresetCollection &collection, const Preset &preset);

// Preserve the original preset and create a visible, standalone user copy from settings that
// OrcaSlicer has already resolved in memory. Compatibility is narrowed only when the caller
// explicitly supplies the active printer/process chosen by the user.
PresetRecoveryActionResult create_standalone_recovery_copy(
    PresetCollection                  &collection,
    const Preset                      &source,
    const PresetRecoveryCopyOptions   &options = {});

// Suggest the reusable identity stored inside a project-loaded preset. External 3MF loading adds
// "(filename.3mf)" to avoid collisions while the project is open; installation should not force
// that transport suffix into the user's preset name.
std::string project_preset_install_name(const Preset &source);

// Promote a temporary project preset to a persistent, standalone user preset. The effective
// settings and source version are retained; the installed copy has no dependency on fork-specific
// parents that may not exist in this OrcaSlicer installation.
PresetRecoveryActionResult install_project_preset(
    PresetCollection &collection, const Preset &source, const std::string &installed_name);

// Replace a same-named writable user preset with the project's resolved standalone settings.
// The existing file is backed up first and restored if the replacement itself fails.
PresetRecoveryActionResult replace_user_preset_from_project(
    PresetCollection &collection, const Preset &source, const std::string &installed_name);

// Rename a writable user preset without leaving a duplicate. Direct children and, when the
// bundle is supplied, cross-type compatibility/default references and physical-printer links are
// rewritten. Every changed on-disk preset is backed up before modification.
PresetRecoveryActionResult rename_user_preset(
    PresetCollection &collection, const std::string &old_name, const std::string &new_name,
    PresetBundle *bundle = nullptr);

std::string missing_parent_from_load_issue(const PresetLoadIssue &issue);

// Return a compact, relevance-ranked set of loaded presets that the user may explicitly choose
// as the missing base. No candidate is selected automatically.
std::vector<std::string> recovery_parent_candidates(
    const PresetCollection &collection, const std::string &missing_parent, size_t maximum = 12);

// Repair a rejected diff-only child against the base explicitly chosen by the user. Only the
// relationship in the existing source JSON is changed; an untouched backup is retained beside it.
PresetRecoveryActionResult reparent_rejected_preset(
    PresetCollection &collection, const PresetLoadIssue &issue, const std::string &parent_name);

// True if a load issue is a duplicate-canonical-name collision: a preset directory holds two
// files (typically one in the base/ subfolder, one directly in the type folder) that resolve to
// the same preset name, so the loader kept the first and silently skipped the second.
bool is_duplicate_name_load_issue(const PresetLoadIssue &issue);

// Load the shadowed duplicate's content as a new, distinctly-named standalone preset so nothing
// is lost, then mark the original file as resolved (renamed with a non-.json suffix, bytes
// preserved) so it stops being rescanned as a conflicting duplicate on the next load.
PresetRecoveryActionResult resolve_duplicate_preset(PresetCollection &collection, const PresetLoadIssue &issue);

// Return all loaded base/system presets that can serve as a parent for an orphan or reparenting.
std::vector<std::string> all_eligible_parent_candidates(const PresetCollection &collection);

// Finds presets in the same collection with high similarity (above min_similarity_percent).
std::vector<PresetSimilarity> find_similar_presets(
    const PresetCollection &collection, const Preset &preset, float min_similarity_percent = 65.0f);

// Consolidates two presets: winner settings override loser, or winner replaces loser with backup.
PresetRecoveryActionResult consolidate_presets(
    PresetCollection &collection, const std::string &winner_name, const std::string &loser_name, bool delete_loser = false);

// Export a preset's fully resolved configuration to a PrusaSlicer-compatible .ini file.
PresetRecoveryActionResult export_preset_to_prusa_ini(
    const PresetCollection &collection, const Preset &preset, const std::string &target_file_path);

std::string format_preset_inspection(const PresetInspection &inspection);
std::string format_preset_recovery(const PresetInspection &inspection);
std::string format_preset_load_issue(const PresetLoadIssue &issue);

} // namespace Slic3r

