#include "article_window.hpp"

#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

namespace {

constexpr wchar_t kClassName[] = L"CTraderArticleWindow";
constexpr int kIdOpenButton = 1001;

const COLORREF kBg   = RGB(28, 28, 32);
const COLORREF kText = RGB(228, 228, 235);
const COLORREF kDim  = RGB(158, 158, 170);

HBRUSH g_bg_brush = nullptr;

std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

// EDIT controls only break lines on CRLF.
std::wstring to_crlf(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r')) out += L"\r\n";
        else out += s[i];
    }
    return out;
}

struct ArticleData {
    std::wstring headline;
    std::wstring meta;
    std::wstring body;
    std::wstring url;
    HFONT title_font = nullptr;
    HFONT meta_font = nullptr;
    HFONT body_font = nullptr;
    HWND title_ctl = nullptr;
    HWND meta_ctl = nullptr;
    HWND body_ctl = nullptr;
    HWND button_ctl = nullptr;
};

int measure_title(HWND hwnd, ArticleData* d, int width) {
    HDC dc = GetDC(hwnd);
    HFONT prev = static_cast<HFONT>(SelectObject(dc, d->title_font));
    RECT rc{0, 0, width, 0};
    DrawTextW(dc, d->headline.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    return rc.bottom - rc.top;
}

void layout(HWND hwnd, ArticleData* d) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int pad = 14;
    const int w = (rc.right - rc.left) - pad * 2;
    if (w <= 0) return;

    int y = pad;
    const int title_h = measure_title(hwnd, d, w);
    MoveWindow(d->title_ctl, pad, y, w, title_h, TRUE);
    y += title_h + 6;

    const int meta_h = 20;
    MoveWindow(d->meta_ctl, pad, y, w, meta_h, TRUE);
    y += meta_h + 12;

    const int button_h = 30;
    int body_h = (rc.bottom - rc.top) - y - button_h - pad * 2;
    if (body_h < 60) body_h = 60;
    MoveWindow(d->body_ctl, pad, y, w, body_h, TRUE);
    y += body_h + pad;

    MoveWindow(d->button_ctl, pad, y, 170, button_h, TRUE);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* d = reinterpret_cast<ArticleData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            d = static_cast<ArticleData*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

            auto make_font = [](int height, int weight) {
                return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   VARIABLE_PITCH, L"Segoe UI");
            };
            d->title_font = make_font(-23, FW_SEMIBOLD);
            d->meta_font  = make_font(-13, FW_NORMAL);
            d->body_font  = make_font(-16, FW_NORMAL);

            HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            d->title_ctl = CreateWindowExW(0, L"STATIC", d->headline.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
            d->meta_ctl = CreateWindowExW(0, L"STATIC", d->meta.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
            d->body_ctl = CreateWindowExW(0, L"EDIT", d->body.c_str(),
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
            d->button_ctl = CreateWindowExW(0, L"BUTTON", L"Open full article",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOpenButton)), inst, nullptr);

            SendMessageW(d->title_ctl,  WM_SETFONT, reinterpret_cast<WPARAM>(d->title_font), TRUE);
            SendMessageW(d->meta_ctl,   WM_SETFONT, reinterpret_cast<WPARAM>(d->meta_font),  TRUE);
            SendMessageW(d->body_ctl,   WM_SETFONT, reinterpret_cast<WPARAM>(d->body_font),  TRUE);
            SendMessageW(d->button_ctl, WM_SETFONT, reinterpret_cast<WPARAM>(d->meta_font),  TRUE);

            if (d->url.empty()) EnableWindow(d->button_ctl, FALSE);
            layout(hwnd, d);
            return 0;
        }
        case WM_SIZE:
            if (d) layout(hwnd, d);
            return 0;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, kBg);
            SetTextColor(dc, (d && reinterpret_cast<HWND>(lp) == d->meta_ctl) ? kDim : kText);
            return reinterpret_cast<LRESULT>(g_bg_brush);
        }
        case WM_COMMAND:
            if (d && LOWORD(wp) == kIdOpenButton && !d->url.empty()) {
                ShellExecuteW(nullptr, L"open", d->url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        case WM_DESTROY:
            if (d) {
                if (d->title_font) DeleteObject(d->title_font);
                if (d->meta_font)  DeleteObject(d->meta_font);
                if (d->body_font)  DeleteObject(d->body_font);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete d;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ensure_class_registered() {
    static std::once_flag once;
    std::call_once(once, [] {
        g_bg_brush = CreateSolidBrush(kBg);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        // LoadCursor (not ...W) so it matches whichever form IDC_ARROW expands to.
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = g_bg_brush;
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);
    });
}

std::string format_meta(const NewsItem& item) {
    // Finnhub's "source" is the aggregator, so only show it as "via X" until
    // the real outlet has been resolved from the article's redirect.
    std::string meta = !item.publisher.empty()
        ? item.publisher
        : (item.source.empty() ? std::string() : "via " + item.source);
    if (item.datetime > 0) {
        const std::time_t t = static_cast<std::time_t>(item.datetime);
        std::tm tmv{};
        localtime_s(&tmv, &t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%d %b %Y  %H:%M", &tmv);
        if (!meta.empty()) meta += "   -   ";
        meta += buf;
    }
    return meta;
}

} // namespace

void show_article_window(const NewsItem& item) {
    auto* data = new ArticleData();
    data->headline = to_wide(item.headline.empty() ? std::string("(untitled)") : item.headline);
    data->meta = to_wide(format_meta(item));
    data->url = to_wide(item.url);

    std::string body = item.summary.empty()
        ? std::string("No summary was provided for this article. Use \"Open full article\" to read it in your browser.")
        : item.summary;
    if (!item.url.empty()) body += "\n\n" + item.url;
    data->body = to_crlf(to_wide(body));

    std::thread([data] {
        ensure_class_registered();
        HWND hwnd = CreateWindowExW(0, kClassName, L"CTrader - Article", WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 760, 620,
                                    nullptr, nullptr, GetModuleHandleW(nullptr), data);
        if (!hwnd) return; // WM_DESTROY owns `data` once the window exists

        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }).detach();
}
