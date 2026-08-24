#include "ConfigRegistry.hpp"

#include "Config.hpp"
#include "PrintConfig.hpp"

#include <cfloat>

#include <nlohmann/json.hpp>

namespace Slic3r {
namespace {

const char *config_option_type_name(ConfigOptionType type)
{
    switch (type) {
    case coNone:              return "none";
    case coFloat:             return "float";
    case coFloats:            return "floats";
    case coInt:               return "int";
    case coInts:              return "ints";
    case coString:            return "string";
    case coStrings:           return "strings";
    case coPercent:           return "percent";
    case coPercents:          return "percents";
    case coFloatOrPercent:    return "float_or_percent";
    case coFloatsOrPercents:  return "floats_or_percents";
    case coPoint:             return "point";
    case coPoints:            return "points";
    case coPoint3:            return "point3";
    case coBool:              return "bool";
    case coBools:             return "bools";
    case coEnum:              return "enum";
    case coEnums:             return "enums";
    case coPointsGroups:      return "point_groups";
    case coIntsGroups:        return "int_groups";
    }
    throw ConfigurationError("Unknown configuration option type while exporting the registry.");
}

const char *printer_technology_name(PrinterTechnology technology)
{
    switch (technology) {
    case ptFFF:     return "fff";
    case ptSLA:     return "sla";
    case ptUnknown: return "unknown";
    case ptAny:     return "any";
    }
    throw ConfigurationError("Unknown printer technology while exporting the registry.");
}

nlohmann::json normalized_option(const std::string &key, const ConfigOptionDef &definition)
{
    nlohmann::json enum_keys = nlohmann::json::array();
    if (definition.enum_keys_map != nullptr)
        for (const auto &[name, value] : *definition.enum_keys_map)
            enum_keys.push_back({ { "name", name }, { "value", value } });

    nlohmann::json bounds = {
        { "min", definition.min <= -FLT_MAX ? nlohmann::json(nullptr) : nlohmann::json(definition.min) },
        { "max", definition.max >=  FLT_MAX ? nlohmann::json(nullptr) : nlohmann::json(definition.max) }
    };

    return {
        { "aliases", definition.aliases },
        { "bounds", std::move(bounds) },
        { "cli", definition.cli_args(key) },
        { "default", definition.default_value ? definition.default_value->serialize() : std::string() },
        { "enum_keys", std::move(enum_keys) },
        { "enum_values", definition.enum_values },
        { "key", key },
        { "mode", int(definition.mode) },
        { "nullable", definition.nullable },
        { "printer_technology", printer_technology_name(definition.printer_technology) },
        { "ratio_over", definition.ratio_over },
        { "serialization_ordinal", definition.serialization_key_ordinal },
        { "type", config_option_type_name(definition.type) }
    };
}

} // namespace

std::string normalized_config_registry_json(const ConfigDef &registry)
{
    nlohmann::json options = nlohmann::json::array();
    for (const auto &[key, definition] : registry.options)
        options.push_back(normalized_option(key, definition));
    return nlohmann::json({ { "options", std::move(options) }, { "schema_version", 1 } }).dump(2);
}

std::string normalized_print_config_registry_json()
{
    return normalized_config_registry_json(print_config_def);
}

} // namespace Slic3r
