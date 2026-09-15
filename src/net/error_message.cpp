#include "error_message.hpp"
#include <cctype>
#include "../util/text.hpp"
#include "../third_party/json.hpp"
using json = nlohmann::json;

namespace {

const char* reason_phrase(long status) {
    switch (status) {
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default:  return nullptr;
    }
}

std::string string_field(const json& obj) {
    for (const char* key : {"description", "message", "detail", "error"}) {
        if (obj.contains(key) && obj[key].is_string()) return obj[key].get<std::string>();
    }
    return {};
}

std::string message_from_error(const json& e) {
    if (e.is_string()) return e.get<std::string>();
    if (e.is_object()) return string_field(e);
    return {};
}

std::string message_from_json(const json& j) {
    if (!j.is_object()) return {};
    if (j.contains("error")) {
        if (auto m = message_from_error(j["error"]); !m.empty()) return m;
    }
    if (auto m = string_field(j); !m.empty()) return m;
    // Yahoo nests errors one level down, e.g. {"chart":{"error":{"description":...}}}
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto& value = it.value();
        if (value.is_object() && value.contains("error")) {
            if (auto m = message_from_error(value["error"]); !m.empty()) return m;
        }
    }
    return {};
}

std::string message_from_body(const std::string& body) {
    const size_t a = body.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const size_t b = body.find_last_not_of(" \t\r\n");
    const std::string s = body.substr(a, b - a + 1);

    if (s[0] == '{' || s[0] == '[') {
        try { return message_from_json(json::parse(s)); } catch (...) {}
        return {};
    }
    if (s[0] == '<') return {}; // HTML error page - nothing useful to show inline

    std::string out;
    bool prev_space = false;
    for (const char c : s) {
        const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
        if (space && prev_space) continue;
        out += space ? ' ' : c;
        prev_space = space;
    }
    if (out.size() > 160) out = out.substr(0, 157) + "...";
    return out;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string describe_http_error(long http_status, int curl_code,
                                const std::string& curl_error, const std::string& body) {
    if (curl_code != 0) {
        return "Network error: " + (curl_error.empty() ? "cURL code " + std::to_string(curl_code) : curl_error);
    }

    std::string msg = "HTTP " + std::to_string(http_status);
    const char* reason = reason_phrase(http_status);
    if (reason) msg += std::string(" ") + reason;

    const std::string detail = to_ascii_punctuation(message_from_body(body));
    if (!detail.empty() && !(reason && iequals(detail, reason))) msg += ": " + detail;
    return msg;
}
