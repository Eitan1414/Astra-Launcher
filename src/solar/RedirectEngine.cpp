#include "solar/RedirectEngine.hpp"
#include "solar/Logger.hpp"
#include "solar/SolKeyProvider.hpp"
#include "solar/SolPackageReader.hpp"
#include "solar/SolVirtualDevice.hpp"

#include <algorithm>
#include <content_redirection/redirection.h>
#include <array>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace Solar::RedirectEngine {
namespace {

bool gInitialized = false;
std::vector<CRLayerHandle> gLayers;

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

const char *ChoosePackDirectory(const ModInfo &mod, const char *primary, const char *alias) {
    if (IsDirectory(mod.path + "/" + primary)) {
        return primary;
    }
    if (IsDirectory(mod.path + "/" + alias)) {
        return alias;
    }
    return nullptr;
}

bool StartsWith(const std::string &value, const char *prefix) {
    const size_t prefixLength = std::strlen(prefix);
    return value.size() >= prefixLength &&
           value.compare(0, prefixLength, prefix) == 0;
}

bool SolTargetPath(const std::string &packagePath, std::string &targetPath) {
    struct Mapping {
        const char *sourcePrefix;
        const char *targetPrefix;
    };

    static constexpr Mapping mappings[] = {
        {"content/", "/vol/content/"},
        {"aoc/", "/vol/aoc/"},
        {"textures/", "/vol/content/"},
        {"texture_pack/", "/vol/content/"},
        {"behavior/", "/vol/content/"},
        {"behavior_pack/", "/vol/content/"},
    };

    for (const auto &mapping : mappings) {
        if (StartsWith(packagePath, mapping.sourcePrefix)) {
            targetPath = mapping.targetPrefix +
                         packagePath.substr(std::strlen(mapping.sourcePrefix));
            return true;
        }
    }

    return false;
}

bool AddSolPackage(const ModInfo &mod, unsigned int mountIndex) {
    if (!SolVirtualDevice::IsAvailable()) {
        Logger::Warn("SOL package %s cannot be redirected: virtual device unavailable",
                     mod.name.c_str());
        return false;
    }

    std::array<uint8_t, 32> key {};
    std::string error;
    if (!SolKeyProvider::GetKey(key, &error)) {
        Logger::Warn("SOL package %s cannot be redirected: %s",
                     mod.name.c_str(), error.c_str());
        return false;
    }

    SolPackageHeader header;
    if (!SolPackageReader::ReadHeader(mod.path, header, &error)) {
        Logger::Warn("SOL package %s header could not be read: %s",
                     mod.name.c_str(), error.c_str());
        return false;
    }

    std::string indexJson;
    if (!SolPackageReader::DecryptIndex(mod.path, header, key, indexJson, &error)) {
        Logger::Warn("SOL package %s index could not be decrypted: %s",
                     mod.name.c_str(), error.c_str());
        return false;
    }

    std::vector<SolVirtualDevice::MountedFile> files;
    const std::string mountId = "mod" + std::to_string(mountIndex);
    if (!SolVirtualDevice::RegisterPackage(
            mod.path, header, key, mountId, indexJson, files, &error)) {
        Logger::Warn("SOL package %s could not be mounted: %s",
                     mod.name.c_str(), error.c_str());
        return false;
    }

    bool applied = false;
    for (const auto &file : files) {
        std::string targetPath;
        if (!SolTargetPath(file.packagePath, targetPath)) {
            continue;
        }

        CRLayerHandle handle = 0;
        const std::string layerName =
            "Astra SOL: " + mod.name + " " + targetPath;

        const ContentRedirectionStatus result = ContentRedirection_AddFSLayerEx(
            &handle,
            layerName.c_str(),
            targetPath.c_str(),
            file.devicePath.c_str(),
            FS_LAYER_TYPE_EX_REPLACE_FILE);

        if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
            Logger::Warn("SOL redirect failed %s <- %s: %s (%d)",
                         targetPath.c_str(),
                         file.devicePath.c_str(),
                         ContentRedirection_GetStatusStr(result),
                         result);
            continue;
        }

        gLayers.push_back(handle);
        applied = true;
        Logger::Info("SOL redirect: %s <- %s (%u bytes)",
                     targetPath.c_str(),
                     file.devicePath.c_str(),
                     static_cast<unsigned int>(file.originalSize));
    }

    if (!applied) {
        Logger::Warn("SOL package %s contained no redirectable content/aoc files",
                     mod.name.c_str());
    }

    return applied;
}

bool AddLayer(const ModInfo &mod,
              const char *sourceSubdir,
              const char *targetLabel,
              FSLayerType layerType) {
    if (sourceSubdir == nullptr) {
        return false;
    }

    const std::string replacementPath = mod.path + "/" + sourceSubdir;
    if (!IsDirectory(replacementPath)) {
        return false;
    }

    CRLayerHandle handle = 0;
    const std::string layerName = "Solar: " + mod.name + " /vol/" + targetLabel + " <- " + sourceSubdir;
    const ContentRedirectionStatus result = ContentRedirection_AddFSLayer(
        &handle, layerName.c_str(), replacementPath.c_str(), layerType);

    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Error("Failed to add %s layer for %s: %s (%d)", sourceSubdir, mod.name.c_str(),
                      ContentRedirection_GetStatusStr(result), result);
        return false;
    }

    gLayers.push_back(handle);
    Logger::Info("Redirecting /vol/%s with %s (payload=%s priority=%d%s)",
                 targetLabel,
                 replacementPath.c_str(),
                 sourceSubdir,
                 mod.priority,
                 mod.legacySDCafiine ? ", SDCafiine" : "");
    return true;
}

} // namespace

bool Initialize() {
    if (gInitialized) {
        return true;
    }

    const ContentRedirectionStatus result = ContentRedirection_InitLibrary();
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Warn("ContentRedirection unavailable: %s (%d). Games will launch without file mods.",
                     ContentRedirection_GetStatusStr(result), result);
        return false;
    }

    gInitialized = true;
    Logger::Info("ContentRedirection initialized");

    if (!SolVirtualDevice::Initialize()) {
        Logger::Warn("SOL virtual file redirection is unavailable; folder-based mods still work");
    }

    return true;
}

void Clear() {
    if (!gInitialized) {
        gLayers.clear();
        return;
    }

    for (auto it = gLayers.rbegin(); it != gLayers.rend(); ++it) {
        const ContentRedirectionStatus result = ContentRedirection_RemoveFSLayer(*it);
        if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
            Logger::Warn("Failed to remove redirection layer %u: %s (%d)",
                         static_cast<unsigned int>(*it), ContentRedirection_GetStatusStr(result), result);
        }
    }

    gLayers.clear();
    SolVirtualDevice::Clear();
}

void Shutdown() {
    if (!gInitialized) {
        return;
    }

    Clear();
    SolVirtualDevice::Shutdown();

    const ContentRedirectionStatus result = ContentRedirection_DeInitLibrary();
    if (result != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        Logger::Warn("ContentRedirection deinit returned %s (%d)",
                     ContentRedirection_GetStatusStr(result), result);
    }

    gInitialized = false;
}

bool IsAvailable() {
    return gInitialized;
}

size_t Apply(const std::vector<ModInfo> &mods) {
    Clear();

    if (!gInitialized) {
        return 0;
    }

    std::vector<const ModInfo *> enabledMods;
    enabledMods.reserve(mods.size());

    for (const auto &mod : mods) {
        if (!mod.enabled ||
            (!mod.solPackage &&
             !mod.hasContent && !mod.hasAoc && !mod.hasTexturePack && !mod.hasBehaviorPack)) {
            continue;
        }
        enabledMods.push_back(&mod);
    }

    std::stable_sort(enabledMods.begin(), enabledMods.end(), [](const ModInfo *lhs, const ModInfo *rhs) {
        if (lhs->priority != rhs->priority) {
            return lhs->priority < rhs->priority;
        }
        return lhs->name < rhs->name;
    });

    size_t appliedMods = 0;
    unsigned int solMountIndex = 0;
    for (const ModInfo *mod : enabledMods) {
        bool applied = false;

        if (mod->solPackage) {
            applied |= AddSolPackage(*mod, solMountIndex++);
        } else {
            if (mod->hasContent) {
                applied |= AddLayer(*mod, "content", "content", FS_LAYER_TYPE_CONTENT_MERGE);
            }
            if (mod->hasTexturePack) {
                const char *dir = ChoosePackDirectory(*mod, "textures", "texture_pack");
                applied |= AddLayer(*mod, dir, "content", FS_LAYER_TYPE_CONTENT_MERGE);
            }
            if (mod->hasBehaviorPack) {
                const char *dir = ChoosePackDirectory(*mod, "behavior", "behavior_pack");
                applied |= AddLayer(*mod, dir, "content", FS_LAYER_TYPE_CONTENT_MERGE);
            }
            if (mod->hasAoc) {
                applied |= AddLayer(*mod, "aoc", "aoc", FS_LAYER_TYPE_AOC_MERGE);
            }
        }

        if (applied) {
            ++appliedMods;
        }
    }

    Logger::Info("Applied %u file/pack mod(s) using %u redirection layer(s)",
                 static_cast<unsigned int>(appliedMods),
                 static_cast<unsigned int>(gLayers.size()));
    return appliedMods;
}

} // namespace Solar::RedirectEngine
