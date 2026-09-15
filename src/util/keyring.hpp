#pragma once
#include <string>
#include <optional>

namespace keyring {
    bool store_secret(const std::string& service, const std::string& account, const std::string& secret);
    std::optional<std::string> load_secret(const std::string& service, const std::string& account);
    bool delete_secret(const std::string& service, const std::string& account);
}
