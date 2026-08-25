#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace Slic3r {

enum class DiagnosticSeverity { info, warning, error, fatal };

struct DiagnosticEvent {
    std::string                        id;
    std::string                        timestamp_utc;
    DiagnosticSeverity                 severity{DiagnosticSeverity::info};
    std::string                        code;
    std::string                        component;
    std::string                        summary;
    std::string                        technical_detail;
    std::string                        recovery_hint;
    std::map<std::string, std::string> context;
};

// Stable for the lifetime of the process and safe to show to users or include in support bundles.
const std::string &diagnostic_session_id();

// Records a structured event in the normal log and in a small in-memory history. The returned
// event ID lets a user-facing message point support directly to the corresponding log entry.
std::string record_diagnostic(
    DiagnosticSeverity                 severity,
    std::string                        code,
    std::string                        component,
    std::string                        summary,
    std::string                        technical_detail = {},
    std::string                        recovery_hint = {},
    std::map<std::string, std::string> context = {});

std::vector<DiagnosticEvent> recent_diagnostics(std::size_t maximum_count = 100);

// JSON is used instead of a human-oriented log excerpt so support tooling can consume the report.
// Sensitive context keys and URL query strings are redacted before events enter the history.
std::string diagnostics_report_json(std::size_t maximum_count = 100);

std::string diagnostic_severity_name(DiagnosticSeverity severity);

} // namespace Slic3r
