#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "../data/quote_row.hpp"

struct request_debug_view {
    std::string url;
    long http_status = 0;
    int curl_code = 0;
    std::string curl_error;
    std::string body_snippet;
};

enum class LogLevel { Info, Success, Error };

struct LogEntry {
    std::string text;
    LogLevel level = LogLevel::Info;
};

// sets refresh_chart_out to true if "Refresh Chart" is clicked; sets
// granularity_changed_out to true if a granularity button changed chart_interval;
// sets symbol_changed_out to true if clicking a ticker (or removing the
// selected one) changed selected_symbol
void draw_quotes_window(std::vector<std::string>& symbols,
                        bool last_request_ok,
                        const std::string& last_error,
                        const std::vector<QuoteRow>& quotes,
                        const request_debug_view* dbg,
                        std::chrono::system_clock::time_point last_sync,
                        const std::vector<LogEntry>& log_lines,
                        // chart (OHLC):
                        std::string& selected_symbol,
                        int& chart_minutes,
                        std::string& chart_interval, // Yahoo interval code, e.g. "1m", "60m", "1d"
                        bool& refresh_chart_out,
                        bool& granularity_changed_out,
                        bool& symbol_changed_out,
                        const std::vector<double>& chart_xs,
                        const std::vector<double>& chart_opens,
                        const std::vector<double>& chart_highs,
                        const std::vector<double>& chart_lows,
                        const std::vector<double>& chart_closes);
