#include "solar/SolPackageReader.hpp"
#include "solar/SolCrypto.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

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

bool SolPackageReader::MatchesTitle(const SolPackageHeader &header, uint64_t titleId) {
    return header.titleId == titleId;
}

} // namespace Solar
