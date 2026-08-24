#include <catch2/catch_all.hpp>

#include "libslic3r/ConfigRegistry.hpp"

#include <algorithm>
#include <cstdint>

#include <nlohmann/json.hpp>

using namespace Slic3r;

namespace {

uint64_t fnv1a_64(const std::string &value)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : value) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

} // namespace

TEST_CASE("Normalized print configuration registry is deterministic", "[Config][Registry]")
{
    const std::string first = normalized_print_config_registry_json();
    const std::string second = normalized_print_config_registry_json();
    REQUIRE(first == second);

    const nlohmann::json registry = nlohmann::json::parse(first);
    REQUIRE(registry.at("schema_version") == 1);
    REQUIRE(registry.at("options").is_array());

    // Review gate: intentional registry changes update these two values together.
    CHECK(registry.at("options").size() == 759);
    CHECK(fnv1a_64(first) == UINT64_C(5756127831750747093));
}

TEST_CASE("Normalized registry preserves representative contracts", "[Config][Registry]")
{
    const nlohmann::json registry = nlohmann::json::parse(normalized_print_config_registry_json());
    const auto &options = registry.at("options");
    const auto find_option = [&options](const std::string &key) -> const nlohmann::json& {
        const auto it = std::find_if(options.begin(), options.end(), [&key](const nlohmann::json &entry) {
            return entry.at("key") == key;
        });
        REQUIRE(it != options.end());
        return *it;
    };

    const nlohmann::json &wall_loops = find_option("wall_loops");
    CHECK(wall_loops.at("type") == "int");
    CHECK(wall_loops.at("default") == "2");
    CHECK(wall_loops.at("bounds").at("min") == 0);

    const nlohmann::json &support_type = find_option("support_type");
    CHECK(support_type.at("type") == "enum");
    CHECK_FALSE(support_type.at("enum_values").empty());
    CHECK_FALSE(support_type.at("enum_keys").empty());

    const nlohmann::json &gcode_comments = find_option("gcode_comments");
    CHECK(gcode_comments.at("type") == "bool");
    CHECK(gcode_comments.at("cli").is_array());
    CHECK(gcode_comments.at("printer_technology") == "fff");
}
