#include "price_chart.hpp"
#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include <cstdio>
#include <ctime>

namespace {

// Adapted from implot_demo.cpp's MyImPlot::PlotCandlestick (tooltip omitted).
// xs here are plain sequential indices (0..count-1), not timestamps, so
// sessions sit right next to each other with no weekend/overnight gaps.
void plot_candlestick(const char* label_id,
                      const double* xs, const double* opens, const double* closes,
                      const double* lows, const double* highs, int count,
                      float width_percent = 0.25f,
                      const ImVec4& bull_col = ImVec4(0.20f, 0.80f, 0.35f, 1.0f),
                      const ImVec4& bear_col = ImVec4(0.90f, 0.30f, 0.30f, 1.0f))
{
    if (count < 1) return;

    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    const double half_width = count > 1 ? (xs[1] - xs[0]) * width_percent : width_percent;

    if (ImPlot::BeginItem(label_id)) {
        if (ImPlot::FitThisFrame()) {
            for (int i = 0; i < count; ++i) {
                ImPlot::FitPoint(ImPlotPoint(xs[i], lows[i]));
                ImPlot::FitPoint(ImPlotPoint(xs[i], highs[i]));
            }
        }
        const double cap_half_width = half_width * 0.5;
        for (int i = 0; i < count; ++i) {
            const ImVec2 open_pos  = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
            const ImVec2 close_pos = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);
            const ImVec2 low_pos   = ImPlot::PlotToPixels(xs[i], lows[i]);
            const ImVec2 high_pos  = ImPlot::PlotToPixels(xs[i], highs[i]);
            const ImVec2 high_l    = ImPlot::PlotToPixels(xs[i] - cap_half_width, highs[i]);
            const ImVec2 high_r    = ImPlot::PlotToPixels(xs[i] + cap_half_width, highs[i]);
            const ImVec2 low_l     = ImPlot::PlotToPixels(xs[i] - cap_half_width, lows[i]);
            const ImVec2 low_r     = ImPlot::PlotToPixels(xs[i] + cap_half_width, lows[i]);
            const ImU32 color = ImGui::GetColorU32(opens[i] > closes[i] ? bear_col : bull_col);
            draw_list->AddLine(low_pos, high_pos, color);
            draw_list->AddLine(high_l, high_r, color);
            draw_list->AddLine(low_l, low_r, color);
            draw_list->AddRectFilled(open_pos, close_pos, color);
        }
        ImPlot::EndItem();
    }
}

// Formats an X-axis index back into the real timestamp it stands in for.
int format_index_as_time(double value, char* buff, int size, void* user_data) {
    const auto* timestamps = static_cast<const std::vector<double>*>(user_data);
    if (!timestamps || timestamps->empty()) return 0;

    int idx = static_cast<int>(value + (value >= 0.0 ? 0.5 : -0.5));
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(timestamps->size())) idx = static_cast<int>(timestamps->size()) - 1;

    const std::time_t t = static_cast<std::time_t>((*timestamps)[idx]);
    std::tm tmv{};
    localtime_s(&tmv, &t);
    char tmp[32];
    std::strftime(tmp, sizeof(tmp), "%H:%M\n%d %b", &tmv);
    return std::snprintf(buff, size, "%s", tmp);
}

} // namespace

void draw_price_chart(const std::string& title,
                      const std::vector<double>& xs,
                      const std::vector<double>& opens,
                      const std::vector<double>& highs,
                      const std::vector<double>& lows,
                      const std::vector<double>& closes,
                      float height)
{
    const size_t n = xs.size();
    if (n == 0 || opens.size() != n || highs.size() != n || lows.size() != n || closes.size() != n) {
        ImGui::TextDisabled("No chart data for %s", title.c_str());
        return;
    }

    // Plot against a contiguous index rather than the real timestamp so
    // non-trading gaps (nights, weekends) don't show up as dead space.
    std::vector<double> idx_xs(n);
    for (size_t i = 0; i < n; ++i) idx_xs[i] = static_cast<double>(i);

    // Small x padding keeps the first/last candle bodies from being clipped
    // at the plot edges (their bodies extend half a candle-width past their
    // index), y padding keeps wicks off the top/bottom border.
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.05f, 0.1f));
    if (ImPlot::BeginPlot(title.c_str(), ImVec2(-1, height))) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisFormat(ImAxis_X1, format_index_as_time, const_cast<std::vector<double>*>(&xs));

        plot_candlestick(title.c_str(), idx_xs.data(), opens.data(), closes.data(), lows.data(), highs.data(),
                         static_cast<int>(n));
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}
