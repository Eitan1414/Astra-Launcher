#include "solar/SolPackageReader.hpp"
#include "solar/SolCrypto.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <zlib.h>

namespace Solar {
namespace {

constexpr char SolMagic[8] = {'A', 'S', 'T', 'R', 'A', 'S', 'O', 'L'};
constexpr size_t HeaderSize = 36;
constexpr uint32_t MaxIndexSize = 16u * 1024u * 1024u;

uint16_t ReadBE16(const uint8_t *data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                 static_cast<uint16_t>(data[1]));
}

uint32_t ReadBE32(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

uint64_t ReadBE64(const uint8_t *data) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<uint64_t>(data[i]);
    }
    return value;
}

void SetError(std::string *error, const char *message) {
    if (error != nullptr) {
        *error = message;
    }
}

size_t FindJsonValue(const std::string &json, const std::string &key, size_t start = 0) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t cursor = json.find(quotedKey, start);
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    cursor = json.find(':', cursor + quotedKey.size());
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    ++cursor;
    while (cursor < json.size() &&
           (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\r' || json[cursor] == '\n')) {
        ++cursor;
    }
    return cursor;
}

bool ExtractObject(const std::string &json,
                   const std::string &key,
                   std::string &object,
                   std::string *error) {
    object.clear();

    size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size() || json[cursor] != '{') {
        SetError(error, "SOL index does not contain a manifest object");
        return false;
    }

    const size_t objectStart = cursor;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];

        if (inString) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                object = json.substr(objectStart, cursor - objectStart + 1);
                return true;
            }
        }
    }

    SetError(error, "SOL manifest object is truncated");
    return false;
}

std::string ExtractJsonString(const std::string &json, const std::string &key) {
    size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size() || json[cursor] != '"') {
        return {};
    }

    ++cursor;
    std::string value;
    bool escaped = false;

    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];
        if (escaped) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        value.push_back(ch);
    }

    return value;
}

bool ExtractJsonBool(const std::string &json, const std::string &key, bool fallback) {
    const size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos) {
        return fallback;
    }
    if (json.compare(cursor, 4, "true") == 0) {
        return true;
    }
    if (json.compare(cursor, 5, "false") == 0) {
        return false;
    }
    return fallback;
}

int ExtractJsonInt(const std::string &json, const std::string &key, int fallback) {
    const size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos) {
        return fallback;
    }

    char *end = nullptr;
    const long value = std::strtol(json.c_str() + cursor, &end, 10);
    if (end == json.c_str() + cursor) {
        return fallback;
    }
    return static_cast<int>(value);
}

uint64_t ExtractJsonUInt64(const std::string &json, const std::string &key, uint64_t fallback) {
    const size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos) {
        return fallback;
    }

    char *end = nullptr;
    const unsigned long long value = std::strtoull(json.c_str() + cursor, &end, 10);
    if (end == json.c_str() + cursor) {
        return fallback;
    }
    return static_cast<uint64_t>(value);
}

bool ExtractArray(const std::string &json,
                  const std::string &key,
                  std::string &array,
                  std::string *error) {
    array.clear();

    size_t cursor = FindJsonValue(json, key);
    if (cursor == std::string::npos || cursor >= json.size() || json[cursor] != '[') {
        SetError(error, "SOL index does not contain a files array");
        return false;
    }

    const size_t start = cursor;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];

        if (inString) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                array = json.substr(start, cursor - start + 1);
                return true;
            }
        }
    }

    SetError(error, "SOL files array is truncated");
    return false;
}

bool ParseHexNonce(const std::string &hex,
                   std::array<uint8_t, 12> &nonce) {
    if (hex.size() != nonce.size() * 2) {
        return false;
    }

    auto hexValue = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    for (size_t i = 0; i < nonce.size(); ++i) {
        const int high = hexValue(hex[i * 2]);
        const int low = hexValue(hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        nonce[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool ReadPackageRange(const std::string &path,
                      uint64_t offset,
                      uint32_t size,
                      std::vector<uint8_t> &output,
                      std::string *error) {
    output.clear();

    struct stat info {};
    if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        SetError(error, "SOL package file does not exist");
        return false;
    }

    const uint64_t fileSize = static_cast<uint64_t>(info.st_size);
    if (offset > fileSize || static_cast<uint64_t>(size) > fileSize - offset) {
        SetError(error, "SOL file record extends beyond end of package");
        return false;
    }

    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        SetError(error, "could not open SOL package payload");
        return false;
    }

    if (fseek(file, static_cast<long>(offset), SEEK_SET) != 0) {
        fclose(file);
        SetError(error, "could not seek to SOL file record");
        return false;
    }

    output.resize(size);
    const size_t bytesRead = size == 0 ? 0 : fread(output.data(), 1, size, file);
    fclose(file);

    if (bytesRead != size) {
        output.clear();
        SetError(error, "could not read complete SOL file record");
        return false;
    }

    return true;
}

} // namespace

bool SolPackageReader::ReadHeader(const std::string &path,
                                  SolPackageHeader &header,
                                  std::string *error) {
    header = {};

    struct stat info {};
    if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        SetError(error, "package file does not exist");
        return false;
    }

    if (info.st_size < static_cast<off_t>(HeaderSize)) {
        SetError(error, "package is smaller than the SOL header");
        return false;
    }

    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        SetError(error, "could not open package");
        return false;
    }

    uint8_t raw[HeaderSize] = {};
    const size_t bytesRead = fread(raw, 1, sizeof(raw), file);
    fclose(file);

    if (bytesRead != sizeof(raw)) {
        SetError(error, "could not read complete SOL header");
        return false;
    }

    if (std::memcmp(raw, SolMagic, sizeof(SolMagic)) != 0) {
        SetError(error, "invalid SOL magic");
        return false;
    }

    header.formatVersion = ReadBE16(raw + 8);
    header.flags = ReadBE16(raw + 10);
    header.titleId = ReadBE64(raw + 12);
    std::memcpy(header.indexNonce.data(), raw + 20, header.indexNonce.size());
    header.indexSize = ReadBE32(raw + 32);
    header.indexOffset = HeaderSize;

    if (header.formatVersion != SupportedFormatVersion) {
        SetError(error, "unsupported SOL format version");
        return false;
    }

    if ((header.flags & FlagEncrypted) == 0) {
        SetError(error, "SOL v1 package is expected to be encrypted");
        return false;
    }

    if (header.indexSize == 0 || header.indexSize > MaxIndexSize) {
        SetError(error, "invalid SOL index size");
        return false;
    }

    const uint64_t minimumSize = header.indexOffset + static_cast<uint64_t>(header.indexSize);
    if (static_cast<uint64_t>(info.st_size) < minimumSize) {
        SetError(error, "SOL index extends beyond end of file");
        return false;
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}


bool SolPackageReader::ReadEncryptedIndex(const std::string &path,
                                          const SolPackageHeader &header,
                                          std::vector<uint8_t> &encryptedIndex,
                                          std::string *error) {
    encryptedIndex.clear();

    if (header.indexSize == 0 || header.indexSize > MaxIndexSize) {
        SetError(error, "invalid SOL index size");
        return false;
    }

    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        SetError(error, "could not open package index");
        return false;
    }

    if (fseek(file, static_cast<long>(header.indexOffset), SEEK_SET) != 0) {
        fclose(file);
        SetError(error, "could not seek to SOL index");
        return false;
    }

    encryptedIndex.resize(header.indexSize);
    const size_t bytesRead = fread(encryptedIndex.data(), 1, encryptedIndex.size(), file);
    fclose(file);

    if (bytesRead != encryptedIndex.size()) {
        encryptedIndex.clear();
        SetError(error, "could not read complete SOL index");
        return false;
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool SolPackageReader::DecryptIndex(const std::string &path,
                                    const SolPackageHeader &header,
                                    const std::array<uint8_t, 32> &key,
                                    std::string &indexJson,
                                    std::string *error) {
    indexJson.clear();

    std::vector<uint8_t> encryptedIndex;
    if (!ReadEncryptedIndex(path, header, encryptedIndex, error)) {
        return false;
    }

    static constexpr uint8_t IndexAad[] = {
        'A', 'S', 'T', 'R', 'A', 'S', 'O', 'L', '-', 'I', 'N', 'D', 'E', 'X', '-', 'v', '1'
    };

    std::vector<uint8_t> plaintext;
    if (!SolCrypto::DecryptChaCha20Poly1305(
            key.data(),
            header.indexNonce.data(),
            IndexAad,
            sizeof(IndexAad),
            encryptedIndex.data(),
            encryptedIndex.size(),
            plaintext,
            error)) {
        return false;
    }

    indexJson.assign(reinterpret_cast<const char *>(plaintext.data()), plaintext.size());

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool SolPackageReader::ParseManifest(const std::string &indexJson,
                                     SolPackageManifest &manifest,
                                     std::string *error) {
    manifest = {};

    std::string manifestObject;
    if (!ExtractObject(indexJson, "manifest", manifestObject, error)) {
        return false;
    }

    manifest.name = ExtractJsonString(manifestObject, "name");
    manifest.author = ExtractJsonString(manifestObject, "author");
    manifest.version = ExtractJsonString(manifestObject, "version");
    manifest.type = ExtractJsonString(manifestObject, "type");
    manifest.titleId = ExtractJsonString(manifestObject, "titleId");
    manifest.enabled = ExtractJsonBool(manifestObject, "enabled", true);
    manifest.priority = ExtractJsonInt(manifestObject, "priority", 0);

    if (manifest.name.empty()) {
        SetError(error, "SOL manifest is missing name");
        return false;
    }
    if (manifest.version.empty()) {
        SetError(error, "SOL manifest is missing version");
        return false;
    }
    if (manifest.titleId.empty()) {
        SetError(error, "SOL manifest is missing titleId");
        return false;
    }

    if (manifest.author.empty()) {
        manifest.author = "Unknown";
    }
    if (manifest.type.empty()) {
        manifest.type = "sol-package";
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool SolPackageReader::ParseFileRecords(const std::string &indexJson,
                                         std::vector<SolFileRecord> &records,
                                         std::string *error) {
    records.clear();

    std::string filesArray;
    if (!ExtractArray(indexJson, "files", filesArray, error)) {
        return false;
    }

    size_t cursor = 1;
    while (cursor + 1 < filesArray.size()) {
        while (cursor < filesArray.size() &&
               (filesArray[cursor] == ' ' || filesArray[cursor] == '\t' ||
                filesArray[cursor] == '\r' || filesArray[cursor] == '\n' ||
                filesArray[cursor] == ',')) {
            ++cursor;
        }

        if (cursor >= filesArray.size() || filesArray[cursor] == ']') {
            break;
        }

        if (filesArray[cursor] != '{') {
            SetError(error, "invalid SOL file record");
            return false;
        }

        const size_t objectStart = cursor;
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        size_t objectEnd = std::string::npos;

        for (; cursor < filesArray.size(); ++cursor) {
            const char ch = filesArray[cursor];

            if (inString) {
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (ch == '\\') {
                    escaped = true;
                    continue;
                }
                if (ch == '"') {
                    inString = false;
                }
                continue;
            }

            if (ch == '"') {
                inString = true;
                continue;
            }

            if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                --depth;
                if (depth == 0) {
                    objectEnd = cursor;
                    ++cursor;
                    break;
                }
            }
        }

        if (objectEnd == std::string::npos) {
            SetError(error, "truncated SOL file record");
            return false;
        }

        const std::string object = filesArray.substr(objectStart, objectEnd - objectStart + 1);
        SolFileRecord record;
        record.path = ExtractJsonString(object, "path");
        record.payloadOffset = ExtractJsonUInt64(object, "offset", UINT64_MAX);

        const uint64_t encryptedSize = ExtractJsonUInt64(object, "encryptedSize", UINT64_MAX);
        const uint64_t compressedSize = ExtractJsonUInt64(object, "compressedSize", UINT64_MAX);
        const uint64_t originalSize = ExtractJsonUInt64(object, "originalSize", UINT64_MAX);

        if (record.path.empty() ||
            record.payloadOffset == UINT64_MAX ||
            encryptedSize == UINT64_MAX || encryptedSize > UINT32_MAX ||
            compressedSize == UINT64_MAX || compressedSize > UINT32_MAX ||
            originalSize == UINT64_MAX || originalSize > UINT32_MAX) {
            SetError(error, "SOL file record has invalid metadata");
            return false;
        }

        record.encryptedSize = static_cast<uint32_t>(encryptedSize);
        record.compressedSize = static_cast<uint32_t>(compressedSize);
        record.originalSize = static_cast<uint32_t>(originalSize);
        record.compression = ExtractJsonString(object, "compression");
        record.encryption = ExtractJsonString(object, "encryption");

        const std::string nonceHex = ExtractJsonString(object, "nonce");
        if (!ParseHexNonce(nonceHex, record.nonce)) {
            SetError(error, "SOL file record has invalid nonce");
            return false;
        }

        records.push_back(std::move(record));
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool SolPackageReader::ReadFile(const std::string &packagePath,
                                const SolPackageHeader &header,
                                const std::array<uint8_t, 32> &key,
                                const std::string &virtualPath,
                                std::vector<uint8_t> &output,
                                std::string *error) {
    output.clear();

    std::string indexJson;
    if (!DecryptIndex(packagePath, header, key, indexJson, error)) {
        return false;
    }

    std::vector<SolFileRecord> records;
    if (!ParseFileRecords(indexJson, records, error)) {
        return false;
    }

    const SolFileRecord *record = nullptr;
    for (const auto &candidate : records) {
        if (candidate.path == virtualPath) {
            record = &candidate;
            break;
        }
    }

    if (record == nullptr) {
        SetError(error, "requested file is not present in SOL package");
        return false;
    }

    if (record->encryption != "chacha20-poly1305" ||
        record->compression != "zlib") {
        SetError(error, "unsupported SOL file encoding");
        return false;
    }

    const uint64_t payloadBase = header.indexOffset + static_cast<uint64_t>(header.indexSize);
    if (record->payloadOffset > UINT64_MAX - payloadBase) {
        SetError(error, "SOL payload offset overflow");
        return false;
    }

    std::vector<uint8_t> encrypted;
    if (!ReadPackageRange(packagePath,
                          payloadBase + record->payloadOffset,
                          record->encryptedSize,
                          encrypted,
                          error)) {
        return false;
    }

    const std::string aad = "ASTRASOL-FILE-v1:" + record->path;
    std::vector<uint8_t> compressed;
    if (!SolCrypto::DecryptChaCha20Poly1305(
            key.data(),
            record->nonce.data(),
            reinterpret_cast<const uint8_t *>(aad.data()),
            aad.size(),
            encrypted.data(),
            encrypted.size(),
            compressed,
            error)) {
        return false;
    }

    if (compressed.size() != record->compressedSize) {
        SetError(error, "SOL compressed file size does not match index");
        return false;
    }

    output.resize(record->originalSize);
    uLongf outputSize = static_cast<uLongf>(output.size());
    const int zlibResult = uncompress(
        output.data(),
        &outputSize,
        compressed.data(),
        static_cast<uLong>(compressed.size()));

    if (zlibResult != Z_OK || outputSize != record->originalSize) {
        output.clear();
        SetError(error, "SOL zlib decompression failed");
        return false;
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool SolPackageReader::MatchesTitle(const SolPackageHeader &header, uint64_t titleId) {
    return header.titleId == titleId;
}

} // namespace Solar
