#include "finnhub.hpp"
#include <sstream>
#include "../third_party/json.hpp"
using json = nlohmann::json;

std::string build_finnhub_quote_url(const std::string& symbol, const std::string& api_key) {
    std::ostringstream oss;
    oss << "https://finnhub.io/api/v1/quote?symbol=" << symbol << "&token=" << api_key;
    return oss.str();
}

QuoteRow parse_finnhub_quote_json(const std::string& payload, const std::string& symbol) {
    QuoteRow r;
    r.symbol = symbol;
    r.name = symbol;
    r.currency = "GBP";

    try {
        auto j = json::parse(payload);
        auto set_opt = [&](const char* key, std::optional<double>& dest) {
            if (j.contains(key) && !j[key].is_null()) dest = j[key].get<double>();
        };
        set_opt("c", r.price);
        set_opt("d", r.change);
        set_opt("dp", r.change_pct);
        set_opt("l", r.day_low);
        set_opt("h", r.day_high);
    } catch (...) {}

    return r;
}

std::string build_finnhub_profile_url(const std::string& symbol, const std::string& api_key) {
    std::ostringstream oss;
    oss << "https://finnhub.io/api/v1/stock/profile2?symbol=" << symbol << "&token=" << api_key;
    return oss.str();
}

std::string parse_finnhub_profile_name(const std::string& payload) {
    try {
        auto j = json::parse(payload);
        if (j.contains("name") && j["name"].is_string()) {
            return j["name"].get<std::string>();
        }
    } catch (...) {}
    return "";
}
