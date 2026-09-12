#include <catch2/catch_all.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/TriangleMesh.hpp"

#include "test_helpers.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

using namespace Slic3r;
using namespace Slic3r::Test;

// The fan is held off for the first close_fan_the_first_x_layers layers, so an explicit
// fan-off command is emitted.
TEST_CASE("Fan is held off for the initial layers", "[Cooling]")
{
    const std::string gcode = slice({ cube(20) }, {
        { "cooling",                      true },
        { "close_fan_the_first_x_layers", 5 },
    });
    CHECK(gcode.find("M106 S0") != std::string::npos);
}

// The cooling pass resolves and strips its internal speed placeholders; none leak into
// the final G-code.
TEST_CASE("Cooling consumes its internal speed markers", "[Cooling]")
{
    const std::string gcode = slice({ cube(20) }, { { "layer_height", 0.2 } });
    CHECK(gcode.find(";_EXTRUDE_SET_SPEED") == std::string::npos);
}

// Regression coverage for #6738 (variable overhang/bridge fan speed): bridge_fan_speed is a
// distinct knob from overhang_fan_speed, selected via a negative-overhang_degree sentinel
// threaded from GCode.cpp through to CoolingBuffer.cpp. An earlier integration attempt clamped
// that sentinel to 0 with std::max(0, ...) before it reached the check, so bridges silently fell
// through to the overhang gradient instead of ever using bridge_fan_speed.
SCENARIO("Bridge fan speed is honored independently of the overhang fan speed", "[Cooling][FanSpeed][Bridge]")
{
    GIVEN("A bridge mesh with distinct overhang and bridge fan speeds") {
        Slic3r::Print print;
        TriangleMesh bridge_mesh = Slic3r::Test::mesh(TestMesh::bridge);
        bridge_mesh.align_to_origin();

        // overhang_fan_speed == overhang_fan_speed_max collapses the overhang gradient to a
        // single flat value (60%, S153), so if bridge_fan_speed (17%, S43) is ever ignored the
        // bridge would fall back to that same flat 60% - S43 and S153 can't both appear from the
        // gradient path alone, making this an unambiguous check.
        Slic3r::Test::init_and_process_print({ bridge_mesh }, print, {
            { "layer_height",                0.2 },
            { "initial_layer_print_height",  0.2 },
            { "cooling",                     true },
            { "enable_overhang_bridge_fan",  true },
            { "overhang_fan_speed",          60 },
            { "overhang_fan_speed_max",      60 },
            { "bridge_fan_speed",            17 },
            { "close_fan_the_first_x_layers",0 },
            { "full_fan_speed_layer",        1 }, // disable the first-layers ramp entirely
            { "fan_min_speed",               60 },
            { "fan_max_speed",               60 }, // keep the baseline layer-time fan flat too
            { "sparse_infill_density",       0 },
            { "top_shell_layers",            0 },
            { "bottom_shell_layers",         1 },
        });

        WHEN("the G-code is generated") {
            const std::string g = Slic3r::Test::gcode(print);
            THEN("a bridge_fan_speed (17% -> S43) command is emitted") {
                CHECK(g.find("M106 S43") != std::string::npos);
            }
        }
    }
}

// Regression coverage for #6738: when bridge_fan_speed is left at its default (-1, meaning
// "unset"), bridges must fall back to overhang_fan_speed - the single knob that covered both
// before this option existed - not to overhang_fan_speed_max, or every profile that customized
// overhang_fan_speed away from 100% would silently get hotter bridge cooling after upgrading.
TEST_CASE("Unset bridge fan speed falls back to the overhang fan speed, not its maximum", "[Cooling][FanSpeed][Bridge]")
{
    Slic3r::Print print;
    TriangleMesh bridge_mesh = Slic3r::Test::mesh(TestMesh::bridge);
    bridge_mesh.align_to_origin();

    Slic3r::Test::init_and_process_print({ bridge_mesh }, print, {
        { "layer_height",                 0.2 },
        { "initial_layer_print_height",   0.2 },
        { "cooling",                      true },
        { "enable_overhang_bridge_fan",   true },
        { "overhang_fan_speed",           37 },  // distinct from overhang_fan_speed_max below
        { "overhang_fan_speed_max",       90 },
        { "bridge_fan_speed",             -1 },  // default: "use overhang_fan_speed"
        { "close_fan_the_first_x_layers", 0 },
        { "full_fan_speed_layer",         1 },
        { "fan_min_speed",                37 },
        { "fan_max_speed",                37 },
        { "sparse_infill_density",        0 },
        { "top_shell_layers",             0 },
        { "bottom_shell_layers",          1 },
    });

    const std::string g = Slic3r::Test::gcode(print);
    // 37% -> S94; 90% -> S229 (255.5 * speed / 100, truncated).
    CHECK(g.find("M106 S94") != std::string::npos);
}


TEST_CASE("Overhang fan transitions do not depend on overhang speed", "[Cooling][Regression]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_deserialize_strict({
        { "bridge_speed",                   2.0 },
        { "enable_arc_fitting",             false },
        { "enable_overhang_bridge_fan",     true },
        { "enable_overhang_speed",          false },
        { "initial_layer_print_height",     0.3 },
        { "inner_wall_speed",              30.0 },
        { "layer_height",                   0.3 },
        { "outer_wall_speed",              30.0 },
        { "overhang_1_4_speed",            "30" },
        { "overhang_2_4_speed",            "29" },
        { "overhang_3_4_speed",             "6" },
        { "overhang_4_4_speed",             "3" },
        { "slow_down_for_layer_cooling",    false },
        { "slowdown_for_curled_perimeters", false },
    });
    config.set_key_value("fan_max_speed", new ConfigOptionFloats{20.0});
    config.set_key_value("fan_min_speed", new ConfigOptionFloats{20.0});
    config.set_key_value("overhang_fan_speed", new ConfigOptionInts{100});
    config.set_key_value("overhang_fan_threshold", new ConfigOptionEnumsGeneric{Overhang_threshold_2_4});
    config.set_key_value("layer_change_gcode", new ConfigOptionString{";TEST_LAYER_Z=[layer_z]"});

    const auto fan_commands = [](const std::string &gcode) {
        std::vector<std::pair<std::string, std::string>> commands;
        std::istringstream input(gcode);
        std::string layer;
        std::string line;
        while (std::getline(input, line)) {
            if (line.rfind(";TEST_LAYER_Z=", 0) == 0)
                layer = line;
            else if (!layer.empty() && (line.rfind("M106", 0) == 0 || line.rfind("M107", 0) == 0))
                commands.emplace_back(layer, line);
        }
        return commands;
    };
    const auto feedrates = [](const std::string &gcode) {
        std::vector<std::string> values;
        std::istringstream input(gcode);
        std::string word;
        while (input >> word)
            if (!word.empty() && word.front() == 'F')
                values.push_back(word);
        return values;
    };

    constexpr double sphere_radius = 50.0; // 100 mm diameter.
    const std::string without_speed_gcode = slice({make_sphere(sphere_radius, PI / 24.0)}, config);
    config.set_deserialize_strict({{"enable_overhang_speed", true}});
    const std::string with_speed_gcode = slice({make_sphere(sphere_radius, PI / 24.0)}, config);

    const auto without_speed_fan = fan_commands(without_speed_gcode);
    const auto with_speed_fan = fan_commands(with_speed_gcode);
    const auto without_speed_feedrates = feedrates(without_speed_gcode);
    const auto with_speed_feedrates = feedrates(with_speed_gcode);

    REQUIRE_FALSE(without_speed_fan.empty());
    REQUIRE(std::any_of(without_speed_fan.begin(), without_speed_fan.end(),
                        [](const auto &command) { return command.second.find("S255") != std::string::npos; }));
    REQUIRE(with_speed_feedrates != without_speed_feedrates);
    CHECK(with_speed_fan == without_speed_fan);
}
