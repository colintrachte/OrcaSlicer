#include <catch2/catch_all.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/TriangleMesh.hpp"

#include "test_helpers.hpp"

#include <string>

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
