#pragma once

#include "solar/SolPackageReader.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Solar::SolVirtualDevice {

struct MountedFile {
    std::string packagePath;
    std::string devicePath;
    uint32_t originalSize = 0;
};

bool Initialize();
void Shutdown();
void Clear();
bool IsAvailable();

bool RegisterPackage(const std::string &packagePath,
                     const SolPackageHeader &header,
                     const std::array<uint8_t, 32> &key,
                     const std::string &mountId,
                     const std::string &indexJson,
                     std::vector<MountedFile> &mountedFiles,
                     std::string *error = nullptr);

} // namespace Solar::SolVirtualDevice
