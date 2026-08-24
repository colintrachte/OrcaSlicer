#include <catch2/catch_all.hpp>

#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/Layer.hpp"

#include "test_helpers.hpp" // get access to init_print, etc

#include <cctype>
#include <set>

using namespace Slic3r::Test;
using namespace Slic3r;

namespace {

TriangleMesh organic_support_cantilever()
{
    TriangleMesh pillar = make_cube(6., 6., 10.);
    TriangleMesh overhang = make_cube(30., 20., 4.);
    overhang.translate(-12., -7., 10.);
    pillar.merge(overhang);
    return pillar;
}

struct SupportLayerSummary
{
    long long print_z_microns;
    long long island_area_scaled;
    size_t island_count;
    bool has_extrusions;
    SupportInnerType support_type;

    bool operator==(const SupportLayerSummary &rhs) const
    {
        return print_z_microns == rhs.print_z_microns &&
               island_area_scaled == rhs.island_area_scaled &&
               island_count == rhs.island_count &&
               has_extrusions == rhs.has_extrusions &&
               support_type == rhs.support_type;
    }
};

std::vector<SupportLayerSummary> summarize_support_layers(const Print &print)
{
    std::vector<SupportLayerSummary> summary;
    for (const SupportLayer *layer : print.objects().front()->support_layers()) {
        double island_area = 0.;
        for (const ExPolygon &island : layer->support_islands)
            island_area += island.area();
        summary.push_back({
            std::llround(layer->print_z * 1000.),
            std::llround(island_area),
            layer->support_islands.size(),
            layer->has_extrusions(),
            layer->support_type
        });
    }
    return summary;
}

void process_organic_support_cantilever(Print &print)
{
    Slic3r::Test::init_and_process_print({ organic_support_cantilever() }, print, {
        { "enable_support",       1 },
        { "support_type",         "tree(auto)" },
        { "support_style",        "organic" },
        { "dont_support_bridges", false },
        { "support_on_build_plate_only", true },
        { "support_interface_top_layers", 3 },
        { "support_top_z_distance", 0.0 },
        { "layer_height",         0.2 },
        { "initial_layer_print_height", 0.2 }
    });
}

struct SupportConnectivityResult
{
    bool   connected { false };
    double failed_print_z { 0. };
    double allowed_step { 0. };
    double required_step { 0. };
};

SupportConnectivityResult support_islands_connect_to_buildplate(const Print &print)
{
    const PrintObject &object = *print.objects().front();
    Polygons reachable;
    double   previous_z = 0.;
    bool     found_base = false;

    for (const SupportLayer *layer : object.support_layers()) {
        if (layer->support_islands.empty())
            continue;

        const Polygons layer_polygons = to_polygons(layer->support_islands);
        if (! found_base) {
            if (layer->print_z != Catch::Approx(print.config().initial_layer_print_height.value))
                return {};
            reachable = union_(layer_polygons);
            previous_z = layer->print_z;
            found_base = true;
            continue;
        }

        // Interface roofs may bridge from neighboring organic nodes, so the connectivity graph
        // includes the configured node spacing and branch radius as well as per-layer branch motion.
        const double max_horizontal_step =
            std::tan(object.config().tree_support_branch_angle_organic.value * M_PI / 180.) *
            (layer->print_z - previous_z) +
            object.config().tree_support_branch_distance_organic.value +
            0.5 * object.config().tree_support_branch_diameter_organic.value + 0.05;
        const Polygons reachable_on_layer = offset(reachable, scale_(max_horizontal_step));
        Polygons       next_reachable;
        for (const ExPolygon &island : layer->support_islands) {
            const Polygons island_polygons = to_polygons(ExPolygons{ island });
            if (intersection(island_polygons, reachable_on_layer).empty()) {
                double required_step = max_horizontal_step;
                while (required_step < 10. && intersection(island_polygons, offset(reachable, scale_(required_step))).empty())
                    required_step += 0.05;
                return { false, layer->print_z, max_horizontal_step, required_step };
            }
            append(next_reachable, island_polygons);
        }
        reachable = union_(next_reachable);
        previous_z = layer->print_z;
    }
    return { found_base };
}

std::set<int> tools_for_role(const std::string &gcode, const std::string &role)
{
    std::set<int> tools;
    int current_tool = 0;
    GCodeReader reader;
    reader.parse_buffer(gcode, [&](GCodeReader &self, const GCodeReader::GCodeLine &line) {
        const std::string command(line.cmd());
        if (command.size() >= 2 && command[0] == 'T' && std::isdigit(static_cast<unsigned char>(command[1])))
            current_tool = std::stoi(command.substr(1));
        else if (line.extruding(self) && std::string(line.comment()).find(role) != std::string::npos)
            tools.insert(current_tool);
    });
    return tools;
}

} // namespace

TEST_CASE("Three raft layers are created", "[SupportMaterial]")
{
	Slic3r::Print print;
	Slic3r::Test::init_and_process_print({ cube(20) }, print, {
        { "enable_support", 1 },
        { "raft_layers",    3 }
		});
    REQUIRE(print.objects().front()->support_layers().size() == 3);
}

TEST_CASE("Enforced support layers are generated", "[SupportMaterial]")
{
    // enforce_support_layers forces support on the first N layers even with support off.
    Slic3r::Print baseline;
    Slic3r::Test::init_and_process_print({ TestMesh::overhang }, baseline, {
        { "enable_support",         0 },
        { "enforce_support_layers", 0 }
    });
    REQUIRE(baseline.objects().front()->support_layers().empty());

    Slic3r::Print enforced;
    Slic3r::Test::init_and_process_print({ TestMesh::overhang }, enforced, {
        { "enable_support",         0 },
        { "enforce_support_layers", 100 }
    });
    REQUIRE(enforced.objects().front()->support_layers().size() > 0);
}

TEST_CASE("Organic support for a generated cantilever is valid and deterministic", "[SupportMaterial][TreeSupport]")
{
    Print first;
    process_organic_support_cantilever(first);

    CHECK(is_tree(first.objects().front()->config().support_type.value));
    CHECK(first.objects().front()->config().support_style.value == smsTreeOrganic);
    const ConstSupportLayerPtrsAdaptor support_layers = first.objects().front()->support_layers();
    REQUIRE_FALSE(support_layers.empty());
    CHECK(support_layers.front()->print_z == Catch::Approx(first.config().initial_layer_print_height.value));

    size_t island_count = 0;
    size_t extrusion_layer_count = 0;
    double previous_print_z = 0.;
    for (const SupportLayer *layer : support_layers) {
        CHECK(layer->height > 0.);
        CHECK(layer->print_z > previous_print_z);
        previous_print_z = layer->print_z;
        extrusion_layer_count += layer->has_extrusions() ? 1 : 0;
        island_count += layer->support_islands.size();
        for (const ExPolygon &island : layer->support_islands) {
            CHECK(island.is_valid());
            CHECK(island.area() > 0.);
        }
    }
    CHECK(island_count > 0);
    CHECK(extrusion_layer_count > 0);
    const SupportConnectivityResult connectivity = support_islands_connect_to_buildplate(first);
    CAPTURE(connectivity.failed_print_z, connectivity.allowed_step, connectivity.required_step);
    CHECK(connectivity.connected);

    const TreeSupportGenerationDiagnostics first_diagnostics = first.objects().front()->tree_support_diagnostics();
    CAPTURE(first_diagnostics.pending_roof_recovery_tips,
            first_diagnostics.recovered_roof_slices,
            first_diagnostics.lost_branch_nodes,
            first_diagnostics.very_lost_branch_nodes,
            first_diagnostics.discarded_branch_meshes);
    CHECK(first_diagnostics.pending_roof_recovery_tips == 0);
    CHECK(first_diagnostics.recovered_roof_slices == 0);
    CHECK(first_diagnostics.lost_branch_nodes == 0);
    CHECK(first_diagnostics.very_lost_branch_nodes == 0);
    CHECK(first_diagnostics.discarded_branch_meshes == 247);

    Print repeated;
    process_organic_support_cantilever(repeated);
    CHECK(summarize_support_layers(first) == summarize_support_layers(repeated));
    CHECK(first_diagnostics == repeated.objects().front()->tree_support_diagnostics());
}

TEST_CASE("Organic support keeps base and interface filament assignments", "[SupportMaterial][TreeSupport][MultiFilament]")
{
    const std::string gcode = slice({ organic_support_cantilever() }, multifilament_config(2, {
        { "enable_support",               1 },
        { "support_type",                 "tree(auto)" },
        { "support_style",                "organic" },
        { "dont_support_bridges",         false },
        { "support_filament",             1 },
        { "support_interface_filament",   2 },
        { "support_interface_top_layers", 3 },
        { "skirt_loops",                  0 },
        { "brim_type",                    "no_brim" }
    }));

    CHECK(tools_for_role(gcode, "support material") == std::set<int>{ 0, 1 });
    CHECK(tools_for_role(gcode, "support material interface") == std::set<int>{ 1 });
}

SCENARIO("Support layer Z honors contact distance", "[SupportMaterial]")
{
    // Box h = 20mm, hole bottom at 5mm, hole height 10mm (top edge at 15mm).
    TriangleMesh mesh = Slic3r::Test::mesh(Slic3r::Test::TestMesh::cube_with_hole);
    mesh.rotate_x(float(M_PI / 2));

	auto check = [](Slic3r::Print &print, bool &first_support_layer_height_ok, bool &layer_height_minimum_ok, bool &layer_height_maximum_ok)
	{
        ConstSupportLayerPtrsAdaptor support_layers = print.objects().front()->support_layers();

		first_support_layer_height_ok = support_layers.front()->print_z == print.config().initial_layer_print_height.value;

		layer_height_minimum_ok = true;
		layer_height_maximum_ok = true;
		double min_layer_height = print.config().min_layer_height.values.front();
		double max_layer_height = print.config().nozzle_diameter.values.front();
		if (print.config().max_layer_height.values.front() > EPSILON)
			max_layer_height = std::min(max_layer_height, print.config().max_layer_height.values.front());
		for (size_t i = 1; i < support_layers.size(); ++ i) {
			if (support_layers[i]->print_z - support_layers[i - 1]->print_z < min_layer_height - EPSILON)
				layer_height_minimum_ok = false;
			if (support_layers[i]->print_z - support_layers[i - 1]->print_z > max_layer_height + EPSILON)
				layer_height_maximum_ok = false;
		}
	};

    GIVEN("A print object having one modelObject") {
        WHEN("Layer height = 0.2 and first layer height = 0.4") {
			Slic3r::Print print;
			Slic3r::Test::init_and_process_print({ mesh }, print, {
                { "enable_support",             1 },
                { "layer_height",               0.2 },
                { "initial_layer_print_height", 0.4 },
                { "dont_support_bridges",       false },
			});
			bool first_layer_ok, layer_min_ok, layer_max_ok;
            check(print, first_layer_ok, layer_min_ok, layer_max_ok);
            THEN("First layer height is honored")			{ REQUIRE(first_layer_ok == true); }
            THEN("No null or negative support layers")		{ REQUIRE(layer_min_ok == true); }
            THEN("No layers thicker than nozzle diameter")	{ REQUIRE(layer_max_ok == true); }
        }
        WHEN("Layer height = 0.2 and first layer height = 0.3") {
			Slic3r::Print print;
			Slic3r::Test::init_and_process_print({ mesh }, print, {
                { "enable_support",             1 },
                { "layer_height",               0.2 },
                { "initial_layer_print_height", 0.3 },
                { "dont_support_bridges",       false },
            });
            bool first_layer_ok, layer_min_ok, layer_max_ok;
            check(print, first_layer_ok, layer_min_ok, layer_max_ok);
            THEN("First layer height is honored")			{ REQUIRE(first_layer_ok == true); }
            THEN("No null or negative support layers")		{ REQUIRE(layer_min_ok == true); }
            THEN("No layers thicker than nozzle diameter")	{ REQUIRE(layer_max_ok == true); }
        }
    }
}
