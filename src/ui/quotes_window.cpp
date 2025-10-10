#include "quotes_window.hpp"
#include "imgui.h"

bool draw_quotes_window(std::string& symbols_csv,
                        float& refresh_seconds,
                        bool& auto_refresh,
                        bool last_request_ok,
                        const std::string& last_error,
                        const std::vector<QuoteRow>& quotes,
                        std::string& api_key,
                        bool& reveal_api_key,
                        const request_debug_view* dbg)
{
    bool fetch_clicked = false;

    // ---- Viewport settings ----
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGuiWindowFlags win_flags =
          ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("CTrader - Quotes", nullptr, win_flags);

    // --- API key row (Finnhub) ---
    ImGui::TextUnformatted("Finnhub API key:");
    ImGui::SameLine();
    ImGui::Checkbox("Reveal", &reveal_api_key);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(360.0f);

    // Mirrors api_key only when api_key changes externally.
    static char keybuf[256] = {};
    static std::string key_mirror;

    if (key_mirror != api_key) {
        std::snprintf(keybuf, sizeof(keybuf), "%s", api_key.c_str());
        key_mirror = api_key;
    }

    ImGuiInputTextFlags key_flags = reveal_api_key ? 0 : ImGuiInputTextFlags_Password;
    ImGui::InputText("##api_key", keybuf, IM_ARRAYSIZE(keybuf), key_flags);
    ImGui::SameLine();
    if (ImGui::Button("Apply")) { // Apply clicked
        api_key = keybuf;
        key_mirror = api_key;
    }

    ImGui::Separator();

    // --- Controls ---
    ImGui::TextUnformatted("Symbols (CSV):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(500.0f);
    static char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", symbols_csv.c_str());
    if (ImGui::InputText("##symbols", buf, IM_ARRAYSIZE(buf))) {
        symbols_csv = buf;
    }

    ImGui::SameLine();
    if (ImGui::Button("Fetch Now")) {
        api_key = keybuf;
        key_mirror = api_key;
        fetch_clicked = true;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto refresh", &auto_refresh);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("every (s)", &refresh_seconds, 0.5f, 2.0f, "%.1f");
    if (refresh_seconds < 1.0f) refresh_seconds = 1.0f;

    // --- Status ---
    if (last_request_ok) {
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Status: OK");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Status: ERROR - %s", last_error.c_str());
    }

    ImGui::Separator();

    // --- Debug ---
    if (!last_request_ok && dbg) {
        if (ImGui::CollapsingHeader("Debug details", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("URL: %s", dbg->url.c_str());
            ImGui::Text("HTTP status: %ld", dbg->http_status);
            ImGui::Text("cURL code: %d", dbg->curl_code);
            if (!dbg->curl_error.empty()) {
                ImGui::TextWrapped("cURL error: %s", dbg->curl_error.c_str());
            }
            if (!dbg->body_snippet.empty()) {
                ImGui::SeparatorText("Response body");
                ImGui::BeginChild("resp_snip", ImVec2(0, 120), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(dbg->body_snippet.c_str());
                ImGui::EndChild();
            }
        }
        ImGui::Separator();
    }

    // --- Data Table ---
    const ImGuiTableFlags flags_tbl = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit |
                                  ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("quotes_table", 8, flags_tbl, ImVec2(0, 0))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);

        float col_w = ImGui::CalcTextSize("00000").x + ImGui::GetStyle().CellPadding.x * 2.0f;
        ImGui::TableSetupColumn("Price",     ImGuiTableColumnFlags_WidthFixed, col_w);
        ImGui::TableSetupColumn("Δ",         ImGuiTableColumnFlags_WidthFixed, col_w);
        ImGui::TableSetupColumn("Δ%",        ImGuiTableColumnFlags_WidthFixed, col_w);
        ImGui::TableSetupColumn("Low",       ImGuiTableColumnFlags_WidthFixed, col_w);
        ImGui::TableSetupColumn("High",      ImGuiTableColumnFlags_WidthFixed, col_w);
        ImGui::TableSetupColumn("CCY",       ImGuiTableColumnFlags_WidthFixed, col_w);

        ImGui::TableHeadersRow();

        for (const auto& q : quotes) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(q.symbol.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(q.name.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(fmt_opt(q.price).c_str());

            ImGui::TableSetColumnIndex(3);
            if (q.change.has_value()) {
                ImVec4 col = (*q.change >= 0.0) ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                                                : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                ImGui::TextColored(col, "%s", fmt_opt(q.change).c_str());
            } else ImGui::TextUnformatted("-");

            ImGui::TableSetColumnIndex(4);
            if (q.change_pct.has_value()) {
                ImVec4 colp = (*q.change_pct >= 0.0) ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                                                     : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                std::string s = fmt_opt(q.change_pct, 2) + "%";
                ImGui::TextColored(colp, "%s", s.c_str());
            } else ImGui::TextUnformatted("-");

            ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(fmt_opt(q.day_low).c_str());
            ImGui::TableSetColumnIndex(6); ImGui::TextUnformatted(fmt_opt(q.day_high).c_str());
            ImGui::TableSetColumnIndex(7); ImGui::TextUnformatted(q.currency.empty() ? "-" : q.currency.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::End();

    return fetch_clicked;
}
