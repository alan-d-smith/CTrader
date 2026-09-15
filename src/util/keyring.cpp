#ifdef _WIN32

#include "keyring.hpp"
#include <string>
#include <optional>
#define NOMINMAX
#include <windows.h>
#include <wincred.h>

namespace keyring {

static std::wstring wstr(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static std::wstring target_name(const std::string& service, const std::string& account) {
    return wstr(service + ":" + account);
}

bool store_secret(const std::string& service, const std::string& account, const std::string& secret) {
    if (service.empty() || account.empty()) return false;
    const std::wstring target = target_name(service, account);
    const std::wstring uname  = wstr(account);

    CREDENTIALW cred{};
    cred.Flags = 0;
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<wchar_t*>(target.c_str());
    cred.Comment = nullptr;
    cred.CredentialBlobSize = (DWORD)secret.size();
    cred.CredentialBlob = (LPBYTE)secret.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.AttributeCount = 0;
    cred.UserName = const_cast<wchar_t*>(uname.c_str());

    return CredWriteW(&cred, 0) == TRUE;
}

std::optional<std::string> load_secret(const std::string& service, const std::string& account) {
    if (service.empty() || account.empty()) return std::nullopt;
    const std::wstring target = target_name(service, account);

    PCREDENTIALW pcred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &pcred))
        return std::nullopt;

    std::optional<std::string> out;
    if (pcred->CredentialBlob && pcred->CredentialBlobSize > 0) {
        out = std::string(reinterpret_cast<const char*>(pcred->CredentialBlob),
                          (size_t)pcred->CredentialBlobSize);
    }
    CredFree(pcred);
    return out;
}

bool delete_secret(const std::string& service, const std::string& account) {
    if (service.empty() || account.empty()) return false;
    const std::wstring target = target_name(service, account);
    return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;
}

}

#endif // _WIN32
