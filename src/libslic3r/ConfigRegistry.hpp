#pragma once

#include <string>

namespace Slic3r {

class ConfigDef;

// Returns a deterministic, nonlocalized JSON view suitable for compatibility
// fixtures and registry diffs. This is intentionally read-only.
std::string normalized_config_registry_json(const ConfigDef &registry);
std::string normalized_print_config_registry_json();

} // namespace Slic3r
