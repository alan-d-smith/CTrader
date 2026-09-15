#pragma once
#include <string>

// ImGui's default font only covers Latin-1, so typographic punctuation (curly
// quotes, dashes, ellipsis) renders as '?'. Swap those for ASCII equivalents.
std::string to_ascii_punctuation(const std::string& s);
