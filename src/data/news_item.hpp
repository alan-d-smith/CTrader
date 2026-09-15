#pragma once
#include <string>
#include <cstdint>

struct NewsItem {
    std::string headline;
    std::string summary;
    std::string source;    // aggregator Finnhub ingested from - often just "Yahoo"
    std::string publisher; // real outlet, resolved by following the article redirect
    std::string url;
    int64_t datetime = 0;  // unix seconds
};

// "https://www.fool.com/investing/x" -> "fool.com"
inline std::string publisher_from_url(const std::string& url) {
    const size_t scheme = url.find("://");
    const size_t start = (scheme == std::string::npos) ? 0 : scheme + 3;
    size_t end = url.find('/', start);
    if (end == std::string::npos) end = url.size();

    std::string host = url.substr(start, end - start);
    if (const size_t at = host.find('@'); at != std::string::npos) host = host.substr(at + 1);
    if (const size_t colon = host.find(':'); colon != std::string::npos) host = host.substr(0, colon);
    if (host.rfind("www.", 0) == 0) host = host.substr(4);
    return host;
}
