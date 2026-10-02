#include "solar/ModManager.hpp"
#include "solar/Logger.hpp"
#include "solar/Paths.hpp"
#include "solar/SolKeyProvider.hpp"
#include "solar/SolPackageReader.hpp"
#include "solar/TitleManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <utility>

namespace Solar {
namespace {

constexpr long MaxManifestSize = 64 * 1024;
constexpr int LegacySDCafiinePriority = -1000;
constexpr uint64_t CupheadTitleId = 0x0005000021000000ULL;

bool IsDirectory(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool FileExists(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool IsSolFilename(const std::string &name) {
    if (name.size() < 4) {
        return false;
    }

    std::string extension = name.substr(name.size() - 4);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".sol";
}

std::string SolDisplayName(const std::string &name) {
    return name.size() > 4 ? name.substr(0, name.size() - 4) : name;
}

std::string ReadTextFile(const std::string &path) {
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return {};
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return {};
    }

    const long size = ftell(file);
    if (size < 0 || size > MaxManifestSize) {
        fclose(file);
        return {};
    }

    rewind(file);
    std::string result(static_cast<size_t>(size), '\0');
    if (size > 0) {
        const size_t bytesRead = fread(result.data(), 1, static_cast<size_t>(size), file);
        result.resize(bytesRead);
    }

    fclose(file);
    return result;
}

size_t FindJsonValue(const std::string &json, const std::string &key) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t cursor = json.find(quotedKey);
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    cursor = json.find(':', cursor + quotedKey.size());
    if (cursor == std::string::npos) {
        return std::string::npos;
    }

    ++cursor;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
        ++cursor;
    }
    return cursor;
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
    if (cursor == std::string::npos || cursor >= json.size()) {
        return fallback;
    }

    char *end = nullptr;
    const long value = std::strtol(json.c_str() + cursor, &end, 10);
    if (end == json.c_str() + cursor) {
        return fallback;
    }
    return static_cast<int>(value);
}

std::string NormalizeTitleId(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isxdigit(ch);
    }), value.end());

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    return value;
}

void PopulatePayloadFlags(ModInfo &mod, uint64_t titleId) {
    mod.hasContent = IsDirectory(mod.path + "/content");
    mod.hasAoc = IsDirectory(mod.path + "/aoc");
    mod.hasPatches = IsDirectory(mod.path + "/patches");

    // Cuphead pack aliases. Pack folders mirror /vol/content so creators do not
    // need to call every visual replacement a generic "content" mod.
    if (titleId == CupheadTitleId && !mod.legacySDCafiine) {
        mod.hasTexturePack =
            IsDirectory(mod.path + "/textures") ||
            IsDirectory(mod.path + "/texture_pack");
        mod.hasBehaviorPack =
            IsDirectory(mod.path + "/behavior") ||
            IsDirectory(mod.path + "/behavior_pack");
    }
}

ModInfo ParseManifest(uint64_t titleId,
                      const std::string &directoryName,
                      const std::string &directoryPath,
                      const std::string &manifest) {
    ModInfo mod;
    mod.directoryName = directoryName;
    mod.path = directoryPath;
    mod.name = ExtractJsonString(manifest, "name");
    mod.author = ExtractJsonString(manifest, "author");
    mod.version = ExtractJsonString(manifest, "version");
    mod.type = ExtractJsonString(manifest, "type");
    mod.declaredTitleId = ExtractJsonString(manifest, "titleId");
    mod.enabled = ExtractJsonBool(manifest, "enabled", true);
    mod.priority = ExtractJsonInt(manifest, "priority", 0);
    mod.defaultEnabled = mod.enabled;
    mod.defaultPriority = mod.priority;

    if (mod.name.empty()) {
        mod.name = directoryName;
    }
    if (mod.author.empty()) {
        mod.author = "Unknown";
    }
    if (mod.version.empty()) {
        mod.version = "Unknown";
    }

    PopulatePayloadFlags(mod, titleId);

    if (mod.type.empty()) {
        if (mod.hasTexturePack && mod.hasBehaviorPack) {
            mod.type = "cuphead-pack";
        } else if (mod.hasTexturePack) {
            mod.type = "texture-pack";
        } else if (mod.hasBehaviorPack) {
            mod.type = "behavior-pack";
        } else {
            mod.type = "unknown";
        }
    }

    return mod;
}

void ScanSolarMods(uint64_t titleId, std::vector<ModInfo> &mods) {
    const std::string currentTitleId = TitleManager::FormatTitleId(titleId);
    const std::string root = Paths::TitleDirectory(titleId);

    DIR *directory = opendir(root.c_str());
    if (directory == nullptr) {
        Logger::Info("No Solar mod directory for title %s", currentTitleId.c_str());
        return;
    }

    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name.empty() || name[0] == '.') {
            continue;
        }

        const std::string modPath = root + "/" + name;

        if (FileExists(modPath) && IsSolFilename(name)) {
            SolPackageHeader packageHeader;
            std::string packageError;
            if (!SolPackageReader::ReadHeader(modPath, packageHeader, &packageError)) {
                Logger::Warn("Skipping SOL package %s: %s", modPath.c_str(), packageError.c_str());
                continue;
            }

            if (!SolPackageReader::MatchesTitle(packageHeader, titleId)) {
                Logger::Warn("Skipping SOL package %s: Title ID %016llX does not match %s",
                             modPath.c_str(),
                             static_cast<unsigned long long>(packageHeader.titleId),
                             currentTitleId.c_str());
                continue;
            }

            ModInfo mod;
            mod.directoryName = name;
            mod.path = modPath;
            mod.name = SolDisplayName(name);
            mod.author = "Encrypted package";
            mod.version = "SOL v" + std::to_string(packageHeader.formatVersion);
            mod.type = "sol-package";
            mod.declaredTitleId = currentTitleId;
            mod.enabled = false;
            mod.priority = 0;
            mod.defaultEnabled = false;
            mod.defaultPriority = 0;
            mod.solPackage = true;
            mod.solFormatVersion = packageHeader.formatVersion;

            std::array<uint8_t, 32> solKey {};
            std::string keyError;
            if (!SolKeyProvider::GetKey(solKey, &keyError)) {
                Logger::Warn("SOL package %s detected but metadata is locked: %s",
                             mod.name.c_str(), keyError.c_str());
                mods.push_back(std::move(mod));
                continue;
            }

            std::string indexJson;
            std::string indexError;
            if (!SolPackageReader::DecryptIndex(modPath, packageHeader, solKey, indexJson, &indexError)) {
                Logger::Warn("Skipping SOL package %s: %s", modPath.c_str(), indexError.c_str());
                continue;
            }

            SolPackageManifest packageManifest;
            std::string manifestError;
            if (!SolPackageReader::ParseManifest(indexJson, packageManifest, &manifestError)) {
                Logger::Warn("Skipping SOL package %s: %s", modPath.c_str(), manifestError.c_str());
                continue;
            }

            if (!packageManifest.titleId.empty()) {
                const std::string declared = NormalizeTitleId(packageManifest.titleId);
                if (declared != currentTitleId) {
                    Logger::Warn("Skipping SOL package %s: manifest Title ID %s does not match %s",
                                 packageManifest.name.c_str(),
                                 declared.c_str(),
                                 currentTitleId.c_str());
                    continue;
                }
            }

            mod.name = packageManifest.name;
            mod.author = packageManifest.author;
            mod.version = packageManifest.version;
            mod.type = packageManifest.type;
            mod.declaredTitleId = packageManifest.titleId;
            mod.enabled = packageManifest.enabled;
            mod.priority = packageManifest.priority;
            mod.defaultEnabled = mod.enabled;
            mod.defaultPriority = mod.priority;

            Logger::Info("Astra SOL package ready: %s v%s by %s (format v%u)",
                         mod.name.c_str(),
                         mod.version.c_str(),
                         mod.author.c_str(),
                         static_cast<unsigned int>(mod.solFormatVersion));
            mods.push_back(std::move(mod));
            continue;
        }

        if (!IsDirectory(modPath)) {
            continue;
        }

        const std::string manifestPath = modPath + "/mod.json";
        if (!FileExists(manifestPath)) {
            Logger::Warn("Skipping %s: missing mod.json", modPath.c_str());
            continue;
        }

        const std::string manifest = ReadTextFile(manifestPath);
        if (manifest.empty()) {
            Logger::Warn("Skipping %s: mod.json is empty or unreadable", modPath.c_str());
            continue;
        }

        ModInfo mod = ParseManifest(titleId, name, modPath, manifest);
        if (!mod.declaredTitleId.empty()) {
            const std::string declared = NormalizeTitleId(mod.declaredTitleId);
            if (declared != currentTitleId) {
                Logger::Warn("Skipping %s: manifest Title ID %s does not match %s",
                             mod.name.c_str(), declared.c_str(), currentTitleId.c_str());
                continue;
            }
        }

        if (mod.hasTexturePack) {
            Logger::Info("Cuphead texture pack detected: %s", mod.name.c_str());
        }
        if (mod.hasBehaviorPack) {
            Logger::Info("Cuphead behavior pack detected: %s", mod.name.c_str());
        }

        mods.push_back(std::move(mod));
    }

    closedir(directory);
}

void ScanLegacySDCafiineMods(uint64_t titleId, std::vector<ModInfo> &mods) {
    const std::string root = Paths::SDCafiineTitleDirectory(titleId);
    DIR *directory = opendir(root.c_str());
    if (directory == nullptr) {
        return;
    }

    std::vector<ModInfo> legacyMods;
    while (dirent *entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name.empty() || name[0] == '.') {
            continue;
        }

        const std::string modPath = root + "/" + name;
        if (!IsDirectory(modPath)) {
            continue;
        }

        ModInfo mod;
        mod.directoryName = name;
        mod.path = modPath;
        mod.name = name;
        mod.author = "Unknown";
        mod.version = "SDCafiine legacy";
        mod.type = "sdcafiine";
        mod.declaredTitleId = TitleManager::FormatTitleId(titleId);
        mod.enabled = false;
        mod.priority = LegacySDCafiinePriority;
        mod.legacySDCafiine = true;
        PopulatePayloadFlags(mod, titleId);

        if (!mod.hasContent && !mod.hasAoc) {
            continue;
        }

        legacyMods.push_back(std::move(mod));
    }

    closedir(directory);

    if (legacyMods.size() == 1) {
        legacyMods[0].enabled = true;
        Logger::Info("Auto-enabled single SDCafiine pack: %s", legacyMods[0].name.c_str());
    } else if (legacyMods.size() > 1) {
        Logger::Info("Detected %u SDCafiine packs; Solar selector will let the user choose.",
                     static_cast<unsigned int>(legacyMods.size()));
    }

    for (auto &mod : legacyMods) {
        mod.defaultEnabled = mod.enabled;
        mod.defaultPriority = mod.priority;
        mods.push_back(std::move(mod));
    }
}

} // namespace

std::vector<ModInfo> ModManager::ScanForTitle(uint64_t titleId, bool includeLegacySDCafiine) {
    std::vector<ModInfo> mods;
    ScanSolarMods(titleId, mods);

    if (includeLegacySDCafiine) {
        ScanLegacySDCafiineMods(titleId, mods);
    }

    return mods;
}

} // namespace Solar
