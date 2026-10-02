#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Solar {

struct SolPackageHeader {
    uint16_t formatVersion = 0;
    uint16_t flags = 0;
    uint64_t titleId = 0;
    std::array<uint8_t, 12> indexNonce{};
    uint32_t indexSize = 0;
    uint64_t indexOffset = 36;
};

struct SolPackageManifest {
    std::string name;
    std::string author;
    std::string version;
    std::string type;
    std::string titleId;
    bool enabled = true;
    int priority = 0;
};

struct SolFileRecord {
    std::string path;
    uint64_t payloadOffset = 0;
    uint32_t encryptedSize = 0;
    uint32_t compressedSize = 0;
    uint32_t originalSize = 0;
    std::array<uint8_t, 12> nonce{};
    std::string compression;
    std::string encryption;
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

    static bool ReadEncryptedIndex(const std::string &path,
                                   const SolPackageHeader &header,
                                   std::vector<uint8_t> &encryptedIndex,
                                   std::string *error = nullptr);

    static bool DecryptIndex(const std::string &path,
                             const SolPackageHeader &header,
                             const std::array<uint8_t, 32> &key,
                             std::string &indexJson,
                             std::string *error = nullptr);

    static bool ParseManifest(const std::string &indexJson,
                              SolPackageManifest &manifest,
                              std::string *error = nullptr);

    static bool ParseFileRecords(const std::string &indexJson,
                                 std::vector<SolFileRecord> &records,
                                 std::string *error = nullptr);

    static bool ReadFile(const std::string &packagePath,
                         const SolPackageHeader &header,
                         const std::array<uint8_t, 32> &key,
                         const std::string &virtualPath,
                         std::vector<uint8_t> &output,
                         std::string *error = nullptr);

    static bool MatchesTitle(const SolPackageHeader &header, uint64_t titleId);
};

} // namespace Solar
