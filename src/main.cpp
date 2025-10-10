#include <cstdio>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>

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

#include "data/quote_row.hpp"
#include "net/http.hpp"
#include "providers/finnhub.hpp"
#include "ui/quotes_window.hpp"

static std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    const size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // ---- App state ----
    std::string symbols_csv = "JPM,GS,NVDA,AMD,MSFT,GOOGL,NVDA";
    float refresh_seconds = 5.0f;
    bool auto_refresh = true;
    bool last_request_ok = true;
    std::string last_error;
    std::vector<QuoteRow> quotes;
    auto last_fetch = std::chrono::steady_clock::now() - std::chrono::seconds(1000);

    // API key
    std::string api_key = []{
        const char* e = std::getenv("FINNHUB_TOKEN");
        return e ? std::string(e) : std::string();
    }();
    bool reveal_api_key = false;

    // Debug info for UI
    request_debug_view debug_info{};
    const bool curl_verbose = std::getenv("CTRADER_CURL_VERBOSE") != nullptr;

    auto do_fetch = [&]
    {
        last_error.clear();
        last_request_ok = true;
        quotes.clear();

        if (api_key.empty()) {
            last_request_ok = false;
            last_error = "Finnhub API key missing";
            return;
        }

        std::istringstream ss(symbols_csv);
        std::string sym;
        while (std::getline(ss, sym, ',')) {
            sym = trim(sym);
            if (sym.empty()) continue;

            auto url = build_finnhub_quote_url(sym, api_key);
            auto [ok, http_status, curl_code, error, body] = http_get_ex(url, curl_verbose);

            // Save debug for last request
            debug_info.url = url;
            debug_info.http_status = http_status;
            debug_info.curl_code = curl_code;
            debug_info.curl_error = error;
            debug_info.body_snippet = (body.size() > 2000) ? body.substr(0, 2000) + "...(truncated)" : body;

            if (!ok) {
                last_request_ok = false;
                if (!body.empty()) {
                    last_error = "HTTP " + std::to_string(http_status) + " - " + body;
                } else if (!error.empty()) {
                    last_error = "cURL " + std::to_string(curl_code) + " - " + error;
                } else {
                    last_error = "HTTP request failed";
                }
                break;
            }

            QuoteRow row = parse_finnhub_quote_json(body, sym);
            if (!row.price.has_value()) {
                last_request_ok = false;
                last_error = "Finnhub: no price in payload";
                break;
            }
            quotes.push_back(std::move(row));
        }

        if (last_request_ok) last_fetch = std::chrono::steady_clock::now();
    };

    do_fetch();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (auto_refresh) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_fetch).count();
            if (elapsed >= refresh_seconds) do_fetch();
        }

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (draw_quotes_window(symbols_csv, refresh_seconds, auto_refresh,
            last_request_ok, last_error, quotes, api_key, reveal_api_key, &debug_info))
        {
            do_fetch();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    curl_global_cleanup();
    return 0;
}
