#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace Solar {

struct SolPackageHeader {
    uint16_t formatVersion = 0;
    uint16_t flags = 0;
    uint64_t titleId = 0;
    std::array<uint8_t, 12> indexNonce{};
    uint32_t indexSize = 0;
    uint64_t indexOffset = 36;
};

class SolPackageReader {
public:
    static constexpr uint16_t SupportedFormatVersion = 1;
    static constexpr uint16_t FlagEncrypted = 1u << 0;
    static constexpr uint16_t FlagCompressed = 1u << 1;

    // Reads and validates the fixed SOL v1 header only.
    // Manifest/index decryption is intentionally the next v0.6 milestone.
    static bool ReadHeader(const std::string &path,
                           SolPackageHeader &header,
                           std::string *error = nullptr);

    static bool MatchesTitle(const SolPackageHeader &header, uint64_t titleId);
};

} // namespace Solar
