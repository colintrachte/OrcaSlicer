#include <catch2/catch_all.hpp>

#include <boost/filesystem.hpp>
#include <miniz/miniz.h>
#include <nlohmann/json.hpp>
#include <fstream>

#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PresetInspector.hpp"
#include "libslic3r/PresetImportPreflight.hpp"
#include "libslic3r/AppConfig.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include "test_utils.hpp"

using namespace Slic3r;

namespace {

namespace fs = boost::filesystem;

struct TempPresetDir {
    fs::path path;

    TempPresetDir()
    {
        path = fs::temp_directory_path() / fs::unique_path("orcaslicer-preset-%%%%-%%%%-%%%%");
        fs::create_directories(path);
    }

    ~TempPresetDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_print_preset(const DynamicPrintConfig &default_config, const fs::path &file, const std::string &name, const std::string &inherits = {})
{
    DynamicPrintConfig config(default_config);
    config.option<ConfigOptionString>("print_settings_id", true)->value = name;
    config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS, true)->value = inherits;

    fs::create_directories(file.parent_path());
    config.save_to_json(file.string(), name, "User", "1.0.0");
}

// Write a preset json carrying a name and an "inherits" value, using the given collection's
// default config so it loads back into that collection. Works for any preset type.
void write_preset_with_inherits(const DynamicPrintConfig &default_config, const fs::path &file,
                                const std::string &name, const std::string &inherits)
{
    DynamicPrintConfig config(default_config);
    config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS, true)->value = inherits;

    fs::create_directories(file.parent_path());
    config.save_to_json(file.string(), name, "User", "1.0.0");
}

void write_print_preset_diff(const fs::path &file, const std::string &name, const std::string &inherits,
                             double layer_height)
{
    DynamicPrintConfig config;
    config.set_key_value(BBL_JSON_KEY_INHERITS, new ConfigOptionString(inherits));
    config.set_key_value("layer_height", new ConfigOptionFloat(layer_height));

    fs::create_directories(file.parent_path());
    config.save_to_json(file.string(), name, "User", "1.0.0");
}

void write_preset_archive(const fs::path &archive_file,
                          const std::vector<std::pair<std::string, fs::path>> &entries)
{
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    if (! mz_zip_writer_init_file(&archive, archive_file.string().c_str(), 0))
        throw std::runtime_error("Could not create test preset archive");

    bool succeeded = true;
    for (const auto &[entry_name, source_file] : entries)
        succeeded = succeeded && mz_zip_writer_add_file(&archive, entry_name.c_str(), source_file.string().c_str(),
                                                        nullptr, 0, MZ_DEFAULT_COMPRESSION);
    succeeded = succeeded && mz_zip_writer_finalize_archive(&archive);
    mz_zip_writer_end(&archive);
    if (! succeeded)
        throw std::runtime_error("Could not populate test preset archive");
}

// Add an in-memory preset (no file) with the given inherits value (empty => root preset).
Preset &add_inmemory_preset(PresetCollection &coll, const std::string &name, const std::string &inherits = {})
{
    DynamicPrintConfig config(coll.default_preset().config);
    config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS, true)->value = inherits;
    return coll.load_preset(std::string(), name, config, /*select=*/false);
}

// Mark an already-loaded preset as renamed from one or more former names.
void set_renamed_from(PresetCollection &coll, const std::string &preset_name, std::vector<std::string> old_names)
{
    for (auto it = coll.begin(); it != coll.end(); ++it)
        if (it->name == preset_name)
            it->renamed_from = std::move(old_names);
}

// A standalone print preset collection that exposes the protected rename-map builder, so a
// renamed_from scenario can be set up without the full system-profile load pipeline.
// (PresetCollection is non-copyable - it holds a mutex - so it is constructed directly with
// the same type/keys/defaults PresetBundle uses for its print collection.)
struct RenameTestCollection : public PresetCollection
{
    RenameTestCollection()
        : PresetCollection(Preset::TYPE_PRINT, Preset::print_options(),
                           static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()))
    {}
    using PresetCollection::update_map_system_profile_renamed;
};

} // namespace

TEST_CASE("Preset inspector explains identity lineage effective values and dependents", "[Preset][Inspector]")
{
    PresetBundle bundle;
    Preset      &parent = add_inmemory_preset(bundle.prints, "System Parent");
    parent.is_system = true;
    parent.setting_id = "system-parent-id";
    parent.config.option<ConfigOptionInt>("wall_loops")->value = 7;

    Preset &child = add_inmemory_preset(bundle.prints, "User Child", parent.name);
    child.config = parent.config;
    child.inherits() = parent.name;
    child.config.option<ConfigOptionFloat>("layer_height")->value = 0.16;
    child.setting_id = "user-child-id";

    Preset &grandchild = add_inmemory_preset(bundle.prints, "Dependent Child", child.name);
    grandchild.config = child.config;
    grandchild.inherits() = child.name;

    const PresetInspection inspection = inspect_preset(bundle.prints, child);
    CHECK(inspection.stable_identity == "user-child-id");
    CHECK(inspection.identity_source == "setting_id");
    CHECK(inspection.provenance == "User");
    CHECK(inspection.parent_chain == std::vector<std::string>{ "System Parent" });
    CHECK(inspection.broken_parent.empty());
    CHECK(inspection.child_presets == std::vector<std::string>{ "Dependent Child" });

    const auto wall_loops = std::find_if(inspection.effective_values.begin(), inspection.effective_values.end(),
        [](const PresetEffectiveValue &value) { return value.key == "wall_loops"; });
    const auto layer_height = std::find_if(inspection.effective_values.begin(), inspection.effective_values.end(),
        [](const PresetEffectiveValue &value) { return value.key == "layer_height"; });
    REQUIRE(wall_loops != inspection.effective_values.end());
    REQUIRE(layer_height != inspection.effective_values.end());
    CHECK(wall_loops->value == "7");
    CHECK(wall_loops->source_preset == "System Parent");
    CHECK(layer_height->value == "0.16");
    CHECK(layer_height->source_preset == "User Child");

    Preset &broken = add_inmemory_preset(bundle.prints, "Broken Child", "Missing Parent");
    const PresetInspection broken_inspection = inspect_preset(bundle.prints, broken);
    CHECK(broken_inspection.broken_parent == "Missing Parent");
    CHECK(preset_needs_recovery(bundle.prints, broken));

    parent.is_visible = false;
    CHECK_FALSE(preset_needs_recovery(bundle.prints, parent));

    child.is_compatible = false;
    CHECK(preset_needs_recovery(bundle.prints, child));

    Preset &healthy_recent = add_inmemory_preset(bundle.printers, "Healthy Recent Printer");
    healthy_recent.loading_unknown_keys = { "forward_compatible_option" };
    CHECK_FALSE(preset_needs_recovery(bundle.printers, healthy_recent));
}

TEST_CASE("Preset inspector reports incompatibility and file diagnostics without writing", "[Preset][Inspector][Compatibility]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;
    const fs::path preset_file = temp_dir.path / PRESET_PRINT_NAME / "Inspectable.json";
    const fs::path broken_file = temp_dir.path / PRESET_PRINT_NAME / "Broken.json";
    const fs::path orphan_info = temp_dir.path / PRESET_PRINT_NAME / "Removed.info";

    write_print_preset(bundle.prints.default_preset().config, preset_file, "Inspectable");
    write_print_preset(bundle.prints.default_preset().config, broken_file, "Broken", "Missing Parent");
    {
        std::ifstream input(preset_file.string());
        nlohmann::json json;
        input >> json;
        json["future_preset_option"] = "future-value";
        std::ofstream output(preset_file.string(), std::ios::trunc);
        output << json.dump(2);
    }
    {
        std::ofstream output(orphan_info.string());
        output << "{\"setting_id\":\"removed-id\"}";
    }

    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);
    Preset *candidate = bundle.prints.find_preset("Inspectable", false, true);
    REQUIRE(candidate != nullptr);
    candidate->is_visible = false;
    candidate->compatible_printers_condition() = "num_extruders == 1";

    Preset &printer = add_inmemory_preset(bundle.printers, "Two Tool Printer");
    printer.config.set_key_value("nozzle_diameter", new ConfigOptionFloats({ 0.4, 0.4 }));
    const PresetWithVendorProfile active_printer(printer, nullptr);

    std::ifstream before_stream(preset_file.string(), std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_stream)), std::istreambuf_iterator<char>());
    const PresetInspection inspection = inspect_preset(bundle.prints, *candidate, &active_printer);
    std::ifstream after_stream(preset_file.string(), std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(after_stream)), std::istreambuf_iterator<char>());

    CHECK_FALSE(inspection.visible);
    CHECK_FALSE(inspection.compatible);
    CHECK(inspection.compatibility_reason.find("compatible_printers_condition") != std::string::npos);
    CHECK(inspection.unknown_keys == std::vector<std::string>{ "future_preset_option" });
    CHECK(inspection.orphaned_sidecars == std::vector<std::string>{ orphan_info.string() });
    REQUIRE(inspection.load_issues.size() == 1);
    CHECK(inspection.load_issues.front().preset_name == "Broken");
    CHECK(inspection.load_issues.front().reason.find("Missing Parent") != std::string::npos);
    CHECK(before == after);
    CHECK(fs::exists(orphan_info));

    const std::string report = format_preset_inspection(inspection);
    CHECK(report.find("Identity:") != std::string::npos);
    CHECK(report.find("Broken parent:") == std::string::npos);
    CHECK(report.find("future_preset_option") != std::string::npos);
    CHECK(report.find("Missing Parent") != std::string::npos);

    const std::string recovery = format_preset_recovery(inspection);
    CHECK(recovery.find("Needs attention") != std::string::npos);
    CHECK(recovery.find("What to try") != std::string::npos);
    CHECK(recovery.find("current printer") != std::string::npos);
    CHECK(recovery.find("Effective values") == std::string::npos);

    const std::string rejected = format_preset_load_issue(inspection.load_issues.front());
    CHECK(rejected.find("Could not be loaded") != std::string::npos);
    CHECK(rejected.find("Missing Parent") != std::string::npos);
    CHECK(rejected.find("cannot create the missing settings without guessing") != std::string::npos);
}

TEST_CASE("Preset recovery creates a visible standalone copy and preserves its source", "[Preset][Inspector][Recovery]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;
    fs::create_directories(temp_dir.path / PRESET_PRINT_NAME);
    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);

    Preset &source = add_inmemory_preset(bundle.prints, "Lost Process", "Missing Parent");
    source.is_visible = false;
    source.config.option<ConfigOptionInt>("wall_loops")->value = 9;
    source.config.option<ConfigOptionStrings>("compatible_printers", true)->values = { "Old Printer" };

    const PresetRecoveryActionResult recovered = create_standalone_recovery_copy(
        bundle.prints, source, { "Current Printer", {} });
    REQUIRE(recovered.succeeded);
    CHECK(recovered.preset_name == "Lost Process (Recovered)");

    const Preset *original = bundle.prints.find_preset("Lost Process", false, true);
    const Preset *copy     = bundle.prints.find_preset(recovered.preset_name, false, true);
    REQUIRE(original != nullptr);
    REQUIRE(copy != nullptr);
    CHECK(original->inherits() == "Missing Parent");
    CHECK_FALSE(original->is_visible);
    CHECK(copy->inherits().empty());
    CHECK(copy->is_visible);
    CHECK(copy->config.option<ConfigOptionInt>("wall_loops")->value == 9);
    CHECK(copy->config.option<ConfigOptionStrings>("compatible_printers")->values ==
          std::vector<std::string>{ "Current Printer" });
    CHECK(fs::exists(copy->file));

    const PresetRecoveryActionResult second = create_standalone_recovery_copy(
        bundle.prints, *original, { "Current Printer", {} });
    REQUIRE(second.succeeded);
    CHECK(second.preset_name == "Lost Process (Recovered) 2");
}

TEST_CASE("Project presets install for future projects without retaining transport or parent dependencies",
          "[Preset][Inspector][Migration]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;
    fs::create_directories(temp_dir.path / PRESET_PRINT_NAME);
    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);

    Preset &source = add_inmemory_preset(
        bundle.prints, "PETG Process 0.25 nozzle(PETG Template.3mf)",
        "0.06mm Standard @Flashforge AD5M Pro 0.25 Nozzle");
    source.file = (temp_dir.path / "PETG Template.3mf").string();
    source.is_project_embedded = true;
    source.is_external = true;
    const Semver source_version = *Semver::parse("2.3.2");
    source.version = source_version;
    source.config.option<ConfigOptionFloat>("layer_height")->value = 0.10;
    source.config.option<ConfigOptionInt>("wall_loops")->value = 4;

    CHECK(project_preset_install_name(source) == "PETG Process 0.25 nozzle");
    const PresetRecoveryActionResult installed =
        install_project_preset(bundle.prints, source, project_preset_install_name(source));
    REQUIRE(installed.succeeded);
    CHECK(installed.preset_name == "PETG Process 0.25 nozzle");

    const Preset *persistent = bundle.prints.find_preset(installed.preset_name, false, true);
    REQUIRE(persistent != nullptr);
    CHECK_FALSE(persistent->is_project_embedded);
    CHECK_FALSE(persistent->is_external);
    CHECK(persistent->inherits().empty());
    CHECK(persistent->version == source_version);
    CHECK(persistent->config.option<ConfigOptionFloat>("layer_height")->value == Catch::Approx(0.10));
    CHECK(persistent->config.option<ConfigOptionInt>("wall_loops")->value == 4);
    CHECK(fs::exists(persistent->file));
    {
        DynamicPrintConfig persisted_config;
        std::map<std::string, std::string> headers;
        std::string reason;
        persisted_config.load_from_json(persistent->file, ForwardCompatibilitySubstitutionRule::Disable, headers, reason);
        CHECK(reason.empty());
        CHECK(headers[BBL_JSON_KEY_VERSION] == source_version.to_string());
        CHECK(persisted_config.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value.empty());
    }

    const Preset *unchanged_source = bundle.prints.find_preset(
        "PETG Process 0.25 nozzle(PETG Template.3mf)", false, true);
    REQUIRE(unchanged_source != nullptr);
    CHECK(unchanged_source->is_project_embedded);
    CHECK(unchanged_source->inherits() == "0.06mm Standard @Flashforge AD5M Pro 0.25 Nozzle");
    const PresetRecoveryActionResult duplicate =
        install_project_preset(bundle.prints, *unchanged_source, installed.preset_name);
    CHECK_FALSE(duplicate.succeeded);
    CHECK(duplicate.error.find("already installed") != std::string::npos);

    Preset *existing = bundle.prints.find_preset(installed.preset_name, false, true);
    REQUIRE(existing != nullptr);
    existing->inherits() = "Old unavailable parent";
    existing->version = Semver(1, 0, 0);
    existing->config.option<ConfigOptionInt>("wall_loops")->value = 2;
    existing->save(nullptr);

    const PresetRecoveryActionResult replaced =
        replace_user_preset_from_project(bundle.prints, *unchanged_source, installed.preset_name);
    REQUIRE(replaced.succeeded);
    CHECK(replaced.preset_name == installed.preset_name);
    CHECK(fs::exists(replaced.backup_file));
    const Preset *repaired = bundle.prints.find_preset(installed.preset_name, false, true);
    REQUIRE(repaired != nullptr);
    CHECK(repaired->inherits().empty());
    CHECK(repaired->version == source_version);
    CHECK(repaired->config.option<ConfigOptionInt>("wall_loops")->value == 4);

    DynamicPrintConfig repaired_file;
    std::map<std::string, std::string> repaired_headers;
    std::string repaired_reason;
    repaired_file.load_from_json(repaired->file, ForwardCompatibilitySubstitutionRule::Disable,
                                 repaired_headers, repaired_reason);
    CHECK(repaired_reason.empty());
    CHECK(repaired_headers[BBL_JSON_KEY_VERSION] == source_version.to_string());
    CHECK(repaired_file.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value.empty());
}

TEST_CASE("User preset rename updates direct children without leaving a duplicate", "[Preset][Inspector][Rename]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    const fs::path process_dir = temp_dir.path / PRESET_PRINT_NAME;
    const fs::path parent_file = process_dir / "Old Process.json";
    const fs::path child_file = process_dir / "Z Child Process.json";
    fs::create_directories(process_dir);
    Preset &old_preset = add_inmemory_preset(bundle.prints, "Old Process");
    old_preset.file = parent_file.string();
    old_preset.save(nullptr);
    Preset &child_preset = add_inmemory_preset(bundle.prints, "Z Child Process", "Old Process");
    child_preset.file = child_file.string();
    child_preset.save(&bundle.prints.find_preset("Old Process", false, true)->config);
    Preset &dependent_filament = add_inmemory_preset(bundle.filaments, "Dependent Filament");
    dependent_filament.config.option<ConfigOptionStrings>("compatible_prints", true)->values = { "Old Process" };
    Preset &dependent_printer = add_inmemory_preset(bundle.printers, "Dependent Printer");
    dependent_printer.config.option<ConfigOptionString>("default_print_profile", true)->value = "Old Process";
    Preset *old = bundle.prints.find_preset("Old Process", false, true);
    REQUIRE(old != nullptr);
    old->setting_id = "stable-process-id";
    bundle.prints.select_preset_by_name(old->name, true);
    bundle.prints.get_edited_preset().config.option<ConfigOptionInt>("wall_loops")->value = 6;

    const PresetRecoveryActionResult renamed =
        rename_user_preset(bundle.prints, "Old Process", "Renamed Process", &bundle);
    INFO(renamed.error);
    REQUIRE(renamed.succeeded);
    CHECK(fs::exists(renamed.backup_file));
    CHECK(bundle.prints.find_preset("Old Process", false, true) == nullptr);
    CHECK_FALSE(fs::exists(parent_file));

    const Preset *replacement = bundle.prints.find_preset("Renamed Process", false, true);
    const Preset *child = bundle.prints.find_preset("Z Child Process", false, true);
    REQUIRE(replacement != nullptr);
    REQUIRE(child != nullptr);
    CHECK(replacement->setting_id == "stable-process-id");
    CHECK(replacement->config.option<ConfigOptionInt>("wall_loops")->value == 6);
    CHECK(std::find(replacement->renamed_from.begin(), replacement->renamed_from.end(), "Old Process") != replacement->renamed_from.end());
    CHECK(child->inherits() == "Renamed Process");
    CHECK(child->base_id == "stable-process-id");
    CHECK(bundle.prints.get_selected_preset_name() == "Renamed Process");
    CHECK(bundle.filaments.find_preset("Dependent Filament", false, true)
              ->config.option<ConfigOptionStrings>("compatible_prints")->values ==
          std::vector<std::string>{ "Renamed Process" });
    CHECK(bundle.printers.find_preset("Dependent Printer", false, true)
              ->config.option<ConfigOptionString>("default_print_profile")->value == "Renamed Process");

    DynamicPrintConfig child_on_disk;
    std::map<std::string, std::string> child_headers;
    std::string child_reason;
    child_on_disk.load_from_json(child->file, ForwardCompatibilitySubstitutionRule::Disable,
                                 child_headers, child_reason);
    CHECK(child_reason.empty());
    CHECK(child_on_disk.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value == "Renamed Process");

    const PresetRecoveryActionResult collision =
        rename_user_preset(bundle.prints, "Renamed Process", "Z Child Process", &bundle);
    CHECK_FALSE(collision.succeeded);
    CHECK(collision.error.find("already exists") != std::string::npos);
}

TEST_CASE("Rejected child repair reparents the existing preset without creating copies", "[Preset][Inspector][Recovery]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;
    const fs::path process_dir = temp_dir.path / PRESET_PRINT_NAME;
    const fs::path source_file = process_dir / "Lost Fine Process.json";
    fs::create_directories(process_dir);
    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);

    Preset &chosen_parent = add_inmemory_preset(bundle.prints, "Generic Fine Process");
    chosen_parent.is_system = true;
    chosen_parent.config.option<ConfigOptionInt>("wall_loops")->value = 8;
    add_inmemory_preset(bundle.prints, "Unrelated Draft Process").is_system = true;
    write_print_preset_diff(source_file, "Lost Fine Process", "Missing Fine Process", 0.12);

    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);
    REQUIRE(bundle.prints.load_issues().size() == 1);
    const PresetLoadIssue issue = bundle.prints.load_issues().front();
    CHECK(missing_parent_from_load_issue(issue) == "Missing Fine Process");
    const std::vector<std::string> candidates = recovery_parent_candidates(bundle.prints, "Missing Fine Process");
    REQUIRE_FALSE(candidates.empty());
    CHECK(candidates.front() == "Generic Fine Process");
    CHECK(recovery_parent_candidates(bundle.prints, "fdm_klipper_common").empty());

    std::ifstream before_stream(source_file.string(), std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_stream)), std::istreambuf_iterator<char>());
    const PresetRecoveryActionResult recovered =
        reparent_rejected_preset(bundle.prints, issue, chosen_parent.name);
    REQUIRE(recovered.succeeded);
    CHECK(recovered.preset_name == "Lost Fine Process");
    CHECK(recovered.backup_file == source_file.string() + ".recovery-backup");
    CHECK(bundle.prints.load_issues().empty());

    const Preset *repaired = bundle.prints.find_preset(recovered.preset_name, false, true);
    REQUIRE(repaired != nullptr);
    CHECK(repaired->inherits() == chosen_parent.name);
    CHECK(repaired->version == Semver(1, 0, 0));
    CHECK(repaired->config.option<ConfigOptionInt>("wall_loops")->value == 8);
    CHECK(repaired->config.option<ConfigOptionFloat>("layer_height")->value == Catch::Approx(0.12));
    CHECK(repaired->config.option<ConfigOptionString>("print_settings_id")->value == recovered.preset_name);
    CHECK(repaired->file == source_file.string());
    CHECK_FALSE(fs::exists(process_dir / "Lost Fine Process (Recovered).json"));

    std::ifstream backup_stream(recovered.backup_file, std::ios::binary);
    const std::string backup((std::istreambuf_iterator<char>(backup_stream)), std::istreambuf_iterator<char>());
    CHECK(backup == before);

    std::ifstream repaired_stream(source_file.string());
    const nlohmann::json repaired_json = nlohmann::json::parse(repaired_stream);
    CHECK(repaired_json.at(BBL_JSON_KEY_INHERITS) == chosen_parent.name);
    nlohmann::json expected_json = nlohmann::json::parse(before);
    expected_json[BBL_JSON_KEY_INHERITS] = chosen_parent.name;
    CHECK(repaired_json == expected_json);
}

TEST_CASE("Legacy user preset without version metadata remains loadable", "[Preset][Inspector][Migration]")
{
    TempPresetDir              temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;
    const fs::path process_dir = temp_dir.path / PRESET_PRINT_NAME;
    const fs::path source_file = process_dir / "Legacy Process.json";
    write_print_preset(bundle.prints.default_preset().config, source_file, "Legacy Process");

    nlohmann::json source_json;
    {
        std::ifstream input(source_file.string());
        input >> source_json;
    }
    source_json.erase(BBL_JSON_KEY_VERSION);
    {
        std::ofstream output(source_file.string());
        output << source_json.dump(2);
    }

    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);
    Preset *loaded = bundle.prints.find_preset("Legacy Process", false, true);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->version == SEMVER);
    CHECK(bundle.prints.load_issues().empty());

    nlohmann::json unchanged_json;
    {
        std::ifstream input(source_file.string());
        input >> unchanged_json;
    }
    CHECK_FALSE(unchanged_json.contains(BBL_JSON_KEY_VERSION));

    loaded->save(nullptr);
    nlohmann::json normalized_json;
    {
        std::ifstream input(source_file.string());
        input >> normalized_json;
    }
    CHECK(normalized_json.at(BBL_JSON_KEY_VERSION).get<std::string>() == SEMVER.to_string());
}

TEST_CASE("Standalone preset export removes hard parent dependency and round trips", "[Preset][Export][Compatibility]")
{
    TempPresetDir source_dir;
    PresetBundle  source;

    Preset &parent = add_inmemory_preset(source.prints, "Unavailable Process Parent");
    parent.config.option<ConfigOptionInt>("wall_loops")->value = 7;

    Preset &child = add_inmemory_preset(source.prints, "Portable Process", parent.name);
    child.config = parent.config;
    child.inherits() = parent.name;
    child.config.option<ConfigOptionFloat>("layer_height")->value = 0.16;

    const DynamicPrintConfig exported = child.standalone_export_config();
    CHECK(child.inherits() == parent.name);
    CHECK(exported.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value.empty());
    CHECK(exported.option<ConfigOptionInt>("wall_loops")->value == 7);
    CHECK(exported.option<ConfigOptionFloat>("layer_height")->value == Catch::Approx(0.16));

    const fs::path exported_file = source_dir.path / PRESET_PRINT_NAME / "Portable Process.json";
    fs::create_directories(exported_file.parent_path());
    exported.save_to_json(exported_file.string(), child.name, "User", "1.0.0");

    PresetBundle               destination;
    PresetsConfigSubstitutions substitutions;
    destination.prints.load_presets(source_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                                    ForwardCompatibilitySubstitutionRule::Disable);
    const Preset *imported = destination.prints.find_preset(child.name, false, true);
    REQUIRE(imported != nullptr);
    CHECK(imported->inherits().empty());
    CHECK(imported->config.option<ConfigOptionInt>("wall_loops")->value == 7);
    CHECK(imported->config.option<ConfigOptionFloat>("layer_height")->value == Catch::Approx(0.16));
    CHECK(destination.prints.load_issues().empty());

    Preset &filament = add_inmemory_preset(source.filaments, "Portable Filament", "Unavailable Filament Parent");
    filament.filament_id = "portable-filament-id";
    const DynamicPrintConfig filament_export = filament.standalone_export_config();
    CHECK(filament_export.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value.empty());
    CHECK(filament_export.option<ConfigOptionString>(BBL_JSON_KEY_FILAMENT_ID)->value == "portable-filament-id");

    Preset &printer = add_inmemory_preset(source.printers, "Portable Printer", "Unavailable Printer Parent");
    const DynamicPrintConfig printer_export = printer.standalone_export_config();
    CHECK(printer_export.option<ConfigOptionString>(BBL_JSON_KEY_INHERITS)->value.empty());
}

TEST_CASE("Preset import preflight resolves and orders parent dependencies without writing", "[Preset][Import][Preflight]")
{
    TempPresetDir temp_dir;
    PresetBundle  bundle;
    const fs::path parent_file = temp_dir.path / "Parent.json";
    const fs::path child_file  = temp_dir.path / "Child.json";

    write_print_preset(bundle.prints.default_preset().config, parent_file, "Included Parent");
    write_print_preset(bundle.prints.default_preset().config, child_file, "Included Child", "Included Parent");

    SECTION("an included parent is ordered before its child") {
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { child_file.string(), parent_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        REQUIRE(preflight.ordered_files.size() == 2);
        CHECK(preflight.ordered_files[0] == parent_file.string());
        CHECK(preflight.ordered_files[1] == child_file.string());
    }

    SECTION("an installed parent satisfies the dependency") {
        add_inmemory_preset(bundle.prints, "Included Parent");
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { child_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        CHECK(preflight.ordered_files == std::vector<std::string>{ child_file.string() });
    }

    SECTION("a missing parent blocks the whole selection and performs no writes") {
        const fs::path marker = temp_dir.path / "unchanged.marker";
        {
            std::ofstream output(marker.string());
            output << "unchanged";
        }
        const auto marker_time = fs::last_write_time(marker);
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { child_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE_FALSE(preflight.can_import());
        CHECK(preflight.ordered_files.empty());
        REQUIRE(preflight.issues.size() == 1);
        CHECK(preflight.issues.front().kind == PresetImportIssueKind::MissingParent);
        CHECK(preflight.issues.front().dependency == "Included Parent");
        CHECK(fs::last_write_time(marker) == marker_time);
        CHECK(format_preset_import_preflight(preflight).find("No presets were imported") != std::string::npos);
    }

    SECTION("duplicate definitions are ambiguous") {
        const fs::path duplicate = temp_dir.path / "Duplicate.json";
        write_print_preset(bundle.prints.default_preset().config, duplicate, "Included Parent");
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { parent_file.string(), duplicate.string(), child_file.string() },
            ForwardCompatibilitySubstitutionRule::Disable);
        CHECK_FALSE(preflight.can_import());
        CHECK(std::any_of(preflight.issues.begin(), preflight.issues.end(), [](const PresetImportIssue &issue) {
            return issue.kind == PresetImportIssueKind::DuplicatePreset;
        }));
        CHECK(std::any_of(preflight.issues.begin(), preflight.issues.end(), [](const PresetImportIssue &issue) {
            return issue.kind == PresetImportIssueKind::AmbiguousParent;
        }));
    }

    SECTION("an existing user preset is disclosed as a deliberate replacement") {
        add_inmemory_preset(bundle.prints, "Included Parent");
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { parent_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        REQUIRE(preflight.candidates.size() == 1);
        CHECK(preflight.candidates.front().replaces_user_preset);
        const std::string review = format_preset_import_review(preflight);
        CHECK(review.find("Existing user presets with the same name: 1") != std::string::npos);
        CHECK(review.find("name already exists") != std::string::npos);
    }

    SECTION("a protected system or default name blocks migration before writing") {
        const std::string protected_name = bundle.prints.default_preset().name;
        const fs::path protected_file = temp_dir.path / "Protected.json";
        write_print_preset(bundle.prints.default_preset().config, protected_file, protected_name);
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { protected_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE_FALSE(preflight.can_import());
        CHECK(std::any_of(preflight.issues.begin(), preflight.issues.end(), [](const PresetImportIssue &issue) {
            return issue.kind == PresetImportIssueKind::ProtectedPresetConflict;
        }));
    }

    SECTION("inheritance cycles are rejected") {
        const fs::path first  = temp_dir.path / "Cycle A.json";
        const fs::path second = temp_dir.path / "Cycle B.json";
        write_print_preset(bundle.prints.default_preset().config, first, "Cycle A", "Cycle B");
        write_print_preset(bundle.prints.default_preset().config, second, "Cycle B", "Cycle A");
        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { first.string(), second.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        CHECK_FALSE(preflight.can_import());
        CHECK(std::count_if(preflight.issues.begin(), preflight.issues.end(), [](const PresetImportIssue &issue) {
            return issue.kind == PresetImportIssueKind::InheritanceCycle;
        }) == 2);
    }

    SECTION("missing compatibility references warn without blocking import") {
        const fs::path filament_file = temp_dir.path / "Soft reference filament.json";
        DynamicPrintConfig filament_config(bundle.filaments.default_preset().config);
        filament_config.option<ConfigOptionStrings>("compatible_printers", true)->values = { "Unavailable Printer" };
        filament_config.save_to_json(filament_file.string(), "Soft Reference Filament", "User", "1.0.0");

        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { filament_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        CHECK(preflight.ordered_files == std::vector<std::string>{ filament_file.string() });
        REQUIRE(preflight.warnings.size() == 1);
        CHECK(preflight.warnings.front().kind == PresetImportIssueKind::MissingCompatibilityReference);
        CHECK(preflight.warnings.front().dependency == "Unavailable Printer");
        CHECK(format_preset_import_warnings(preflight.warnings).find("did not block") != std::string::npos);
    }

    SECTION("a compatible printer included in the selection needs no warning") {
        const fs::path printer_file  = temp_dir.path / "Included Printer.json";
        const fs::path filament_file = temp_dir.path / "Included Filament.json";
        DynamicPrintConfig printer_config(bundle.printers.default_preset().config);
        printer_config.save_to_json(printer_file.string(), "Included Printer", "User", "1.0.0");
        DynamicPrintConfig filament_config(bundle.filaments.default_preset().config);
        filament_config.option<ConfigOptionStrings>("compatible_printers", true)->values = { "Included Printer" };
        filament_config.save_to_json(filament_file.string(), "Included Filament", "User", "1.0.0");

        const PresetImportPreflight preflight = preflight_preset_imports(
            bundle, { filament_file.string(), printer_file.string() }, ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        CHECK(preflight.warnings.empty());
    }
}

TEST_CASE("Preset migration folder discovery includes user trees and excludes vendor data", "[Preset][Import][Migration]")
{
    TempPresetDir temp_dir;
    PresetBundle  bundle;

    const fs::path machine = temp_dir.path / "user_backup-v2.5.0-dev" / "default" / PRESET_PRINTER_NAME / "Printer.json";
    const fs::path process = temp_dir.path / "user" / "default" / PRESET_PRINT_NAME / "Process.JSON";
    const fs::path filament = temp_dir.path / "loose" / PRESET_FILAMENT_NAME / "Filament.json";
    const fs::path system = temp_dir.path / "system" / PRESET_PRINT_NAME / "Vendor.json";
    const fs::path runtime = temp_dir.path / "log" / PRESET_PRINT_NAME / "Log.json";
    write_preset_with_inherits(bundle.printers.default_preset().config, machine, "Printer", {});
    write_preset_with_inherits(bundle.prints.default_preset().config, process, "Process", {});
    write_preset_with_inherits(bundle.filaments.default_preset().config, filament, "Filament", {});
    write_preset_with_inherits(bundle.prints.default_preset().config, system, "Vendor", {});
    write_preset_with_inherits(bundle.prints.default_preset().config, runtime, "Log", {});

    const std::vector<std::string> files = discover_preset_migration_files(temp_dir.path.string());
    CHECK(files == std::vector<std::string>{ filament.string(), process.string(), machine.string() });

    const fs::path loose_root = temp_dir.path / "copied-presets";
    const fs::path loose_file = loose_root / "My copied preset.json";
    write_preset_with_inherits(bundle.prints.default_preset().config, loose_file, "My copied preset", {});
    CHECK(discover_preset_migration_files(loose_root.string()) == std::vector<std::string>{ loose_file.string() });
}

TEST_CASE("Preset migration source discovery keeps current, backups, and versions separate", "[Preset][Import][Migration]")
{
    TempPresetDir temp_dir;
    PresetBundle  bundle;

    write_print_preset(bundle.prints.default_preset().config,
                       temp_dir.path / PRESET_USER_DIR / "default" / PRESET_PRINT_NAME / "Current.json", "Current");
    write_print_preset(bundle.prints.default_preset().config,
                       temp_dir.path / "user_backup-v2.3.1" / "default" / PRESET_PRINT_NAME / "Backup.json", "Backup");
    write_print_preset(bundle.prints.default_preset().config,
                       temp_dir.path / "versions" / "2.5.0-dev" / PRESET_USER_DIR / "default" / PRESET_PRINT_NAME / "Version.json", "Version");

    const std::vector<PresetMigrationSource> sources = discover_preset_migration_sources(temp_dir.path.string());
    REQUIRE(sources.size() == 3);
    CHECK(std::any_of(sources.begin(), sources.end(), [](const PresetMigrationSource &source) {
        return source.label == "User settings";
    }));
    CHECK(std::any_of(sources.begin(), sources.end(), [](const PresetMigrationSource &source) {
        return source.label == "Backup v2.3.1";
    }));
    CHECK(std::any_of(sources.begin(), sources.end(), [](const PresetMigrationSource &source) {
        return source.label == "Version 2.5.0-dev";
    }));
    for (const PresetMigrationSource &source : sources)
        CHECK(discover_preset_migration_files(source.folder).size() == 1);
}

TEST_CASE("Preset archive preflight validates the whole archive before import", "[Preset][Import][Preflight][Archive]")
{
    TempPresetDir temp_dir;
    PresetBundle  bundle;
    const fs::path parent_file = temp_dir.path / "Parent.json";
    const fs::path child_file  = temp_dir.path / "Child.json";

    write_print_preset(bundle.prints.default_preset().config, parent_file, "Archive Parent");
    write_print_preset(bundle.prints.default_preset().config, child_file, "Archive Child", "Archive Parent");

    SECTION("children encountered first are reordered after their included parent") {
        const fs::path archive_file = temp_dir.path / "ordered.orca_bundle";
        write_preset_archive(archive_file, { { "process/Child.json", child_file }, { "process/Parent.json", parent_file } });

        const PresetImportPreflight preflight = preflight_preset_archive(
            bundle, archive_file.string(), ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        REQUIRE(preflight.ordered_files.size() == 2);
        CHECK(preflight.ordered_files[0] == "process/Parent.json");
        CHECK(preflight.ordered_files[1] == "process/Child.json");
    }

    SECTION("a missing parent blocks the entire archive") {
        const fs::path archive_file = temp_dir.path / "missing-parent.zip";
        write_preset_archive(archive_file, { { "process/Child.json", child_file } });

        const PresetImportPreflight preflight = preflight_preset_archive(
            bundle, archive_file.string(), ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE_FALSE(preflight.can_import());
        CHECK(preflight.ordered_files.empty());
        REQUIRE(preflight.issues.size() == 1);
        CHECK(preflight.issues.front().kind == PresetImportIssueKind::MissingParent);
        CHECK(preflight.issues.front().file.find("process/Child.json") != std::string::npos);
    }

    SECTION("a missing compatibility target is an archive notice, not a blocker") {
        const fs::path filament_file = temp_dir.path / "Archive Filament.json";
        const fs::path archive_file  = temp_dir.path / "soft-reference.orca_filament";
        DynamicPrintConfig filament_config(bundle.filaments.default_preset().config);
        filament_config.option<ConfigOptionStrings>("compatible_printers", true)->values = { "Unavailable Printer" };
        filament_config.save_to_json(filament_file.string(), "Archive Filament", "User", "1.0.0");
        write_preset_archive(archive_file, { { "filament/Archive Filament.json", filament_file } });

        const PresetImportPreflight preflight = preflight_preset_archive(
            bundle, archive_file.string(), ForwardCompatibilitySubstitutionRule::Disable);
        REQUIRE(preflight.can_import());
        REQUIRE(preflight.warnings.size() == 1);
        CHECK(preflight.warnings.front().dependency == "Unavailable Printer");
        CHECK(preflight.warnings.front().file.find("filament/Archive Filament.json") != std::string::npos);
    }

    SECTION("bundle-scoped names are not mistaken for installed system-name conflicts") {
        const fs::path bundled_preset = temp_dir.path / "Bundled.json";
        const fs::path structure = temp_dir.path / BUNDLE_STRUCTURE_JSON_NAME;
        const fs::path archive_file = temp_dir.path / "scoped.orca_bundle";
        write_print_preset(bundle.prints.default_preset().config, bundled_preset,
                           bundle.prints.default_preset().name);
        {
            std::ofstream output(structure.string());
            output << "{}";
        }
        write_preset_archive(archive_file, {
            { BUNDLE_STRUCTURE_JSON_NAME, structure },
            { "process/Bundled.json", bundled_preset }
        });

        const PresetImportPreflight preflight = preflight_preset_archive(
            bundle, archive_file.string(), ForwardCompatibilitySubstitutionRule::Disable);
        CHECK(preflight.can_import());
        REQUIRE(preflight.candidates.size() == 1);
        CHECK_FALSE(preflight.candidates.front().replaces_user_preset);
    }
}

TEST_CASE("Preset identity is canonicalized from load path", "[Preset][Identity]")
{
    ScopedTemporaryDir         temp_dir;
    PresetBundle               bundle;
    PresetsConfigSubstitutions substitutions;

    write_print_preset(bundle.prints.default_preset().config, temp_dir.path() / PRESET_PRINT_NAME / "User.json", "User");
    write_print_preset(bundle.prints.default_preset().config, temp_dir.path() / PRESET_LOCAL_DIR / "bundle-1" / PRESET_PRINT_NAME / "LocalBundle.json", "LocalBundle");
    write_print_preset(bundle.prints.default_preset().config, temp_dir.path() / PRESET_SUBSCRIBED_DIR / "remote-1" / PRESET_PRINT_NAME / "Subscribed.json", "Subscribed");

    bundle.prints.load_presets(temp_dir.path().string(), PRESET_PRINT_NAME, substitutions, ForwardCompatibilitySubstitutionRule::Disable);
    bundle.prints.load_presets((temp_dir.path() / PRESET_LOCAL_DIR / "bundle-1").string(), PRESET_PRINT_NAME, substitutions, ForwardCompatibilitySubstitutionRule::Disable);
    bundle.prints.load_presets((temp_dir.path() / PRESET_SUBSCRIBED_DIR / "remote-1").string(), PRESET_PRINT_NAME, substitutions, ForwardCompatibilitySubstitutionRule::Disable);

    const Preset *root_user = bundle.prints.find_preset("User");
    REQUIRE(root_user != nullptr);
    CHECK(root_user->name == "User");
    CHECK_FALSE(root_user->is_from_bundle());

    const Preset *local_bundle = bundle.prints.find_preset("_local/bundle-1/LocalBundle");
    REQUIRE(local_bundle != nullptr);
    CHECK(local_bundle->name == "_local/bundle-1/LocalBundle");
    CHECK(local_bundle->is_from_bundle());

    const Preset *subscribed = bundle.prints.find_preset("_subscribed/remote-1/Subscribed");
    REQUIRE(subscribed != nullptr);
    CHECK(subscribed->name == "_subscribed/remote-1/Subscribed");
    CHECK(subscribed->is_from_bundle());
}

TEST_CASE("Legacy bundle import without bundle metadata stays in the user preset directory", "[Preset][Identity]")
{
    ScopedTemporaryDir temp_dir;
    PresetBundle  bundle;

    PresetsConfigSubstitutions substitutions;
    std::vector<std::string>   result;
    int                        overwrite = 0;
    std::string                file      = (temp_dir.path() / "legacy-bundle" / "Imported.json").string();
    const fs::path             user_root = temp_dir.path() / "user";

    write_print_preset(bundle.prints.default_preset().config, file, "Imported");
    fs::create_directories(user_root);
    bundle.prints.update_user_presets_directory(user_root.string(), PRESET_PRINT_NAME);

    REQUIRE(bundle.import_json_presets(
        substitutions,
        file,
        [](std::string const &) { return 1; },
        ForwardCompatibilitySubstitutionRule::Disable,
        overwrite,
        result));

    const Preset *imported = bundle.prints.find_preset("Imported");
    REQUIRE(imported != nullptr);
    CHECK(imported->name == "Imported");
    CHECK(imported->bundle_id.empty());
    CHECK_FALSE(imported->is_from_bundle());
    // Detached user presets (no inherits) are saved in the "base" subfolder of the user preset root.
    CHECK(fs::equivalent(fs::path(imported->file).parent_path().parent_path(), user_root / PRESET_PRINT_NAME));
}

TEST_CASE("Current vendor type tolerates missing printer model", "[Preset][Bundle]")
{
    PresetBundle bundle;

    VendorProfile orca_vendor; orca_vendor.id = "ORCA";
    VendorProfile::PrinterModel model;
    model.name = "Orca Test";
    orca_vendor.models.emplace_back(model);
    bundle.vendors.emplace("ORCA", std::move(orca_vendor));

    bundle.printers.get_edited_preset().config.erase("printer_model");

    CHECK(bundle.get_current_vendor_type() == VendorType::Unknown);
}

TEST_CASE("A malformed entry in a vendor's preset list is counted, not thrown", "[Preset][Bundle]")
{
    ScopedTemporaryDir dir;

    // A bare number where the list wants an object. An array element has no key,
    // so reporting one as if it did throws nlohmann's invalid_iterator - which is
    // not a parse_error, and escapes the catch around the vendor profile parse.
    std::ofstream((dir.path() / "Acme.json").string())
        << R"({"version":"1.0.0","name":"Acme","process_list":[123,)"
        << R"({"name":"0.20mm Standard @Acme","sub_path":"process/standard.json"}]})";
    fs::create_directories(dir.path() / "Acme" / "process");
    std::ofstream((dir.path() / "Acme" / "process" / "standard.json").string())
        << R"({"type":"process","name":"0.20mm Standard @Acme","from":"system",)"
        << R"("instantiation":"true","layer_height":"0.2"})";

    PresetBundle bundle;
    size_t       loaded = 0;
    REQUIRE_NOTHROW(loaded = bundle.load_vendor_configs_from_json(
                        dir.path().string(), "Acme", PresetBundle::LoadSystem,
                        ForwardCompatibilitySubstitutionRule::EnableSilent).second);

    CHECK(bundle.error_count() > 0);   // the malformed element was counted
    CHECK(loaded == 1);                // the well-formed one beside it still loaded
}

TEST_CASE("Printer extruder count tolerates missing nozzle diameter", "[Preset][Bundle]")
{
    PresetBundle bundle;
    DynamicPrintConfig& config = bundle.printers.get_edited_preset().config;

    config.erase("nozzle_diameter");
    CHECK(bundle.get_printer_extruder_count() == 1);

    config.set_key_value("nozzle_diameter", new ConfigOptionFloats());
    CHECK(bundle.get_printer_extruder_count() == 1);

    config.set_key_value("nozzle_diameter", new ConfigOptionFloats({ 0.4, 0.6 }));
    CHECK(bundle.get_printer_extruder_count() == 2);
}

TEST_CASE("find_preset resolves a system preset's renamed_from", "[Preset][Rename]")
{
    RenameTestCollection coll;

    // "New Process" is the current preset; it was renamed from "Old Process".
    add_inmemory_preset(coll, "New Process");
    set_renamed_from(coll, "New Process", { "Old Process" });
    coll.update_map_system_profile_renamed();

    // The rename map knows the old name...
    const std::string *renamed = coll.get_preset_name_renamed("Old Process");
    REQUIRE(renamed != nullptr);
    CHECK(*renamed == "New Process");

    // ...and plain find_preset() now follows it (the core of this PR; previously this
    // resolution lived only in find_preset2 and a few call sites).
    const Preset *resolved = coll.find_preset("Old Process");
    REQUIRE(resolved != nullptr);
    CHECK(resolved->name == "New Process");

    // A genuinely unknown name still returns null (no spurious match).
    CHECK(coll.find_preset("Totally Unknown") == nullptr);

    // A child that still inherits the OLD name resolves through the runtime walker,
    // which uses plain find_preset().
    Preset       &child  = add_inmemory_preset(coll, "Child Process", "Old Process");
    const Preset *parent = coll.get_preset_parent(child);
    REQUIRE(parent != nullptr);
    CHECK(parent->name == "New Process");
}

TEST_CASE("find_preset resolves a preset renamed more than once", "[Preset][Rename]")
{
    RenameTestCollection coll;

    // "New Process" was renamed twice, so it carries both former names in renamed_from.
    add_inmemory_preset(coll, "New Process");
    set_renamed_from(coll, "New Process", { "Original Process", "Old Process" });
    coll.update_map_system_profile_renamed();

    // Each historical name resolves to the current preset.
    for (const char *old_name : { "Original Process", "Old Process" }) {
        INFO("resolving old name: " << old_name);
        const std::string *renamed = coll.get_preset_name_renamed(old_name);
        REQUIRE(renamed != nullptr);
        CHECK(*renamed == "New Process");

        const Preset *resolved = coll.find_preset(old_name);
        REQUIRE(resolved != nullptr);
        CHECK(resolved->name == "New Process");
    }

    // A child inheriting either former name resolves through the runtime walker.
    Preset &child = add_inmemory_preset(coll, "Child Process", "Original Process");
    REQUIRE(coll.get_preset_parent(child) != nullptr);
    CHECK(coll.get_preset_parent(child)->name == "New Process");
}

TEST_CASE("find_preset2 auto-matches removed Generic vendor profiles to the library", "[Preset][Rename]")
{
    PresetBundle bundle;

    // The OrcaFilamentLibrary replacement that removed empty "<vendor> Generic" profiles map to.
    add_inmemory_preset(bundle.filaments, "Generic PLA @System");

    // Plain lookups do NOT fuzzy-match a removed vendor profile.
    CHECK(bundle.filaments.find_preset("Voron Generic PLA") == nullptr);
    CHECK(bundle.filaments.find_preset2("Voron Generic PLA", /*auto_match=*/false) == nullptr);

    // With auto_match, the removed "Voron Generic PLA" resolves to "Generic PLA @System".
    const Preset *matched = bundle.filaments.find_preset2("Voron Generic PLA", /*auto_match=*/true);
    REQUIRE(matched != nullptr);
    CHECK(matched->name == "Generic PLA @System");

    // No library preset exists for an unrelated material => still no match.
    CHECK(bundle.filaments.find_preset2("BrandX Generic PETG", /*auto_match=*/true) == nullptr);
}

TEST_CASE("Renamed parent is normalized into a loaded preset's inherits", "[Preset][Rename]")
{
    ScopedTemporaryDir   temp_dir;
    RenameTestCollection coll;

    // Current parent, renamed from "Old Process".
    add_inmemory_preset(coll, "New Process");
    set_renamed_from(coll, "New Process", { "Old Process" });
    coll.update_map_system_profile_renamed();

    // A user preset on disk that still inherits the OLD name.
    write_preset_with_inherits(coll.default_preset().config,
                               temp_dir.path() / PRESET_PRINT_NAME / "Child.json", "Child", "Old Process");

    PresetsConfigSubstitutions substitutions;
    coll.load_presets(temp_dir.path().string(), PRESET_PRINT_NAME, substitutions,
                      ForwardCompatibilitySubstitutionRule::Disable);

    const Preset *child = coll.find_preset("Child");
    REQUIRE(child != nullptr);
    // The dangling "Old Process" was rewritten to the resolved parent name at load time,
    // so the runtime walker (plain find_preset) can resolve the chain.
    CHECK(child->inherits() == "New Process");
    REQUIRE(coll.get_preset_parent(*child) != nullptr);
    CHECK(coll.get_preset_parent(*child)->name == "New Process");
}

TEST_CASE("Removed Generic parent is normalized into a loaded filament's inherits", "[Preset][Rename]")
{
    ScopedTemporaryDir temp_dir;
    PresetBundle  bundle;

    add_inmemory_preset(bundle.filaments, "Generic PLA @System");

    // A user filament that still inherits a removed "<vendor> Generic PLA" profile.
    write_preset_with_inherits(bundle.filaments.default_preset().config,
                               temp_dir.path() / PRESET_FILAMENT_NAME / "MyPLA.json", "MyPLA", "Voron Generic PLA");

    PresetsConfigSubstitutions substitutions;
    bundle.filaments.load_presets(temp_dir.path().string(), PRESET_FILAMENT_NAME, substitutions,
                                  ForwardCompatibilitySubstitutionRule::Disable);

    const Preset *child = bundle.filaments.find_preset("MyPLA");
    REQUIRE(child != nullptr);
    CHECK(child->inherits() == "Generic PLA @System");
    REQUIRE(bundle.filaments.get_preset_parent(*child) != nullptr);
    CHECK(bundle.filaments.get_preset_parent(*child)->name == "Generic PLA @System");
}

TEST_CASE("User print preset inherits its system parent's active configuration", "[Preset][Compatibility]")
{
    TempPresetDir temp_dir;
    PresetBundle  bundle;

    Preset &system = add_inmemory_preset(bundle.prints, "System Process");
    const std::string system_name = system.name;
    system.is_system = true;
    system.config.option<ConfigOptionInt>("wall_loops")->value = 7;
    system.config.option<ConfigOptionFloat>("layer_height")->value = 0.28;

    write_print_preset_diff(temp_dir.path / PRESET_PRINT_NAME / "User Process.json",
                            "User Process", system_name, 0.16);

    PresetsConfigSubstitutions substitutions;
    bundle.prints.load_presets(temp_dir.path.string(), PRESET_PRINT_NAME, substitutions,
                               ForwardCompatibilitySubstitutionRule::Disable);

    const Preset *user          = bundle.prints.find_preset("User Process");
    const Preset *system_loaded = bundle.prints.find_preset("System Process");
    REQUIRE(user != nullptr);
    REQUIRE(system_loaded != nullptr);
    REQUIRE(bundle.prints.get_preset_parent(*user) == system_loaded);
    CHECK(bundle.prints.get_preset_base(*user) == system_loaded);
    CHECK(user->inherits() == system_name);
    CHECK(user->config.opt_int("wall_loops") == 7);
    CHECK(user->config.opt_float("layer_height") == Catch::Approx(0.16));
}

TEST_CASE("Preset compatibility expressions are deterministic and explain failures", "[Preset][Compatibility]")
{
    PresetBundle bundle;
    Preset      &candidate = add_inmemory_preset(bundle.filaments, "Candidate Filament");
    Preset      &printer   = add_inmemory_preset(bundle.printers, "Two Tool Printer");
    Preset      &process   = add_inmemory_preset(bundle.prints, "Active Process");

    printer.config.set_key_value("nozzle_diameter", new ConfigOptionFloats({ 0.4, 0.4 }));
    process.config.option<ConfigOptionFloat>("layer_height")->value = 0.2;

    const PresetWithVendorProfile candidate_profile(candidate, nullptr);
    const PresetWithVendorProfile printer_profile(printer, nullptr);
    const PresetWithVendorProfile process_profile(process, nullptr);
    std::string                   reason;

    candidate.compatible_printers_condition() = "num_extruders == 2";
    CHECK(is_compatible_with_printer(candidate_profile, printer_profile, &reason));
    CHECK(reason.empty());

    candidate.compatible_printers_condition() = "num_extruders == 1";
    CHECK_FALSE(is_compatible_with_printer(candidate_profile, printer_profile, &reason));
    CHECK(reason.find("compatible_printers_condition") != std::string::npos);

    reason.clear();
    candidate.compatible_prints_condition() = "layer_height == 0.2";
    CHECK(is_compatible_with_print(candidate_profile, process_profile, printer_profile, &reason));
    CHECK(reason.empty());

    candidate.compatible_prints_condition() = "layer_height < 0.1";
    CHECK_FALSE(is_compatible_with_print(candidate_profile, process_profile, printer_profile, &reason));
    CHECK(reason.find("compatible_prints_condition") != std::string::npos);

    SECTION("an explicit compatibility list takes precedence over its expression") {
        candidate.config.option<ConfigOptionStrings>("compatible_printers", true)->values = { printer.name };
        candidate.compatible_printers_condition() = "false";
        reason.clear();
        CHECK(is_compatible_with_printer(candidate_profile, printer_profile, &reason));
        CHECK(reason.empty());
    }

    SECTION("malformed expressions preserve fail-open behavior and provide diagnostics") {
        candidate.compatible_printers_condition() = "num_extruders ==";
        reason.clear();
        CHECK(is_compatible_with_printer(candidate_profile, printer_profile, &reason));
        CHECK(reason.find("Could not evaluate") != std::string::npos);
        CHECK(reason.find("num_extruders ==") != std::string::npos);

        candidate.compatible_prints_condition() = "layer_height ==";
        reason.clear();
        CHECK(is_compatible_with_print(candidate_profile, process_profile, printer_profile, &reason));
        CHECK(reason.find("Could not evaluate") != std::string::npos);
        CHECK(reason.find("layer_height ==") != std::string::npos);
    }
}

namespace {

// A live reference to a preset's compatible_printers / compatible_prints list. Fetches the *stored*
// preset (real=true) so writes and reads hit the same object; creates the option if absent.
std::vector<std::string> &compatible_list(PresetCollection &coll, const std::string &preset_name, const char *field_key)
{
    Preset *preset = coll.find_preset(preset_name, /*first_visible_if_not_found=*/false, /*real=*/true);
    REQUIRE(preset != nullptr);
    return preset->config.option<ConfigOptionStrings>(field_key, true)->values;
}

} // namespace

TEST_CASE("Renamed printer/process names are normalized into compatible lists on load", "[Preset][Rename]")
{
    PresetBundle bundle;

    // Current printer + process, each renamed from an older name.
    add_inmemory_preset(bundle.printers, "New Printer");
    set_renamed_from(bundle.printers, "New Printer", { "Old Printer" });
    add_inmemory_preset(bundle.prints, "New Process");
    set_renamed_from(bundle.prints, "New Process", { "Old Process" });

    // A user process still compatible with the OLD printer name.
    add_inmemory_preset(bundle.prints, "My Process");
    compatible_list(bundle.prints, "My Process", "compatible_printers") = { "Old Printer" };

    // A user filament referencing the OLD printer AND OLD process names, plus an unknown printer.
    add_inmemory_preset(bundle.filaments, "My Filament");
    compatible_list(bundle.filaments, "My Filament", "compatible_printers") = { "Old Printer", "Unknown Printer" };
    compatible_list(bundle.filaments, "My Filament", "compatible_prints")   = { "Old Process" };

    // Build the rename maps (done during system load in the real pipeline), then normalize.
    AppConfig app_config;
    bundle.load_installed_printers(app_config); // rebuilds every collection's rename map
    bundle.normalize_compatible_presets();

    // The stale printer name in a process' compatible_printers is rewritten to the current name.
    CHECK(compatible_list(bundle.prints, "My Process", "compatible_printers") == std::vector<std::string>{ "New Printer" });

    // The stale process name in a filament's compatible_prints is rewritten (this field has no
    // runtime rename fallback, so load-time normalization is the only fix).
    CHECK(compatible_list(bundle.filaments, "My Filament", "compatible_prints") == std::vector<std::string>{ "New Process" });

    // The renamed printer is rewritten while the unknown/deleted name is preserved as-is.
    CHECK(compatible_list(bundle.filaments, "My Filament", "compatible_printers") ==
          (std::vector<std::string>{ "New Printer", "Unknown Printer" }));

    // Normalizing rewrites config in place without flagging the preset dirty.
    CHECK_FALSE(bundle.prints.find_preset("My Process", false, true)->is_dirty);

    // A system preset that already references the current name is left untouched (idempotent no-op).
    bundle.normalize_compatible_presets();
    CHECK(compatible_list(bundle.prints, "My Process", "compatible_printers") == std::vector<std::string>{ "New Printer" });
}

TEST_CASE("Renamed names are normalized into a SYSTEM preset's compatible lists", "[Preset][Rename]")
{
    PresetBundle bundle;

    // Current printer + process, each renamed from an older name.
    add_inmemory_preset(bundle.printers, "New Printer");
    set_renamed_from(bundle.printers, "New Printer", { "Old Printer" });
    add_inmemory_preset(bundle.prints, "New Process");
    set_renamed_from(bundle.prints, "New Process", { "Old Process" });

    // A *system* (vendor) filament whose own compatible lists still reference the OLD names. A vendor
    // profile can point at a sibling preset that was later renamed, so system presets must be
    // normalized too (they are skipped by neither collection walk).
    add_inmemory_preset(bundle.filaments, "System Filament").is_system = true;
    compatible_list(bundle.filaments, "System Filament", "compatible_printers") = { "Old Printer" };
    compatible_list(bundle.filaments, "System Filament", "compatible_prints")   = { "Old Process" };

    AppConfig app_config;
    bundle.load_installed_printers(app_config); // build the rename maps
    bundle.normalize_compatible_presets();

    // The stale references in the system preset are rewritten to the current names.
    CHECK(compatible_list(bundle.filaments, "System Filament", "compatible_printers") ==
          std::vector<std::string>{ "New Printer" });
    CHECK(compatible_list(bundle.filaments, "System Filament", "compatible_prints") ==
          std::vector<std::string>{ "New Process" });

    // The rewrite does not flag the system preset dirty, and is idempotent.
    CHECK_FALSE(bundle.filaments.find_preset("System Filament", false, true)->is_dirty);
    bundle.normalize_compatible_presets();
    CHECK(compatible_list(bundle.filaments, "System Filament", "compatible_printers") ==
          std::vector<std::string>{ "New Printer" });
}

TEST_CASE("compatible_prints on SLA materials resolves against sla_prints, not prints", "[Preset][Rename]")
{
    PresetBundle bundle;

    // A renamed SLA process, and a same-named FFF process that must NOT be picked up: resolving the
    // SLA material's compatible_prints against `prints` would wrongly rewrite to "Wrong FFF Process".
    add_inmemory_preset(bundle.sla_prints, "New SLA Process");
    set_renamed_from(bundle.sla_prints, "New SLA Process", { "Old SLA Process" });
    add_inmemory_preset(bundle.prints, "Wrong FFF Process");
    set_renamed_from(bundle.prints, "Wrong FFF Process", { "Old SLA Process" });

    add_inmemory_preset(bundle.sla_materials, "My SLA Material");
    compatible_list(bundle.sla_materials, "My SLA Material", "compatible_prints") = { "Old SLA Process" };

    AppConfig app_config;
    bundle.load_installed_printers(app_config);
    bundle.normalize_compatible_presets();

    CHECK(compatible_list(bundle.sla_materials, "My SLA Material", "compatible_prints") ==
          std::vector<std::string>{ "New SLA Process" });
}

TEST_CASE("Profile validator flags dangling and renamed preset references", "[Preset][Validate]")
{
    PresetBundle bundle;

    // Current printers: a real one, and a renamed one (its old name resolves via renamed_from).
    add_inmemory_preset(bundle.printers, "Real Printer");
    add_inmemory_preset(bundle.printers, "New Printer");
    set_renamed_from(bundle.printers, "New Printer", { "Old Printer" });

    // A real process, referenced from a filament's compatible_prints.
    add_inmemory_preset(bundle.prints, "Real Process").is_system = true;

    // A fully valid system filament: references only current names.
    add_inmemory_preset(bundle.filaments, "Good Filament").is_system = true;
    compatible_list(bundle.filaments, "Good Filament", "compatible_printers") = { "Real Printer" };
    compatible_list(bundle.filaments, "Good Filament", "compatible_prints")   = { "Real Process" };

    AppConfig app_config;
    bundle.load_installed_printers(app_config); // build the rename maps

    // With only valid references, the validator is clean.
    CHECK_FALSE(bundle.check_preset_references());

    SECTION("deleted compatible_printers is flagged") {
        add_inmemory_preset(bundle.filaments, "Ghost Ref Filament").is_system = true;
        compatible_list(bundle.filaments, "Ghost Ref Filament", "compatible_printers") = { "Ghost Printer" };
        CHECK(bundle.check_preset_references());
    }

    SECTION("renamed compatible_printers (old name) is flagged") {
        add_inmemory_preset(bundle.filaments, "Old Ref Filament").is_system = true;
        compatible_list(bundle.filaments, "Old Ref Filament", "compatible_printers") = { "Old Printer" };
        CHECK(bundle.check_preset_references());
    }

    SECTION("deleted compatible_prints is flagged") {
        add_inmemory_preset(bundle.filaments, "Bad Process Ref").is_system = true;
        compatible_list(bundle.filaments, "Bad Process Ref", "compatible_prints") = { "Ghost Process" };
        CHECK(bundle.check_preset_references());
    }

    SECTION("deleted inherits parent is flagged") {
        add_inmemory_preset(bundle.filaments, "Orphan Filament", "Ghost Parent").is_system = true;
        CHECK(bundle.check_preset_references());
    }

    SECTION("non-system preset with a dangling reference is ignored") {
        add_inmemory_preset(bundle.filaments, "User Filament"); // is_system stays false
        compatible_list(bundle.filaments, "User Filament", "compatible_printers") = { "Ghost Printer" };
        CHECK_FALSE(bundle.check_preset_references());
    }
}

// Under a shared override key, the last preset merged into the full config overwrote the others', so an
// edited slicing-pipeline override never reached Print::apply's diff and re-configuring a plugin never
// re-sliced. Per-type keys make that collision impossible; guard the scoping here.
TEST_CASE("Plugin capability override keys are scoped per preset type", "[Preset][Plugin]")
{
    // Pin the key names: presets and 3mf files store them verbatim, so a rename is a format change.
    CHECK(Preset::plugin_overrides_key(Preset::TYPE_PRINT)    == std::string("print_plugin_config_overrides"));
    CHECK(Preset::plugin_overrides_key(Preset::TYPE_PRINTER)  == std::string("printer_plugin_config_overrides"));
    CHECK(Preset::plugin_overrides_key(Preset::TYPE_FILAMENT) == std::string("filament_plugin_config_overrides"));

    // ...and each key lives on exactly its own preset type's option list, so no two ever share a slot.
    const std::pair<Preset::Type, const std::vector<std::string>*> scopes[] = {
        {Preset::TYPE_PRINT,    &Preset::print_options()},
        {Preset::TYPE_PRINTER,  &Preset::printer_options()},
        {Preset::TYPE_FILAMENT, &Preset::filament_options()},
    };
    for (const auto &owner : scopes)
        for (const auto &scoped : scopes) {
            const std::string key = Preset::plugin_overrides_key(scoped.first);
            CAPTURE(owner.first, key);
            CHECK(contains(*owner.second, key) == (owner.first == scoped.first));
        }
}

namespace {

// A standalone filament collection that exposes the protected library masking builder, so the Orca
// Filament Library scenario can be set up without the full system-profile load pipeline.
struct LibraryFilamentTestCollection : public PresetCollection
{
    LibraryFilamentTestCollection()
        : PresetCollection(Preset::TYPE_FILAMENT, Preset::filament_options(),
                           static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()))
    {}
    using PresetCollection::update_library_profile_excluded_from;
};

} // namespace

// Orca: a filament in the Orca Filament Library that names its compatible printers has to hide the generic
// library filament sharing its alias, the same way a vendor owned filament does. Otherwise both are compatible
// with that printer and the plater combo box lists the shared alias twice.
TEST_CASE("A printer specific filament supersedes the generic library filament with the same alias", "[Preset][Bundle]")
{
    LibraryFilamentTestCollection filaments;
    PresetCollection              printers(Preset::TYPE_PRINTER, Preset::printer_options(),
                                           static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()));
    // The masking keys off the vendor name, which VendorProfile's constructor does not derive from the id.
    VendorProfile                 library(PresetBundle::ORCA_FILAMENT_LIBRARY);
    VendorProfile                 vendor("Vendor");
    library.name = PresetBundle::ORCA_FILAMENT_LIBRARY;
    vendor.name  = "Vendor";

    auto add_filament = [&filaments](const VendorProfile &owner, const std::string &name, std::vector<std::string> compatible_printers) {
        Preset &preset = add_inmemory_preset(filaments, name);
        preset.alias   = "Generic ABS";
        preset.vendor  = &owner;
        preset.config.option<ConfigOptionStrings>("compatible_printers", true)->values = std::move(compatible_printers);
    };

    add_filament(library, "Generic ABS @System", {});
    add_filament(library, "Generic ABS @Printer A", { "Printer A" });
    add_filament(vendor,  "Generic ABS @Printer B", { "Printer B" });

    filaments.update_library_profile_excluded_from();

    const Preset *generic = filaments.find_preset("Generic ABS @System");
    REQUIRE(generic != nullptr);
    CHECK(generic->m_excluded_from.count("Printer A") == 1);
    CHECK(generic->m_excluded_from.count("Printer B") == 1);
    CHECK(generic->m_excluded_from.size() == 2);

    // A printer specific profile names printers, so it is never the one being hidden - not even by itself.
    const Preset *specific = filaments.find_preset("Generic ABS @Printer A");
    REQUIRE(specific != nullptr);
    CHECK(specific->m_excluded_from.empty());

    // ...and the generic profile really drops out of the compatible set on the printer it is hidden from.
    add_inmemory_preset(printers, "Printer A");
    add_inmemory_preset(printers, "Printer C");
    const Preset *printer_a = printers.find_preset("Printer A");
    const Preset *printer_c = printers.find_preset("Printer C");
    REQUIRE(printer_a != nullptr);
    REQUIRE(printer_c != nullptr);

    const PresetWithVendorProfile generic_lib(*generic, &library);
    CHECK_FALSE(is_compatible_with_printer(generic_lib, PresetWithVendorProfile(*printer_a, nullptr)));
    CHECK(is_compatible_with_printer(generic_lib, PresetWithVendorProfile(*printer_c, nullptr)));
}

