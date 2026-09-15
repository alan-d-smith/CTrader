#include "dotenv.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    const size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, b - a + 1);
}

std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

void set_env_if_absent(const std::string& key, const std::string& value) {
    if (key.empty() || std::getenv(key.c_str()) != nullptr) return;

#ifdef _WIN32
    // _putenv_s (not SetEnvironmentVariableA) updates the CRT's own environment
    // table, which is what std::getenv actually reads from on Windows.
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 0);
#endif
}

} // namespace

void load_dotenv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = trim(trimmed.substr(0, eq));
        const std::string value = strip_quotes(trim(trimmed.substr(eq + 1)));

        set_env_if_absent(key, value);
    }
}
