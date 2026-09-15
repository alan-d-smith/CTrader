#include <cstdio>
#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <ctime>

#include <curl/curl.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifdef _WIN32
  #include <windows.h>
  #include <dwmapi.h>
  #pragma comment(lib, "dwmapi.lib")
  #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
    #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
  #endif
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb/stb_image.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"
#include "implot.h"

#include "data/candles.hpp"
#include "data/news_item.hpp"
#include "data/quote_row.hpp"
#include "net/error_message.hpp"
#include "net/http.hpp"
#include "providers/finnhub.hpp"
#include "providers/yahoo.hpp"
#include "ui/price_chart.hpp"
#include "ui/quotes_window.hpp"
#include "util/dotenv.hpp"
#include "util/keyring.hpp"

void apply_dark_titlebar(GLFWwindow* window) {
#ifdef _WIN32
    if (const HWND hwnd = glfwGetWin32Window(window)) {
        constexpr BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }
#endif
}

void apply_window_icon(GLFWwindow* window, const char* icon_path)
{
    if (!window || !icon_path) return;

    int w = 0, h = 0, channels = 0;

    unsigned char* pixels = stbi_load(icon_path, &w, &h, &channels, 4);
    if (!pixels) {
        std::fprintf(stderr, "[WARN] Failed to load icon: %s\n", icon_path);
        return;
    }

    GLFWimage img;
    img.width  = w;
    img.height = h;
    img.pixels = pixels;

    glfwSetWindowIcon(window, 1, &img);
    stbi_image_free(pixels);
}

int main() {
    load_dotenv();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialise GLFW
    if (!glfwInit()) { std::fprintf(stderr, "Failed to initialise GLFW\n"); return 1; }
    GLFWwindow* window = glfwCreateWindow(1000, 520, "CTrader", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    apply_dark_titlebar(window);
    apply_window_icon(window, "assets/icon_32.png");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // ---- App state (guarded by state_mutex; read/written from both the UI
    // thread and the background network thread) ----
    std::mutex state_mutex;

    // Default symbols: CTRADER_SYMBOLS (env/.env, comma-separated) if set,
    // otherwise a built-in fallback list.
    std::vector<std::string> symbols = []{
        std::vector<std::string> result;
        if (const char* e = std::getenv("CTRADER_SYMBOLS"); e && *e) {
            std::string s(e);
            size_t start = 0;
            while (start <= s.size()) {
                const size_t comma = s.find(',', start);
                const std::string tok = s.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                const size_t a = tok.find_first_not_of(" \t\r\n");
                const size_t b = tok.find_last_not_of(" \t\r\n");
                if (a != std::string::npos) result.push_back(tok.substr(a, b - a + 1));
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        if (result.empty()) {
            result = {"JPM", "GS", "NVDA", "AMD", "MSFT", "GOOGL", "NFLX", "TSLA"};
        }
        return result;
    }();

    bool last_request_ok = true;
    std::string last_error;
    std::vector<QuoteRow> quotes;

    // Persistent history of status changes (errors, recoveries) - unlike
    // last_error/last_request_ok, entries here are never overwritten, only appended.
    std::vector<LogEntry> log_lines;

    // Full company names, resolved once per symbol (Finnhub profile, falling
    // back to Yahoo) and never re-fetched unless the symbol is removed and re-added.
    std::map<std::string, std::string> symbol_names;
    auto last_sync = std::chrono::system_clock::time_point{}; // epoch == never synced
    std::string selected_symbol = symbols.empty() ? std::string() : symbols.front();
    int chart_minutes = 7 * 24 * 60; // 7 days
    std::string chart_interval = "60m"; // Yahoo interval code
    std::vector<double> chart_xs;
    std::vector<double> chart_opens;
    std::vector<double> chart_highs;
    std::vector<double> chart_lows;
    std::vector<double> chart_closes;

    // Every granularity fetched together and cached so switching between them
    // is instant; keyed by Yahoo interval code. Valid only for cache_symbol
    // and cache_minutes - a symbol or duration change invalidates it.
    std::map<std::string, candles> chart_cache;
    std::string cache_symbol;
    int cache_minutes = -1;

    // Company news for whichever symbol is charted.
    std::vector<NewsItem> news;
    bool news_loading = false;

    // Appends a timestamped, leveled line to log_lines, capped at 200 entries.
    // Caller must already hold state_mutex (this never locks itself).
    auto append_log = [&](const std::string& msg, LogLevel level = LogLevel::Info,
                          const request_debug_view* dbg = nullptr) {
        const std::time_t t = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &t);
        char ts[16];
        std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        LogEntry entry{std::string("[") + ts + "] " + msg, level};
        if (dbg) {
            entry.has_debug = true;
            entry.debug = *dbg;
        }
        log_lines.push_back(std::move(entry));
        if (log_lines.size() > 200) log_lines.erase(log_lines.begin());
    };

    // Tracked only by the background worker thread (do_fetch and
    // fetch_all_granularities both run there, never concurrently), so these
    // need no locking of their own.
    bool prev_quotes_ok = true;
    bool prev_chart_ok = true;

    // Article URL -> real outlet, resolved by following the redirect. An empty
    // value means "tried and gave up", so it isn't retried. Worker thread only.
    std::map<std::string, std::string> publisher_cache;

    // API key: env/.env first, then fall back to the OS keyring. Read-only
    // after this point, so no locking is needed to access it from either thread.
    const std::string api_key = []{
        const char* e = std::getenv("FINNHUB_TOKEN");
        if (e && *e) return std::string(e);
        return keyring::load_secret("CTrader", "finnhub_token").value_or(std::string());
    }();

    // Auto-refresh interval (env-only, seconds)
    const float refresh_seconds = []{
        const char* e = std::getenv("CTRADER_REFRESH_SECONDS");
        if (!e) return 5.0f;
        const float v = std::strtof(e, nullptr);
        return v >= 1.0f ? v : 5.0f;
    }();

    const bool curl_verbose = std::getenv("CTRADER_CURL_VERBOSE") != nullptr;

    // Signalled by the UI thread; consumed by the worker thread.
    std::atomic<bool> running{true};
    std::atomic<bool> refresh_chart_requested{true}; // true so the chart loads on startup
    std::atomic<bool> refresh_news_requested{true};  // likewise for the news column

    auto do_fetch = [&]
    {
        std::vector<std::string> syms_copy;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            syms_copy = symbols;
        }

        if (api_key.empty()) {
            std::lock_guard<std::mutex> lk(state_mutex);
            last_request_ok = false;
            last_error = "Finnhub API key missing";
            return;
        }

        std::vector<QuoteRow> new_quotes;
        bool ok_all = true;
        bool transient = false; // rate-limited / temporarily unauthorized: retry silently
        std::string err_msg;
        request_debug_view dbg_local;

        for (const auto& sym : syms_copy) {
            if (sym.empty()) continue;

            auto url = build_finnhub_quote_url(sym, api_key);
            auto [ok, http_status, curl_code, error, body, effective_url] = http_get_ex(url, curl_verbose);

            dbg_local.url = url;
            dbg_local.http_status = http_status;
            dbg_local.curl_code = curl_code;
            dbg_local.curl_error = error;
            dbg_local.body_snippet = (body.size() > 2000) ? body.substr(0, 2000) + "...(truncated)" : body;

            if (!ok) {
                ok_all = false;
                if (http_status == 403 || http_status == 429) {
                    transient = true;
                } else {
                    err_msg = describe_http_error(http_status, curl_code, error, body);
                }
                break;
            }

            QuoteRow row = parse_finnhub_quote_json(body, sym);
            if (!row.price.has_value()) {
                ok_all = false;
                err_msg = "Finnhub: no price in payload";
                break;
            }
            new_quotes.push_back(std::move(row));
        }

        std::lock_guard<std::mutex> lk(state_mutex);
        if (ok_all) {
            for (auto& row : new_quotes) {
                if (const auto it = symbol_names.find(row.symbol); it != symbol_names.end()) {
                    row.name = it->second;
                }
            }
            quotes = std::move(new_quotes);
            last_request_ok = true;
            last_error.clear();
            last_sync = std::chrono::system_clock::now();
            if (!prev_quotes_ok) append_log("Quotes sync recovered.", LogLevel::Success);
            prev_quotes_ok = true;
        } else if (transient) {
            // Keep whatever quotes we already have on screen; no error shown.
            last_request_ok = true;
            last_error.clear();
        } else {
            last_request_ok = false;
            last_error = err_msg;
            if (prev_quotes_ok) append_log("Quotes error: " + err_msg, LogLevel::Error, &dbg_local);
            prev_quotes_ok = false;
        }
    };

    static const std::string kYahooUserAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";
    static const std::vector<std::string> kAllIntervals = {
        "1m", "5m", "15m", "30m", "60m", "1d", "1wk", "1mo"
    };

    // Resolves the full company name for any symbol that doesn't have one yet:
    // Finnhub's profile endpoint first, falling back to a batched Yahoo quote
    // lookup for whatever Finnhub couldn't provide. Cheap no-op once every
    // symbol currently tracked has been resolved.
    auto resolve_missing_names = [&] {
        std::vector<std::string> missing;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            for (const auto& sym : symbols) {
                if (!sym.empty() && symbol_names.find(sym) == symbol_names.end()) missing.push_back(sym);
            }
        }
        if (missing.empty()) return;

        std::map<std::string, std::string> resolved;
        std::vector<std::string> still_missing;

        for (const auto& sym : missing) {
            std::string name;
            if (!api_key.empty()) {
                const auto R = http_get_ex(build_finnhub_profile_url(sym, api_key), curl_verbose);
                if (R.ok) name = parse_finnhub_profile_name(R.body);
            }
            if (!name.empty()) resolved[sym] = name;
            else still_missing.push_back(sym);
        }

        if (!still_missing.empty()) {
            std::string joined;
            for (size_t i = 0; i < still_missing.size(); ++i) {
                if (i) joined += ",";
                joined += still_missing[i];
            }
            const auto R = http_get_ex(build_yahoo_quote_names_url(joined), curl_verbose, kYahooUserAgent);
            if (R.ok) {
                for (auto& [sym, name] : parse_yahoo_quote_names_json(R.body)) resolved[sym] = name;
            }
        }

        std::lock_guard<std::mutex> lk(state_mutex);
        for (auto& [sym, name] : resolved) symbol_names[sym] = name;
        // Stop retrying symbols that failed both lookups by falling back to the ticker itself.
        for (const auto& sym : missing) {
            if (symbol_names.find(sym) == symbol_names.end()) symbol_names[sym] = sym;
        }

        for (auto& row : quotes) {
            if (const auto it = symbol_names.find(row.symbol); it != symbol_names.end()) {
                row.name = it->second;
            }
        }
    };

    // Fetches every granularity for the current symbol/duration, but the
    // currently selected one always goes first and is applied to the display
    // immediately - the rest are fetched afterwards purely to populate the
    // cache, without making the visible chart wait on them.
    auto fetch_all_granularities = [&] {
        std::string first_sym;
        int minutes;
        std::string selected_interval;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            first_sym = selected_symbol;
            minutes = chart_minutes;
            selected_interval = chart_interval;
        }

        if (first_sym.empty()) return;

        const int64_t to_unix   = static_cast<int64_t>(std::time(nullptr));
        const int64_t from_unix = to_unix - static_cast<int64_t>(minutes) * 60;

        std::vector<std::string> order = {selected_interval};
        for (const auto& interval : kAllIntervals) {
            if (interval != selected_interval) order.push_back(interval);
        }

        for (const auto& interval : order) {
            const std::string url = build_yahoo_chart_url(first_sym, interval, from_unix, to_unix);
            const auto R = http_get_ex(url, curl_verbose, kYahooUserAgent);

            bool ok_here = false;
            std::string err_here;
            candles C;

            if (!R.ok) {
                if (R.http_status != 429) {
                    err_here = describe_http_error(R.http_status, R.curl_code, R.error, R.body);
                }
            } else {
                C = parse_yahoo_chart_json(R.body);
                if (C.ok && !C.t.empty()) {
                    ok_here = true;
                } else {
                    err_here = C.err.empty() ? std::string("no data for this range") : C.err;
                }
            }

            std::lock_guard<std::mutex> lk(state_mutex);
            // Bail if the symbol/duration moved on while this was in flight,
            // so a late-arriving response can't clobber a newer selection.
            if (selected_symbol != first_sym || chart_minutes != minutes) return;

            cache_symbol = first_sym;
            cache_minutes = minutes;
            if (ok_here) chart_cache[interval] = C;

            if (interval == selected_interval) {
                request_debug_view dbg_local;
                dbg_local.url = url;
                dbg_local.http_status = R.http_status;
                dbg_local.curl_code = R.curl_code;
                dbg_local.curl_error = R.error;
                dbg_local.body_snippet = R.body.size() > 2000 ? R.body.substr(0,2000)+"...(truncated)" : R.body;

                if (ok_here) {
                    chart_xs = std::move(C.t);
                    chart_opens = std::move(C.o);
                    chart_highs = std::move(C.h);
                    chart_lows = std::move(C.l);
                    chart_closes = std::move(C.c);
                    last_request_ok = true;
                    last_error.clear();
                    if (!prev_chart_ok) append_log("Chart sync recovered.", LogLevel::Success);
                    prev_chart_ok = true;
                } else if (!err_here.empty()) {
                    last_request_ok = false;
                    last_error = err_here;
                    if (prev_chart_ok) append_log("Chart error (" + interval + "): " + err_here, LogLevel::Error, &dbg_local);
                    prev_chart_ok = false;
                }
                // else: selected interval was rate-limited; stay silent, keep
                // whatever was already on screen.
            }
        }
    };

    // Company news for the charted symbol. Re-run when the symbol changes and
    // periodically, since headlines go stale far slower than quotes.
    auto fetch_news = [&] {
        std::string sym;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            sym = selected_symbol;
            if (sym.empty()) {
                news.clear();
                news_loading = false;
                return;
            }
            news_loading = true;
        }

        if (api_key.empty()) {
            std::lock_guard<std::mutex> lk(state_mutex);
            news_loading = false;
            return;
        }

        auto fmt_date = [](std::time_t t) {
            std::tm tmv{};
            localtime_s(&tmv, &t);
            char buf[16];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmv);
            return std::string(buf);
        };
        const std::time_t to_t = std::time(nullptr);
        const std::time_t from_t = to_t - 14 * 24 * 60 * 60;

        const std::string url = build_finnhub_company_news_url(sym, api_key, fmt_date(from_t), fmt_date(to_t));
        const auto R = http_get_ex(url, curl_verbose);

        std::vector<NewsItem> items;
        std::string err;
        if (R.ok) {
            items = parse_finnhub_news_json(R.body);
            if (items.size() > 40) items.resize(40);
        } else if (R.http_status != 429) {
            err = describe_http_error(R.http_status, R.curl_code, R.error, R.body);
        }

        std::lock_guard<std::mutex> lk(state_mutex);
        news_loading = false;
        if (selected_symbol != sym) return; // user moved on while this was in flight

        if (R.ok) {
            news = std::move(items);
        } else if (!err.empty()) {
            request_debug_view dbg_local;
            dbg_local.url = url;
            dbg_local.http_status = R.http_status;
            dbg_local.curl_code = R.curl_code;
            dbg_local.curl_error = R.error;
            dbg_local.body_snippet = R.body.size() > 2000 ? R.body.substr(0, 2000) + "...(truncated)" : R.body;
            append_log("News error (" + sym + "): " + err, LogLevel::Error, &dbg_local);
        }
    };

    // Finnhub reports the aggregator ("Yahoo") rather than who actually wrote
    // the piece; the real outlet only shows up if the article link is followed.
    // Each one is a redirect round-trip that can take seconds, so only a couple
    // are resolved per tick and results are cached.
    auto resolve_news_publishers = [&] {
        std::vector<std::string> todo;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            for (auto& item : news) {
                if (!item.publisher.empty() || item.url.empty()) continue;

                const auto cached = publisher_cache.find(item.url);
                if (cached != publisher_cache.end()) {
                    if (!cached->second.empty()) item.publisher = cached->second;
                    continue; // empty cache entry: already tried and failed
                }
                todo.push_back(item.url);
                if (todo.size() >= 2) break;
            }
        }
        if (todo.empty()) return;

        for (const auto& url : todo) {
            // GET, not HEAD: Finnhub's link doesn't honour HEAD and loops back
            // to finnhub.io. The byte cap stops it downloading whole articles.
            const auto R = http_get_ex(url, curl_verbose, kYahooUserAgent, /*max_body_bytes=*/16 * 1024);

            // A 403 (or the capped-transfer abort) still happens after the
            // redirect resolved, so use the final URL regardless of success.
            std::string pub;
            if (!R.effective_url.empty()) pub = publisher_from_url(R.effective_url);
            if (pub == "finnhub.io") pub.clear(); // never redirected anywhere useful

            publisher_cache[url] = pub;
            if (pub.empty()) continue;

            std::lock_guard<std::mutex> lk(state_mutex);
            for (auto& item : news) {
                if (item.url == url) item.publisher = pub;
            }
        }
    };

    // ---- Background network thread ----
    // All blocking cURL calls happen here so the render loop never stalls.
    std::thread worker([&] {
        auto last_quote_fetch = std::chrono::steady_clock::now() - std::chrono::seconds(1000);
        auto last_news_fetch = std::chrono::steady_clock::now() - std::chrono::seconds(10000);

        while (running.load(std::memory_order_relaxed)) {
            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_quote_fetch).count();
            if (elapsed >= refresh_seconds) {
                last_quote_fetch = now;
                do_fetch();
            }

            if (refresh_chart_requested.exchange(false)) {
                fetch_all_granularities();
            }

            const float news_elapsed =
                std::chrono::duration_cast<std::chrono::duration<float>>(now - last_news_fetch).count();
            if (refresh_news_requested.exchange(false) || news_elapsed >= 300.0f) {
                last_news_fetch = now;
                fetch_news();
            }

            resolve_missing_names();
            resolve_news_publishers();

            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        bool refresh_chart_clicked = false;
        bool granularity_changed = false;
        bool symbol_changed = false;
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            draw_quotes_window(symbols,
                    last_request_ok, last_error, quotes, last_sync, log_lines,
                    selected_symbol, chart_minutes, chart_interval,
                    refresh_chart_clicked, granularity_changed, symbol_changed,
                    chart_xs, chart_opens, chart_highs, chart_lows, chart_closes,
                    news, news_loading);

            if (granularity_changed || symbol_changed) {
                const auto it = chart_cache.find(chart_interval);
                if (it != chart_cache.end() && cache_symbol == selected_symbol && cache_minutes == chart_minutes) {
                    // Already cached for this symbol/duration: swap in instantly, no fetch.
                    chart_xs = it->second.t;
                    chart_opens = it->second.o;
                    chart_highs = it->second.h;
                    chart_lows = it->second.l;
                    chart_closes = it->second.c;
                    last_request_ok = true;
                    last_error.clear();
                } else {
                    refresh_chart_clicked = true; // not cached yet - fetch (and cache) all granularities
                }
            }
        }
        if (refresh_chart_clicked) refresh_chart_requested.store(true);
        if (symbol_changed) refresh_news_requested.store(true);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    running.store(false);
    worker.join();

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    curl_global_cleanup();
    return 0;
}
