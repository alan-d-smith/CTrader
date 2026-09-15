#pragma once
#include "../data/news_item.hpp"

// Opens the article in its own native window, on its own thread so the
// dashboard keeps rendering while it's up.
void show_article_window(const NewsItem& item);
