#include "solar/SolKeyProvider.hpp"

#include <cctype>
#include <cstring>

namespace Solar::SolKeyProvider {
namespace {

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

void SetError(std::string *error, const char *message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool GetKey(std::array<uint8_t, 32> &key, std::string *error) {
    key.fill(0);

#ifndef ASTRA_SOL_KEY_HEX
    SetError(error, "Astra was built without ASTRA_SOL_KEY_HEX");
    return false;
#else
    constexpr const char *HexKey = ASTRA_SOL_KEY_HEX;
    constexpr size_t RequiredHexLength = 64;

    if (std::strlen(HexKey) != RequiredHexLength) {
        SetError(error, "ASTRA_SOL_KEY_HEX must contain exactly 64 hexadecimal characters");
        return false;
    }

    for (size_t i = 0; i < key.size(); ++i) {
        const int high = HexValue(HexKey[i * 2]);
        const int low = HexValue(HexKey[i * 2 + 1]);
        if (high < 0 || low < 0) {
            key.fill(0);
            SetError(error, "ASTRA_SOL_KEY_HEX contains a non-hexadecimal character");
            return false;
        }
        key[i] = static_cast<uint8_t>((high << 4) | low);
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
#endif
}

} // namespace Solar::SolKeyProvider
