#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs = std::filesystem;
std::string executable_hash(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    HCRYPTPROV provider{};
    HCRYPTHASH hash{};
    if (!input || !CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return {};
    bool ok = CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE;
    std::array<BYTE, 65536> buffer{};
    while (ok && input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        ok = CryptHashData(hash, buffer.data(), static_cast<DWORD>(input.gcount()), 0) != FALSE;
    }
    std::array<BYTE, 32> digest{};
    DWORD size = static_cast<DWORD>(digest.size());
    ok = ok && input.eof() && !input.bad() && CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &size, 0);
    if (hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!ok || size != digest.size()) return {};
    std::string hex;
    constexpr char digits[] = "0123456789abcdef";
    for (BYTE value : digest) {
        hex += digits[value >> 4];
        hex += digits[value & 15];
    }
    return hex;
}

