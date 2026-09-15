#include "quotes_window.hpp"
#include "imgui.h"
#include "article_window.hpp"
#include "price_chart.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <thread>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    const size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

// Native Windows dialog on its own thread, so it doesn't block the render loop.
void show_debug_popup(const std::string& title, const request_debug_view& dbg) {
    std::string body = title + "\n\n"
        + "URL: " + dbg.url + "\n"
        + "HTTP status: " + std::to_string(dbg.http_status) + "\n"
        + "cURL code: " + std::to_string(dbg.curl_code) + "\n";
    if (!dbg.curl_error.empty()) body += "cURL error: " + dbg.curl_error + "\n";
    if (!dbg.body_snippet.empty()) body += "\nResponse body:\n" + dbg.body_snippet;

    std::thread([text = to_wide(body)] {
        MessageBoxW(nullptr, text.c_str(), L"CTrader - Debug details",
                    MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }).detach();
}
} // namespace

void draw_quotes_window(std::vector<std::string>& symbols,
                        bool last_request_ok,
                        const std::string& last_error,
                        const std::vector<QuoteRow>& quotes,
                        std::chrono::system_clock::time_point last_sync,
                        const std::vector<LogEntry>& log_lines,
                        // chart (OHLC):
                        std::string& selected_symbol,
                        int& chart_minutes,
                        std::string& chart_interval,
                        bool& refresh_chart_out,
                        bool& granularity_changed_out,
                        bool& symbol_changed_out,
                        const std::vector<double>& chart_xs,
                        const std::vector<double>& chart_opens,
                        const std::vector<double>& chart_highs,
                        const std::vector<double>& chart_lows,
                        const std::vector<double>& chart_closes,
                        const std::vector<NewsItem>& news,
                        bool news_loading)

{
    refresh_chart_out = false;
    granularity_changed_out = false;
    symbol_changed_out = false;

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

    // ---- Split off a news column on the right; everything else goes left ----
    const float full_w = ImGui::GetContentRegionAvail().x;
    const float news_w = std::min(340.0f, full_w * 0.25f);
    const float main_w = full_w - news_w - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("main_col", ImVec2(main_w, 0), false);

    // ---- Bottom row height: sized to fit the ticker table's rows, with a floor ----
    const float row_h = ImGui::GetTextLineHeightWithSpacing();
    const float table_h = row_h * static_cast<float>(symbols.size() + 2);
    const float bottom_h = table_h > 150.0f ? table_h : 150.0f;

    // ---- Chart pane (top, full width) ----
    const float chart_avail_h = ImGui::GetContentRegionAvail().y - bottom_h - ImGui::GetStyle().ItemSpacing.y;
    const float chart_pane_h = chart_avail_h > 100.0f ? chart_avail_h : 100.0f;

    ImGui::BeginChild("chart_pane", ImVec2(0, chart_pane_h), false);
    {
        // Duration, split into days/hours/minutes but backed by the single
        // chart_minutes value the fetch logic uses. Mirror it into the three
        // fields only when it changes externally, so in-progress typing survives.
        {
            static int mirror_minutes = -1;
            static int days = 0, hours = 0, mins = 0;
            if (mirror_minutes != chart_minutes) {
                days  = chart_minutes / (24 * 60);
                hours = (chart_minutes % (24 * 60)) / 60;
                mins  = chart_minutes % 60;
                mirror_minutes = chart_minutes;
            }

            ImGui::TextUnformatted("Duration:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            bool changed = ImGui::InputInt("days", &days);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            changed |= ImGui::InputInt("hours", &hours);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            changed |= ImGui::InputInt("minutes", &mins);

            if (days < 0) days = 0;
            if (hours < 0) hours = 0;
            if (mins < 0) mins = 0;

            if (changed) {
                chart_minutes = days * 24 * 60 + hours * 60 + mins;
                if (chart_minutes < 1) chart_minutes = 1;
                mirror_minutes = chart_minutes;
            }
        }

        // Granularity, as a row of standard broker-style interval buttons.
        ImGui::TextUnformatted("Granularity:");
        {
            static const std::pair<const char*, const char*> options[] = {
                {"1m", "1m"}, {"5m", "5m"}, {"15m", "15m"}, {"30m", "30m"},
                {"1H", "60m"}, {"1D", "1d"}, {"1W", "1wk"}, {"1M", "1mo"},
            };
            for (const auto& [label, code] : options) {
                ImGui::SameLine();
                const bool selected = chart_interval == code;
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                if (ImGui::SmallButton(label) && chart_interval != code) {
                    chart_interval = code;
                    granularity_changed_out = true;
                }
                if (selected) ImGui::PopStyleColor();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh Chart")) {
            refresh_chart_out = true;
        }

        const float chart_height = ImGui::GetContentRegionAvail().y > 100.0f
            ? ImGui::GetContentRegionAvail().y
            : 100.0f;
        draw_price_chart(selected_symbol.empty() ? "Chart" : selected_symbol,
                         chart_xs, chart_opens, chart_highs, chart_lows, chart_closes, chart_height);
    }
    ImGui::EndChild();

    // ---- Bottom row: log (left) + ticker table (right, capped at half width) ----
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float table_w = avail_w * 0.5f;
    const float log_w = avail_w - table_w - ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginChild("log_pane", ImVec2(log_w, bottom_h), true);
    {
        const std::string status_str = last_request_ok ? "Status: OK" : ("Status: ERROR - " + last_error);
        const ImVec4 status_col = last_request_ok ? ImVec4(0.6f, 1.0f, 0.6f, 1.0f) : ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
        ImGui::TextColored(status_col, "%s", status_str.c_str());

        char synced_buf[32] = "Not synced yet";
        if (last_sync.time_since_epoch().count() != 0) {
            const std::time_t t = std::chrono::system_clock::to_time_t(last_sync);
            std::tm tmv{};
            localtime_s(&tmv, &t);
            std::snprintf(synced_buf, sizeof(synced_buf), "Synced: %02d:%02d", tmv.tm_hour, tmv.tm_min);
        }
        ImGui::TextUnformatted(synced_buf);

        ImGui::Separator();

        // Persistent history of status changes - unlike the summary line
        // above, entries here stay even after the state clears back to OK.
        ImGui::BeginChild("log_scroll", ImVec2(0, 0), false);
        for (size_t i = 0; i < log_lines.size(); ++i) {
            const auto& entry = log_lines[i];
            ImVec4 col;
            switch (entry.level) {
                case LogLevel::Error:   col = ImVec4(1.0f, 0.5f, 0.5f, 1.0f); break;
                case LogLevel::Success: col = ImVec4(0.5f, 1.0f, 0.5f, 1.0f); break;
                default:                col = ImGui::GetStyle().Colors[ImGuiCol_Text]; break;
            }
            ImGui::PushID(static_cast<int>(i));
            if (entry.has_debug) {
                // Selectable doesn't wrap, so size it to the wrapped text's
                // height and draw the wrapped text over it to keep it clickable.
                const std::string label = entry.text + "  [details]";
                float wrap_w = ImGui::GetContentRegionAvail().x;
                if (wrap_w < 1.0f) wrap_w = 1.0f;
                const ImVec2 text_size = ImGui::CalcTextSize(label.c_str(), nullptr, false, wrap_w);
                const ImVec2 start = ImGui::GetCursorPos();

                if (ImGui::Selectable("##entry", false, ImGuiSelectableFlags_None, ImVec2(wrap_w, text_size.y))) {
                    show_debug_popup(entry.text, entry.debug);
                }

                ImGui::SetCursorPos(start);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(label.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextWrapped("%s", entry.text.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("table_pane", ImVec2(0, bottom_h), false);
    {
        const ImGuiTableFlags flags_tbl = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit |
                                      ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("quotes_table", 8, flags_tbl, ImVec2(0, table_h))) {
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

            auto find_quote = [&](const std::string& sym) -> const QuoteRow* {
                for (const auto& q : quotes) if (q.symbol == sym) return &q;
                return nullptr;
            };

            for (size_t i = 0; i < symbols.size(); ) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                const QuoteRow* q = find_quote(symbols[i]);
                bool erase_this = false;

                ImGui::TableSetColumnIndex(0);
                erase_this = ImGui::SmallButton("x");
                ImGui::SameLine();
                // Clicking the ticker itself charts it.
                if (ImGui::Selectable(symbols[i].c_str(), symbols[i] == selected_symbol) &&
                    symbols[i] != selected_symbol) {
                    selected_symbol = symbols[i];
                    symbol_changed_out = true;
                }

                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(q ? q->name.c_str() : "-");
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(q ? fmt_opt(q->price).c_str() : "-");

                ImGui::TableSetColumnIndex(3);
                if (q && q->change.has_value()) {
                    ImVec4 col = (*q->change >= 0.0) ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                                                     : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(col, "%s", fmt_opt(q->change).c_str());
                } else ImGui::TextUnformatted("-");

                ImGui::TableSetColumnIndex(4);
                if (q && q->change_pct.has_value()) {
                    ImVec4 colp = (*q->change_pct >= 0.0) ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                                                          : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    std::string s = fmt_opt(q->change_pct, 2) + "%";
                    ImGui::TextColored(colp, "%s", s.c_str());
                } else ImGui::TextUnformatted("-");

                ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(q ? fmt_opt(q->day_low).c_str() : "-");
                ImGui::TableSetColumnIndex(6); ImGui::TextUnformatted(q ? fmt_opt(q->day_high).c_str() : "-");
                ImGui::TableSetColumnIndex(7); ImGui::TextUnformatted(q && !q->currency.empty() ? q->currency.c_str() : "-");

                ImGui::PopID();

                if (erase_this) {
                    const bool was_selected = (symbols[i] == selected_symbol);
                    symbols.erase(symbols.begin() + i);
                    if (was_selected) {
                        selected_symbol = symbols.empty() ? std::string() : symbols.front();
                        symbol_changed_out = true;
                    }
                } else {
                    ++i;
                }
            }

            // --- Add-symbol row ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::SmallButton("+")) {
                ImGui::OpenPopup("add_symbol_popup");
            }

            if (ImGui::BeginPopup("add_symbol_popup")) {
                static char newsym[16] = "";
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::SetNextItemWidth(100.0f);
                const bool submitted = ImGui::InputText("##newsym", newsym, IM_ARRAYSIZE(newsym),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase);
                ImGui::SameLine();
                const bool add_clicked = ImGui::Button("Add");
                if (submitted || add_clicked) {
                    const std::string s = trim(newsym);
                    if (!s.empty() && std::find(symbols.begin(), symbols.end(), s) == symbols.end()) {
                        symbols.push_back(s);
                        if (selected_symbol.empty()) {
                            selected_symbol = s;
                            symbol_changed_out = true;
                        }
                    }
                    newsym[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::EndChild(); // main_col

    ImGui::SameLine();

    // ---- News column (right, full height) ----
    ImGui::BeginChild("news_pane", ImVec2(0, 0), true);
    {
        ImGui::TextUnformatted(selected_symbol.empty() ? "News" : ("News - " + selected_symbol).c_str());
        ImGui::Separator();

        if (news.empty()) {
            ImGui::TextDisabled(news_loading ? "Loading news..." : "No recent news.");
        }

        for (size_t i = 0; i < news.size(); ++i) {
            const auto& item = news[i];
            ImGui::PushID(static_cast<int>(i));

            float wrap_w = ImGui::GetContentRegionAvail().x;
            if (wrap_w < 1.0f) wrap_w = 1.0f;
            const ImVec2 text_size = ImGui::CalcTextSize(item.headline.c_str(), nullptr, false, wrap_w);
            const ImVec2 start = ImGui::GetCursorPos();

            if (ImGui::Selectable("##headline", false, ImGuiSelectableFlags_None, ImVec2(wrap_w, text_size.y))) {
                show_article_window(item);
            }

            ImGui::SetCursorPos(start);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(item.headline.c_str());
            ImGui::PopTextWrapPos();

            char when[32] = "";
            if (item.datetime > 0) {
                const std::time_t t = static_cast<std::time_t>(item.datetime);
                std::tm tmv{};
                localtime_s(&tmv, &t);
                std::strftime(when, sizeof(when), "%d %b  %H:%M", &tmv);
            }
            const std::string outlet = !item.publisher.empty()
                ? item.publisher
                : (item.source.empty() ? std::string() : "via " + item.source);
            if (!outlet.empty() || when[0]) {
                ImGui::TextDisabled("%s%s%s", outlet.c_str(),
                                    (!outlet.empty() && when[0]) ? "  -  " : "", when);
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
