#pragma once
#include <string>

struct http_result {
    bool ok = false;
    long http_status = 0;
    int curl_code = 0;
    std::string error;
    std::string body;
    std::string effective_url; // final URL after any redirects
};

// max_body_bytes > 0 aborts the transfer once that much body has arrived, which
// is enough to follow redirects (see effective_url) without downloading a whole
// page. Aborting this way reports a cURL write error, so ok/error are only
// meaningful for uncapped requests.
http_result http_get_ex(const std::string& url, bool verbose = false,
                        const std::string& user_agent = "ctrader/1.0",
                        size_t max_body_bytes = 0);
