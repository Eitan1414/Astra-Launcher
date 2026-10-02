#include "solar/SolVirtualDevice.hpp"

#include "solar/Logger.hpp"

#include <algorithm>
#include <cerrno>
#include <content_redirection/defines.h>
#include <content_redirection/redirection.h>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <vector>

namespace Solar::SolVirtualDevice {
namespace {

constexpr const char *DeviceName = "astrasol";
constexpr const char *DevicePrefix = "astrasol:";

struct VirtualEntry {
    std::string virtualPath;
    std::string packagePath;
    std::string packageFilePath;
    SolPackageHeader header;
    std::array<uint8_t, 32> key {};
    uint32_t originalSize = 0;
};

struct FileHandle {
    std::vector<uint8_t> *data = nullptr;
    size_t offset = 0;
};

std::recursive_mutex gMutex;
std::vector<VirtualEntry> gEntries;
bool gRegistered = false;
ContentRedirectionDeviceABI gDevice {};

std::string NormalizePath(const char *path) {
    if (path == nullptr) {
        return {};
    }

    std::string result = path;
    const std::string prefix = DevicePrefix;
    if (result.compare(0, prefix.size(), prefix) == 0) {
        result.erase(0, prefix.size());
    }

    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
    }

    return result;
}

bool FindEntry(const std::string &path, VirtualEntry &outEntry) {
    const std::string normalized = NormalizePath(path.c_str());
    std::lock_guard<std::recursive_mutex> lock(gMutex);

    const auto it = std::find_if(gEntries.begin(), gEntries.end(), [&](const VirtualEntry &entry) {
        return entry.virtualPath == normalized;
    });

    if (it == gEntries.end()) {
        return false;
    }

    outEntry = *it;
    return true;
}

void FillStat(CR_Stat *st, uint32_t size) {
    if (st == nullptr) {
        return;
    }

    std::memset(st, 0, sizeof(*st));
    st->mode = S_IFREG | 0444;
    st->nlink = 1;
    st->size = static_cast<int64_t>(size);
    st->blksize = 4096;
    st->blocks = (static_cast<int64_t>(size) + 511) / 512;
}

int DeviceOpen(void *, void *fileStruct, const char *path, int flags, uint32_t) {
    if (fileStruct == nullptr || path == nullptr) {
        return -EINVAL;
    }

    if ((flags & O_ACCMODE) != O_RDONLY) {
        return -EROFS;
    }

    VirtualEntry entry;
    if (!FindEntry(path, entry)) {
        return -ENOENT;
    }

    std::vector<uint8_t> data;
    std::string error;
    if (!SolPackageReader::ReadFile(
            entry.packageFilePath,
            entry.header,
            entry.key,
            entry.packagePath,
            data,
            &error)) {
        Logger::Error("SOL virtual device: failed to open %s: %s",
                      entry.packagePath.c_str(),
                      error.c_str());
        return -EIO;
    }

    auto *handle = static_cast<FileHandle *>(fileStruct);
    handle->data = new (std::nothrow) std::vector<uint8_t>(std::move(data));
    handle->offset = 0;

    if (handle->data == nullptr) {
        return -ENOMEM;
    }

    return 0;
}

int DeviceClose(void *, void *fd) {
    if (fd == nullptr) {
        return -EINVAL;
    }

    auto *handle = static_cast<FileHandle *>(fd);
    delete handle->data;
    handle->data = nullptr;
    handle->offset = 0;
    return 0;
}

ssize_t DeviceRead(void *, void *fd, char *ptr, size_t len) {
    if (fd == nullptr || ptr == nullptr) {
        return -EINVAL;
    }

    auto *handle = static_cast<FileHandle *>(fd);
    if (handle->data == nullptr) {
        return -EBADF;
    }

    const size_t size = handle->data->size();
    if (handle->offset >= size) {
        return 0;
    }

    const size_t remaining = size - handle->offset;
    const size_t toRead = std::min(len, remaining);
    if (toRead > 0) {
        std::memcpy(ptr, handle->data->data() + handle->offset, toRead);
        handle->offset += toRead;
    }

    return static_cast<ssize_t>(toRead);
}

int64_t DeviceSeek(void *, void *fd, int64_t pos, int dir) {
    if (fd == nullptr) {
        return -EINVAL;
    }

    auto *handle = static_cast<FileHandle *>(fd);
    if (handle->data == nullptr) {
        return -EBADF;
    }

    int64_t base = 0;
    switch (dir) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = static_cast<int64_t>(handle->offset);
            break;
        case SEEK_END:
            base = static_cast<int64_t>(handle->data->size());
            break;
        default:
            return -EINVAL;
    }

    if ((pos > 0 && base > INT64_MAX - pos) ||
        (pos < 0 && base < INT64_MIN - pos)) {
        return -EOVERFLOW;
    }

    const int64_t next = base + pos;
    if (next < 0) {
        return -EINVAL;
    }

    handle->offset = static_cast<size_t>(next);
    return next;
}

int DeviceFStat(void *, void *fd, CR_Stat *st) {
    if (fd == nullptr || st == nullptr) {
        return -EINVAL;
    }

    auto *handle = static_cast<FileHandle *>(fd);
    if (handle->data == nullptr) {
        return -EBADF;
    }

    if (handle->data->size() > UINT32_MAX) {
        return -EOVERFLOW;
    }

    FillStat(st, static_cast<uint32_t>(handle->data->size()));
    return 0;
}

int DeviceStat(void *, const char *file, CR_Stat *st) {
    if (file == nullptr || st == nullptr) {
        return -EINVAL;
    }

    VirtualEntry entry;
    if (!FindEntry(file, entry)) {
        return -ENOENT;
    }

    FillStat(st, entry.originalSize);
    return 0;
}

} // namespace

bool Initialize() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);

    if (gRegistered) {
        return true;
    }

    gDevice = {};
    gDevice.magic = CONTENT_REDIRECTION_DEVICE_MAGIC;
    gDevice.version = CONTENT_REDIRECTION_DEVICE_VERSION;
    gDevice.name = DeviceName;
    gDevice.structSize = sizeof(FileHandle);
    gDevice.dirStateSize = 0;
    gDevice.deviceData = nullptr;
    gDevice.open = &DeviceOpen;
    gDevice.close = &DeviceClose;
    gDevice.read = &DeviceRead;
    gDevice.seek = &DeviceSeek;
    gDevice.fstat = &DeviceFStat;
    gDevice.stat = &DeviceStat;

    int resultOut = -1;
    const ContentRedirectionStatus result = ContentRedirection_AddDeviceABI(&gDevice, &resultOut);
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS || resultOut < 0) {
        Logger::Warn("SOL virtual device registration failed: %s (%d), module result=%d",
                     ContentRedirection_GetStatusStr(result),
                     result,
                     resultOut);
        return false;
    }

    gRegistered = true;
    Logger::Info("SOL virtual device registered as %s", DevicePrefix);
    return true;
}

void Clear() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    gEntries.clear();
}

void Shutdown() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);

    gEntries.clear();

    if (!gRegistered) {
        return;
    }

    int resultOut = -1;
    const ContentRedirectionStatus result = ContentRedirection_RemoveDeviceABI(DevicePrefix, &resultOut);
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS || resultOut < 0) {
        Logger::Warn("SOL virtual device removal returned %s (%d), module result=%d",
                     ContentRedirection_GetStatusStr(result),
                     result,
                     resultOut);
    }

    gRegistered = false;
}

bool IsAvailable() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    return gRegistered;
}

bool RegisterPackage(const std::string &packagePath,
                     const SolPackageHeader &header,
                     const std::array<uint8_t, 32> &key,
                     const std::string &mountId,
                     const std::string &indexJson,
                     std::vector<MountedFile> &mountedFiles,
                     std::string *error) {
    mountedFiles.clear();

    if (!gRegistered) {
        if (error != nullptr) {
            *error = "SOL virtual device is not registered";
        }
        return false;
    }

    std::vector<SolFileRecord> records;
    if (!SolPackageReader::ParseFileRecords(indexJson, records, error)) {
        return false;
    }

    if (mountId.empty()) {
        if (error != nullptr) {
            *error = "SOL mount id is empty";
        }
        return false;
    }

    std::vector<VirtualEntry> pending;
    pending.reserve(records.size());
    mountedFiles.reserve(records.size());

    for (const auto &record : records) {
        VirtualEntry entry;
        entry.virtualPath = mountId + "/" + record.path;
        entry.packagePath = record.path;
        entry.packageFilePath = packagePath;
        entry.header = header;
        entry.key = key;
        entry.originalSize = record.originalSize;
        pending.push_back(entry);

        MountedFile mounted;
        mounted.packagePath = record.path;
        mounted.devicePath = std::string(DevicePrefix) + "/" + entry.virtualPath;
        mounted.originalSize = record.originalSize;
        mountedFiles.push_back(std::move(mounted));
    }

    {
        std::lock_guard<std::recursive_mutex> lock(gMutex);
        for (const auto &entry : pending) {
            const auto duplicate = std::find_if(
                gEntries.begin(),
                gEntries.end(),
                [&](const VirtualEntry &existing) {
                    return existing.virtualPath == entry.virtualPath;
                });
            if (duplicate != gEntries.end()) {
                if (error != nullptr) {
                    *error = "duplicate SOL virtual path";
                }
                return false;
            }
        }

        gEntries.insert(gEntries.end(), pending.begin(), pending.end());
    }

    if (error != nullptr) {
        error->clear();
    }

    Logger::Info("SOL virtual device mounted %u file(s) from %s as %s",
                 static_cast<unsigned int>(mountedFiles.size()),
                 packagePath.c_str(),
                 mountId.c_str());
    return true;
}

} // namespace Solar::SolVirtualDevice
