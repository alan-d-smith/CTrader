#pragma once
#include <string>
#include "../data/quote_row.hpp"

std::string build_finnhub_quote_url(const std::string& symbol, const std::string& api_key);
QuoteRow parse_finnhub_quote_json(const std::string& payload, const std::string& symbol);

// Company profile (for the full company name); returns "" if unavailable.
std::string build_finnhub_profile_url(const std::string& symbol, const std::string& api_key);
std::string parse_finnhub_profile_name(const std::string& payload);
