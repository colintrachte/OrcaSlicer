#include "Diagnostics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>

#include <boost/log/keywords/severity.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/trivial.hpp>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace Slic3r {
namespace {

constexpr std::size_t retained_event_count = 100;

struct DiagnosticState {
    std::mutex                  mutex;
    std::deque<DiagnosticEvent> events;
    std::atomic<unsigned long>  next_event_number{1};
};

DiagnosticState &diagnostic_state()
{
    // Diagnostics are also used during shutdown. Intentionally retain the small state object so
    // cross-translation-unit static destruction order cannot invalidate it before the last event.
    static DiagnosticState *state = new DiagnosticState;
    return *state;
}

unsigned long process_id()
{
#ifdef _WIN32
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

std::string utc_timestamp(bool compact)
{
    const auto now          = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};
#ifdef _WIN32
    gmtime_s(&utc_time, &now_time);
#else
    gmtime_r(&now_time, &utc_time);
#endif

    std::ostringstream out;
    if (compact)
        out << std::put_time(&utc_time, "%Y%m%dT%H%M%SZ");
    else
        out << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
            << milliseconds.count() << 'Z';
    return out.str();
}

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

bool is_sensitive_key(const std::string &key)
{
    const std::string normalized = lowercase(key);
    static const char *sensitive_fragments[] = {
        "access_code", "api_key", "authorization", "cookie", "credential", "password", "secret", "session_key", "token"
    };
    return std::any_of(std::begin(sensitive_fragments), std::end(sensitive_fragments),
                       [&normalized](const char *fragment) { return normalized.find(fragment) != std::string::npos; });
}

bool is_url_key(const std::string &key)
{
    const std::string normalized = lowercase(key);
    return normalized == "url" || normalized == "uri" || normalized.find("_url") != std::string::npos ||
           normalized.find("_uri") != std::string::npos;
}

std::string sanitize_value(const std::string &key, std::string value)
{
    if (is_sensitive_key(key))
        return "<redacted>";

    if (is_url_key(key)) {
        const std::size_t query = value.find('?');
        if (query != std::string::npos)
            value.replace(query, std::string::npos, "?<redacted>");
    }

    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    constexpr std::size_t maximum_value_length = 512;
    if (value.size() > maximum_value_length)
        value.replace(maximum_value_length, std::string::npos, "...<truncated>");
    return value;
}

nlohmann::json event_to_json(const DiagnosticEvent &event)
{
    return {{"id", event.id},
            {"timestamp_utc", event.timestamp_utc},
            {"severity", diagnostic_severity_name(event.severity)},
            {"code", event.code},
            {"component", event.component},
            {"summary", event.summary},
            {"technical_detail", event.technical_detail},
            {"recovery_hint", event.recovery_hint},
            {"context", event.context}};
}

boost::log::trivial::severity_level boost_severity(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::info: return boost::log::trivial::info;
    case DiagnosticSeverity::warning: return boost::log::trivial::warning;
    case DiagnosticSeverity::error: return boost::log::trivial::error;
    case DiagnosticSeverity::fatal: return boost::log::trivial::fatal;
    }
    return boost::log::trivial::error;
}

} // namespace

const std::string &diagnostic_session_id()
{
    static const std::string *id = new std::string("ORCA-" + utc_timestamp(true) + '-' + std::to_string(process_id()));
    return *id;
}

std::string diagnostic_severity_name(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::info: return "info";
    case DiagnosticSeverity::warning: return "warning";
    case DiagnosticSeverity::error: return "error";
    case DiagnosticSeverity::fatal: return "fatal";
    }
    return "error";
}

std::string record_diagnostic(DiagnosticSeverity severity, std::string code, std::string component,
                              std::string summary, std::string technical_detail, std::string recovery_hint,
                              std::map<std::string, std::string> context)
{
    DiagnosticState &state = diagnostic_state();
    const unsigned long sequence = state.next_event_number.fetch_add(1, std::memory_order_relaxed);
    DiagnosticEvent event;
    event.id               = diagnostic_session_id() + '-' + std::to_string(sequence);
    event.timestamp_utc    = utc_timestamp(false);
    event.severity         = severity;
    event.code             = sanitize_value("code", std::move(code));
    event.component        = sanitize_value("component", std::move(component));
    event.summary          = sanitize_value("summary", std::move(summary));
    event.technical_detail = sanitize_value("technical_detail", std::move(technical_detail));
    event.recovery_hint    = sanitize_value("recovery_hint", std::move(recovery_hint));
    for (auto &[key, value] : context)
        value = sanitize_value(key, std::move(value));
    event.context = std::move(context);

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.events.emplace_back(event);
        while (state.events.size() > retained_event_count)
            state.events.pop_front();
    }

    // Logging must never turn an error-reporting path into another failure. Boost.Log may already
    // be shutting down when late plugin or singleton cleanup reports a diagnostic.
    try {
        const nlohmann::json structured_event = event_to_json(event);
        BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),
            (::boost::log::keywords::severity = boost_severity(severity))) << "DIAGNOSTIC " << structured_event.dump();
    } catch (...) {
    }
    return event.id;
}

std::vector<DiagnosticEvent> recent_diagnostics(std::size_t maximum_count)
{
    DiagnosticState &state = diagnostic_state();
    std::lock_guard<std::mutex> lock(state.mutex);
    const std::size_t count = std::min(maximum_count, state.events.size());
    return {state.events.end() - static_cast<std::ptrdiff_t>(count), state.events.end()};
}

std::string diagnostics_report_json(std::size_t maximum_count)
{
    nlohmann::json report = {{"schema_version", 1},
                             {"session_id", diagnostic_session_id()},
                             {"generated_at_utc", utc_timestamp(false)},
                             {"privacy", "Sensitive context fields and URL query strings are redacted. Application logs and project files may still contain personal data; review a bundle before sharing."},
                             {"events", nlohmann::json::array()}};
    for (const DiagnosticEvent &event : recent_diagnostics(maximum_count))
        report["events"].push_back(event_to_json(event));
    return report.dump(2);
}

} // namespace Slic3r
