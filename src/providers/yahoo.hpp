#pragma once
#include <string>
#include <cstdint>
#include <map>
#include "../data/candles.hpp"

// interval: one of "1m","2m","5m","15m","30m","60m","1d" (Yahoo's own codes)
std::string build_yahoo_chart_url(const std::string& symbol,
                                  const std::string& interval,
                                  int64_t from_unix, int64_t to_unix);

candles parse_yahoo_chart_json(const std::string& payload);

// Company names fallback (for when Finnhub doesn't have one), batched.
std::string build_yahoo_quote_names_url(const std::string& symbols_csv);
std::map<std::string, std::string> parse_yahoo_quote_names_json(const std::string& payload);
