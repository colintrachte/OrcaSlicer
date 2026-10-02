#include <catch2/catch_all.hpp>

#include "libslic3r/Preset.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <algorithm>

using namespace Slic3r;

// Regression test for the python-plugin branch's intentional divergence from
// upstream in add_correct_opts_to_diff() (src/libslic3r/Preset.cpp): a vector
// option entry whose index is beyond the reference vector's length is reported
// dirty even when it duplicates an existing value. On main these duplicates
// were NOT flagged. See the comment on add_correct_opts_to_diff() in src/libslic3r/Preset.cpp.
TEST_CASE("deep_diff flags new vector entries that duplicate values[0]", "[PresetDiff][Config]")
{
    // reference: single-extruder vector (one entry)
    Preset reference(Preset::TYPE_PRINTER, "ref");
    reference.config.set_key_value("nozzle_diameter", new ConfigOptionFloats{0.4});

    // edited: a second extruder entry was added whose value duplicates the first
    Preset edited(Preset::TYPE_PRINTER, "edited");
    edited.config.set_key_value("nozzle_diameter", new ConfigOptionFloats{0.4, 0.4});

    // deep_compare = true routes through deep_diff() -> add_correct_opts_to_diff()
    std::vector<std::string> diff =
        PresetCollection::dirty_options(&edited, &reference, /*deep_compare=*/true);

    // The new index #1 is reported dirty even though 0.4 == values[0] (0.4).
    REQUIRE(std::find(diff.begin(), diff.end(), "nozzle_diameter#1") != diff.end());

    // Sanity: the unchanged existing index #0 is NOT reported, so the rule is
    // specific to new indices rather than flagging the whole vector.
    REQUIRE(std::find(diff.begin(), diff.end(), "nozzle_diameter#0") == diff.end());
}

#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include "libslic3r/PresetInspector.hpp"

TEST_CASE("all_eligible_parent_candidates returns eligible base presets", "[PresetInspector]")
{
    PresetCollection collection(Preset::TYPE_PRINTER, Preset::printer_options(),
                                static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()));

    std::vector<std::string> candidates = all_eligible_parent_candidates(collection);
    // Default preset is filtered out
    REQUIRE(std::find(candidates.begin(), candidates.end(), "- default -") == candidates.end());
}

TEST_CASE("export_preset_to_prusa_ini translates gcode macros and keys", "[PresetInspector]")
{
    PresetCollection collection(Preset::TYPE_PRINTER, Preset::printer_options(),
                                static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()));

    Preset p(Preset::TYPE_PRINTER, "TestPrinter");
    p.config.set_key_value("machine_start_gcode", new ConfigOptionString("M190 S[bed_temperature_initial_layer_single]\nM109 S[nozzle_temperature_initial_layer]"));
    p.config.set_key_value("outer_wall_speed", new ConfigOptionFloat(45.0));

    boost::filesystem::path temp_ini = boost::filesystem::temp_directory_path() / "orca_test_export.ini";
    auto res = export_preset_to_prusa_ini(collection, p, temp_ini.string());
    REQUIRE(res.succeeded);

    boost::nowide::ifstream in(temp_ini.string());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    boost::system::error_code ec;
    boost::filesystem::remove(temp_ini, ec);

    REQUIRE(content.find("start_gcode") != std::string::npos);
    REQUIRE(content.find("[first_layer_bed_temperature]") != std::string::npos);
    REQUIRE(content.find("[first_layer_temperature]") != std::string::npos);
    REQUIRE(content.find("external_perimeter_speed = 45") != std::string::npos);
}

TEST_CASE("deep_diff distinguishes absolute and percentage speeds for each variant", "[PresetDiff][Config]")
{
    const size_t changed_index = GENERATE(size_t(0), size_t(1));
    Preset reference(Preset::TYPE_PRINT, "ref");
    reference.config.set_key_value("small_perimeter_speed", new ConfigOptionFloatsOrPercents{{50., false}, {50., false}});

    Preset edited = reference;
    edited.config.option<ConfigOptionFloatsOrPercents>("small_perimeter_speed")->values[changed_index].percent = true;

    const auto diff = PresetCollection::dirty_options(&edited, &reference, /*deep_compare=*/true);
    REQUIRE(diff == std::vector<std::string>{"small_perimeter_speed#" + std::to_string(changed_index)});

    DynamicPrintConfig transferred = reference.config;
    transferred.apply_only(edited.config, diff);
    REQUIRE(*transferred.option("small_perimeter_speed") == *edited.config.option("small_perimeter_speed"));
}
