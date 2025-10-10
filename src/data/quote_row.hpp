#pragma once
#include <string>
#include <optional>
#include <sstream>
#include <iomanip>

struct QuoteRow {
    std::string symbol;
    std::string name;
    std::optional<double> price;
    std::optional<double> change;
    std::optional<double> change_pct;
    std::optional<double> day_low;
    std::optional<double> day_high;
    std::string currency;
};

// Fmt numbers to specified decimal places or "-" if missing
inline std::string fmt_opt(const std::optional<double>& v, int decimals = 2) {
    if (!v.has_value()) return "-";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << *v;
    return oss.str();
}
