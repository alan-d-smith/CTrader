#pragma once
#include <string>
#include <vector>
#include "../data/quote_row.hpp"

std::string build_finnhub_quote_url(const std::string& symbol, const std::string& api_key);
QuoteRow parse_finnhub_quote_json(const std::string& payload, const std::string& symbol);
