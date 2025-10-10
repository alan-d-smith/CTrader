#include "http.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <string>
#include <filesystem>

static size_t curl_write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

http_result http_get_ex(const std::string& url, bool verbose) {
    http_result r;
    CURL* curl = curl_easy_init();
    if (!curl) { r.error = "curl_easy_init failed"; return r; }

    char errbuf[CURL_ERROR_SIZE] = {};
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ctrader/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    if (verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    const char* env_bundle = std::getenv("CURL_CA_BUNDLE");
    if (env_bundle && std::filesystem::exists(env_bundle)) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, env_bundle);
    } else {
        // Try local certs/cacert.pem
        std::filesystem::path ca_local = std::filesystem::current_path() / "cacert.pem";
        if (!exists(ca_local)) {
            ca_local = std::filesystem::current_path() / "certs" / "cacert.pem";
        }
        if (exists(ca_local)) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, ca_local.string().c_str());
        }
    }

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    r.curl_code = static_cast<int>(res);
    r.http_status = code;
    r.body = std::move(response);
    if (res != CURLE_OK)
        r.error = errbuf[0] ? errbuf : curl_easy_strerror(res);
    r.ok = res == CURLE_OK && code >= 200 && code < 300;
    return r;
}
