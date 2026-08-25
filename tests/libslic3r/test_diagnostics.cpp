#include <catch2/catch_all.hpp>

#include <nlohmann/json.hpp>

#include "libslic3r/Diagnostics.hpp"

using namespace Slic3r;

TEST_CASE("diagnostics correlate events with their process session", "[Diagnostics]")
{
    const std::string id = record_diagnostic(DiagnosticSeverity::warning, "TEST-WARN-001", "tests",
                                             "A test warning", "Technical detail", "Try the safe action");
    const auto events = recent_diagnostics(1);

    REQUIRE(events.size() == 1);
    CHECK(events.front().id == id);
    CHECK_THAT(id, Catch::Matchers::StartsWith(diagnostic_session_id() + "-"));
    CHECK(events.front().code == "TEST-WARN-001");
    CHECK(events.front().severity == DiagnosticSeverity::warning);
}

TEST_CASE("diagnostics redact secrets before logging or reporting", "[Diagnostics]")
{
    record_diagnostic(DiagnosticSeverity::error, "TEST-REDACT-001", "tests", "Redaction test", {}, {},
                      {{"access_token", "do-not-keep"},
                       {"request_url", "https://example.invalid/path?token=do-not-keep"},
                       {"safe_value", "line one\nline two"}});

    const auto event = recent_diagnostics(1).front();
    CHECK(event.context.at("access_token") == "<redacted>");
    CHECK(event.context.at("request_url") == "https://example.invalid/path?<redacted>");
    CHECK(event.context.at("safe_value") == "line one line two");

    const nlohmann::json report = nlohmann::json::parse(diagnostics_report_json(1));
    CHECK(report.at("schema_version") == 1);
    CHECK(report.at("session_id") == diagnostic_session_id());
    CHECK(report.at("events").back().at("context").at("access_token") == "<redacted>");
    CHECK(report.dump().find("do-not-keep") == std::string::npos);
}
