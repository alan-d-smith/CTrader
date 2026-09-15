#pragma once
#include <string>
#include <vector>

struct candles {
    std::vector<double> t; // unix seconds
    std::vector<double> o, h, l, c;
    bool ok = false; // true if the response actually contained data
    std::string err;
};
