#pragma once
#include <string>
#include <vector>
#include "../data/quote_row.hpp"

struct request_debug_view {
    std::string url;
    long http_status = 0;
    int curl_code = 0;
    std::string curl_error;
    std::string body_snippet;
};

// returns true if "Fetch Now" is pressed
bool draw_quotes_window(std::string& symbols_csv,
                        float& refresh_seconds,
                        bool& auto_refresh,
                        bool last_request_ok,
                        const std::string& last_error,
                        const std::vector<QuoteRow>& quotes,
                        std::string& api_key,
                        bool& reveal_api_key,
                        const request_debug_view* dbg);
