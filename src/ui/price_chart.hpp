#pragma once
#include <vector>
#include <string>

// xs = unix seconds (double); opens/highs/lows/closes are parallel OHLC arrays
void draw_price_chart(const std::string& title,
                      const std::vector<double>& xs,
                      const std::vector<double>& opens,
                      const std::vector<double>& highs,
                      const std::vector<double>& lows,
                      const std::vector<double>& closes,
                      float height = 260.0f);
