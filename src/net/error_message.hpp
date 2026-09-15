#pragma once
#include <string>

// Turns a failed request into a short readable message, pulling the message out
// of JSON error bodies (Finnhub/Yahoo) instead of dumping the raw body.
std::string describe_http_error(long http_status, int curl_code,
                                const std::string& curl_error, const std::string& body);
