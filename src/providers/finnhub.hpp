#pragma once
#include <string>
#include <vector>
#include "../data/news_item.hpp"
#include "../data/quote_row.hpp"

std::string build_finnhub_quote_url(const std::string& symbol, const std::string& api_key);
QuoteRow parse_finnhub_quote_json(const std::string& payload, const std::string& symbol);

// Company profile (for the full company name); returns "" if unavailable.
std::string build_finnhub_profile_url(const std::string& symbol, const std::string& api_key);
std::string parse_finnhub_profile_name(const std::string& payload);

// Company news; from/to are YYYY-MM-DD.
std::string build_finnhub_company_news_url(const std::string& symbol, const std::string& api_key,
                                           const std::string& from, const std::string& to);
std::vector<NewsItem> parse_finnhub_news_json(const std::string& payload);
