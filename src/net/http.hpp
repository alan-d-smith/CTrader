#pragma once
#include <string>

struct http_result {
    bool ok = false;
    long http_status = 0;
    int curl_code = 0;
    std::string error;
    std::string body;
};

http_result http_get_ex(const std::string& url, bool verbose = false,
                        const std::string& user_agent = "ctrader/1.0");
