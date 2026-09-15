<h1 align="center">
  <img src="assets/icon_32.png" width="28" height="28" alt="" valign="middle"/>
  CTrader
</h1>

<p align="center">A real-time stock quote and candlestick charting dashboard, built with Dear ImGui.</p>

<p align="center">
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/alan-d-smith/CTrader"></a>
  <img alt="Language" src="https://img.shields.io/badge/language-C%2B%2B17-blue">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-informational">
  <a href="https://github.com/alan-d-smith/CTrader/stargazers"><img alt="Stars" src="https://img.shields.io/github/stars/alan-d-smith/CTrader"></a>
  <a href="https://github.com/alan-d-smith/CTrader/issues"><img alt="Issues" src="https://img.shields.io/github/issues/alan-d-smith/CTrader"></a>
</p>

<img height="550" alt="Demo Screenshot" src="https://github.com/user-attachments/assets/6c9958b4-5307-4562-bab2-ca771c71f1db"/>

## Features

- **Live quotes** for a configurable watchlist, auto-refreshed on a background thread so the UI never blocks
- **Candlestick chart** for any ticker in your list, with one click to switch symbols
- Standard broker-style **granularity switching** (1m/5m/15m/30m/1H/1D/1W/1M), all pre-fetched and cached for instant switching
- Adjustable chart **duration** (days/hours/minutes) and window resolution that scales with the app
- Persistent, colour-coded **status log** for sync errors and recoveries
- Company names resolved automatically, with a fallback provider if the primary source doesn't have one

## Data sources

- [Finnhub](https://finnhub.io/) for live quotes
- [Yahoo Finance](https://finance.yahoo.com/) for historical candles

You'll need your own free [Finnhub API key](https://finnhub.io/register) for quotes; the chart doesn't require one.

## Getting started

### Prerequisites

- [vcpkg](https://vcpkg.io/) with `glfw3`, `curl` installed
- CMake 3.16+ and a C++17 compiler (MSVC on Windows)

### Build

```sh
git clone --recurse-submodules https://github.com/alan-d-smith/CTrader.git
cd CTrader
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive` before configuring.

### Configure

Copy `.env.example` to `.env` and fill in your Finnhub token:

```env
FINNHUB_TOKEN=your_finnhub_api_key_here
CTRADER_REFRESH_SECONDS=5
CTRADER_SYMBOLS=JPM,GS,NVDA,AMD,MSFT,GOOGL,NFLX,TSLA
```

| Variable                   | Description                                   | Default            |
|-----------------------------|------------------------------------------------|---------------------|
| `FINNHUB_TOKEN`             | Your Finnhub API key                           | *(required)*        |
| `CTRADER_SYMBOLS`           | Comma-separated default watchlist              | built-in fallback list |
| `CTRADER_REFRESH_SECONDS`   | Quote auto-refresh interval, in seconds        | `5`                  |
| `CTRADER_CURL_VERBOSE`      | Set to anything to log verbose cURL output     | unset                |

The Finnhub key also falls back to the OS keyring (`CTrader` / `finnhub_token`) if `.env` doesn't provide one.

## License

[MIT](LICENSE)
