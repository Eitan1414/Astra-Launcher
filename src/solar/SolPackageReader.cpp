#include "solar/SolPackageReader.hpp"

#include <cstdio>
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

bool SolPackageReader::MatchesTitle(const SolPackageHeader &header, uint64_t titleId) {
    return header.titleId == titleId;
}

} // namespace Solar
