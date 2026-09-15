#include "yahoo.hpp"
#include <sstream>
#include "../util/text.hpp"
#include "../third_party/json.hpp"
using json = nlohmann::json;

std::string build_yahoo_chart_url(const std::string& symbol,
                                  const std::string& interval,
                                  int64_t from_unix, int64_t to_unix) {
    std::ostringstream oss;
    oss << "https://query1.finance.yahoo.com/v8/finance/chart/" << symbol
        << "?period1=" << from_unix
        << "&period2=" << to_unix
        << "&interval=" << interval
        << "&includePrePost=false";
    return oss.str();
}

candles parse_yahoo_chart_json(const std::string& payload) {
    candles out;
    try {
        auto j = json::parse(payload);
        if (!j.contains("chart")) { out.err = "unexpected response format"; return out; }
        const auto& chart = j["chart"];

        if (chart.contains("error") && !chart["error"].is_null()) {
            out.err = (chart["error"].contains("description") && chart["error"]["description"].is_string())
                ? chart["error"]["description"].get<std::string>()
                : std::string("Yahoo returned an error");
            return out;
        }

        if (!chart.contains("result") || chart["result"].is_null() || chart["result"].empty()) {
            out.err = "no data for this range";
            return out;
        }

        const auto& result0 = chart["result"][0];
        if (!result0.contains("timestamp") || result0["timestamp"].is_null()) {
            out.err = "no data for this range";
            return out;
        }

        const auto& ts = result0["timestamp"];
        const auto& quotes = result0["indicators"]["quote"];
        if (!result0.contains("indicators") || !result0["indicators"].contains("quote") ||
            quotes.empty() || !quotes[0].contains("close")) {
            out.err = "no data for this range";
            return out;
        }
        const auto& q0 = quotes[0];
        const auto& opens  = q0.contains("open")  ? q0["open"]  : json::array();
        const auto& highs  = q0.contains("high")  ? q0["high"]  : json::array();
        const auto& lows   = q0.contains("low")   ? q0["low"]   : json::array();
        const auto& closes = q0["close"];

        const size_t n = ts.size();
        out.t.reserve(n);
        out.o.reserve(n);
        out.h.reserve(n);
        out.l.reserve(n);
        out.c.reserve(n);
        for (size_t i = 0; i < n && i < closes.size(); ++i) {
            if (ts[i].is_null() || closes[i].is_null()) continue; // market-closed gap
            if (i >= opens.size() || i >= highs.size() || i >= lows.size() ||
                opens[i].is_null() || highs[i].is_null() || lows[i].is_null()) continue;

            out.t.push_back(static_cast<double>(ts[i].get<int64_t>()));
            out.o.push_back(opens[i].get<double>());
            out.h.push_back(highs[i].get<double>());
            out.l.push_back(lows[i].get<double>());
            out.c.push_back(closes[i].get<double>());
        }

        if (out.t.empty()) { out.err = "no data for this range"; return out; }
        out.ok = true;
    } catch (const std::exception&) {
        out.err = "unexpected response format";
    }
    return out;
}

std::string build_yahoo_quote_names_url(const std::string& symbols_csv) {
    std::ostringstream oss;
    oss << "https://query1.finance.yahoo.com/v7/finance/quote?symbols=" << symbols_csv;
    return oss.str();
}

std::map<std::string, std::string> parse_yahoo_quote_names_json(const std::string& payload) {
    std::map<std::string, std::string> out;
    try {
        auto j = json::parse(payload);
        if (!j.contains("quoteResponse")) return out;
        const auto& qr = j["quoteResponse"];
        if (!qr.contains("result") || qr["result"].is_null()) return out;

        for (const auto& item : qr["result"]) {
            if (!item.contains("symbol") || !item["symbol"].is_string()) continue;
            const std::string sym = item["symbol"].get<std::string>();

            std::string name;
            if (item.contains("longName") && item["longName"].is_string()) {
                name = item["longName"].get<std::string>();
            } else if (item.contains("shortName") && item["shortName"].is_string()) {
                name = item["shortName"].get<std::string>();
            }
            if (!name.empty()) out[sym] = to_ascii_punctuation(name);
        }
    } catch (...) {}
    return out;
}
