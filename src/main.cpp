#include "solar/ConflictDetector.hpp"
#include "solar/GameAdapterRegistry.hpp"
#include "solar/Logger.hpp"
#include "solar/ModManager.hpp"
#include "solar/ModMenu.hpp"
#include "solar/PatchEngine.hpp"
#include "solar/Paths.hpp"
#include "solar/RedirectEngine.hpp"
#include "solar/SelectionStore.hpp"
#include "solar/TitleManager.hpp"

#include <coreinit/debug.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

WUPS_PLUGIN_NAME("Astra Launcher");
WUPS_PLUGIN_DESCRIPTION("Astra universal Wii U modding framework for Aroma.");
WUPS_PLUGIN_VERSION("v0.6.0-dev");
WUPS_PLUGIN_AUTHOR("Eitan1414");
WUPS_PLUGIN_LICENSE("Unlicensed");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("solar_launcher");

namespace {

constexpr const char *EnabledConfigId = "enabled";
constexpr const char *FileModsConfigId = "file_mods_enabled";
constexpr const char *PatchModsConfigId = "patch_mods_enabled";
constexpr const char *LegacySDCafiineConfigId = "legacy_sdcafiine_enabled";
constexpr const char *PreLaunchMenuConfigId = "prelaunch_menu_enabled";

constexpr bool DefaultEnabled = true;
constexpr bool DefaultFileModsEnabled = true;
constexpr bool DefaultPatchModsEnabled = true;
constexpr bool DefaultLegacySDCafiineEnabled = true;
constexpr bool DefaultPreLaunchMenuEnabled = true;

bool gEnabled = DefaultEnabled;
bool gFileModsEnabled = DefaultFileModsEnabled;
bool gPatchModsEnabled = DefaultPatchModsEnabled;
bool gLegacySDCafiineEnabled = DefaultLegacySDCafiineEnabled;
bool gPreLaunchMenuEnabled = DefaultPreLaunchMenuEnabled;

// Keep the pre-launch menu lifetime separate from the runtime-hook lifetime.
// Cuphead can tear down/recreate application/runtime state when restarting a boss
// or returning to the map. Those transitions may produce new WUPS application
// callbacks, but they are still part of the same user-visible Cuphead launch.
bool gMenuSessionActive = false;
uint64_t gMenuSessionTitleId = 0;
uint32_t gLastSessionUpid = 0;
bool gSessionVanilla = false;

void ResetMenuSessionState() {
    gMenuSessionActive = false;
    gMenuSessionTitleId = 0;
    gLastSessionUpid = 0;
    gSessionVanilla = false;
}

bool IsSameMenuSession(uint64_t titleId) {
    return gMenuSessionActive && gMenuSessionTitleId == titleId;
}

void StoreBool(const char *key, bool value) {
    if (WUPSStorageAPI_StoreBool(nullptr, key, value) != WUPS_STORAGE_ERROR_SUCCESS) {
        Solar::Logger::Error("Failed to store setting %s", key);
    }
}

void EnabledChanged(ConfigItemBoolean *, bool newValue) {
    gEnabled = newValue;
    StoreBool(EnabledConfigId, gEnabled);
}

void FileModsChanged(ConfigItemBoolean *, bool newValue) {
    gFileModsEnabled = newValue;
    StoreBool(FileModsConfigId, gFileModsEnabled);
}

void PatchModsChanged(ConfigItemBoolean *, bool newValue) {
    gPatchModsEnabled = newValue;
    StoreBool(PatchModsConfigId, gPatchModsEnabled);
}

void LegacySDCafiineChanged(ConfigItemBoolean *, bool newValue) {
    gLegacySDCafiineEnabled = newValue;
    StoreBool(LegacySDCafiineConfigId, gLegacySDCafiineEnabled);
}

void PreLaunchMenuChanged(ConfigItemBoolean *, bool newValue) {
    gPreLaunchMenuEnabled = newValue;
    StoreBool(PreLaunchMenuConfigId, gPreLaunchMenuEnabled);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle root) {
    if (WUPSConfigItemBoolean_AddToCategory(root, EnabledConfigId, "Enable Astra Launcher",
                                            DefaultEnabled, gEnabled, &EnabledChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, FileModsConfigId, "Enable file replacement mods",
                                            DefaultFileModsEnabled, gFileModsEnabled, &FileModsChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, PatchModsConfigId, "Enable memory/native patch mods",
                                            DefaultPatchModsEnabled, gPatchModsEnabled, &PatchModsChanged) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, LegacySDCafiineConfigId, "Detect SDCafiine packs",
                                            DefaultLegacySDCafiineEnabled, gLegacySDCafiineEnabled,
                                            &LegacySDCafiineChanged) != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    if (WUPSConfigItemBoolean_AddToCategory(root, PreLaunchMenuConfigId, "Show pre-launch mod menu",
                                            DefaultPreLaunchMenuEnabled, gPreLaunchMenuEnabled,
                                            &PreLaunchMenuChanged) != WUPSCONFIG_API_RESULT_SUCCESS) {
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI_SaveStorage(false);
}

void LoadBoolSetting(const char *key, bool defaultValue, bool &target) {
    const WUPSStorageError result = WUPSStorageAPI_GetBool(nullptr, key, &target);
    if (result == WUPS_STORAGE_ERROR_NOT_FOUND) {
        target = defaultValue;
        WUPSStorageAPI_StoreBool(nullptr, key, target);
    } else if (result != WUPS_STORAGE_ERROR_SUCCESS) {
        target = defaultValue;
        Solar::Logger::Warn("Could not read setting %s; using default", key);
    }
}

} // namespace

INITIALIZE_PLUGIN() {
    // Create Solar's SD structure before any file-backed log entry can occur.
    // OSReport still captures directory-creation errors even if the SD path is unavailable.
    const bool baseDirectoriesReady = Solar::Paths::EnsureBaseDirectories();

    WUPSConfigAPIOptionsV1 options = {.name = "Astra Launcher"};
    if (WUPSConfigAPI_Init(options, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        Solar::Logger::Error("Failed to initialize WUPS config API");
    }

    LoadBoolSetting(EnabledConfigId, DefaultEnabled, gEnabled);
    LoadBoolSetting(FileModsConfigId, DefaultFileModsEnabled, gFileModsEnabled);
    LoadBoolSetting(PatchModsConfigId, DefaultPatchModsEnabled, gPatchModsEnabled);
    LoadBoolSetting(LegacySDCafiineConfigId, DefaultLegacySDCafiineEnabled, gLegacySDCafiineEnabled);
    LoadBoolSetting(PreLaunchMenuConfigId, DefaultPreLaunchMenuEnabled, gPreLaunchMenuEnabled);
    WUPSStorageAPI_SaveStorage(false);

    Solar::RedirectEngine::Initialize();
    Solar::PatchEngine::Initialize();

    if (baseDirectoriesReady) {
        Solar::Logger::Info("Astra Launcher v0.6 initialized");
    } else {
        // Keep an explicit diagnostic in OSReport; file logging may be unavailable.
        OSReport("[Solar][WARN] Solar Launcher v0.5 initialized without writable SD log directories\n");
    }
}

DEINITIALIZE_PLUGIN() {
    // Native hooks must be removed before adapter registrations are discarded.
    Solar::PatchEngine::Shutdown();
    Solar::RedirectEngine::Shutdown();
    Solar::GameAdapterRegistry::Reset();
    ResetMenuSessionState();
    Solar::Logger::Info("Astra Launcher v0.6 deinitialized");
}

ON_APPLICATION_START() {
    const uint64_t titleId = Solar::TitleManager::CurrentTitleId();
    const std::string titleIdText = Solar::TitleManager::FormatTitleId(titleId);
    const uint32_t upid = OSGetUPID();
    const bool sameMenuSession = IsSameMenuSession(titleId);

    // Runtime state belongs to the current application process and must be rebuilt
    // after a Cuphead scene/process reload. The menu session intentionally survives.
    Solar::PatchEngine::Clear();
    Solar::RedirectEngine::Clear();
    Solar::GameAdapterRegistry::Reset();

    if (!Solar::TitleManager::IsGameTitle(titleId)) {
        if (gMenuSessionActive) {
            Solar::Logger::Info("Left game title %s; resetting Solar pre-launch session",
                                Solar::TitleManager::FormatTitleId(gMenuSessionTitleId).c_str());
            ResetMenuSessionState();
        }
        Solar::Logger::Info("Ignoring non-game title %s", titleIdText.c_str());
        return;
    }

    if (!gEnabled) {
        Solar::Logger::Info("Solar disabled; title %s will start normally", titleIdText.c_str());
        return;
    }

    if (!Solar::Paths::EnsureBaseDirectories()) {
        Solar::Logger::Error("Could not initialize Solar SD directories");
        return;
    }

    if (!Solar::Paths::EnsureTitleDirectory(titleId)) {
        Solar::Logger::Error("Could not initialize mod directory for title %s", titleIdText.c_str());
        return;
    }

    Solar::GameAdapterRegistry::PrepareForTitle(titleId);

    auto mods = Solar::ModManager::ScanForTitle(titleId, gLegacySDCafiineEnabled);
    Solar::SelectionStore::Load(titleId, mods);

    Solar::Logger::Info("Detected title %s with %u compatible mod(s)",
                        titleIdText.c_str(), static_cast<unsigned int>(mods.size()));

    if (!sameMenuSession) {
        gMenuSessionActive = true;
        gMenuSessionTitleId = titleId;
        gLastSessionUpid = upid;
        gSessionVanilla = false;
    } else {
        Solar::Logger::Info("Runtime reload for title %s (UPID %u -> %u); skipping pre-launch menu",
                            titleIdText.c_str(), static_cast<unsigned int>(gLastSessionUpid),
                            static_cast<unsigned int>(upid));
        gLastSessionUpid = upid;
    }

    if (gSessionVanilla) {
        Solar::Logger::Info("Preserving vanilla launch across runtime reload for title %s", titleIdText.c_str());
        return;
    }

    if (!sameMenuSession && gPreLaunchMenuEnabled && !mods.empty()) {
        const Solar::MenuResult menu = Solar::ModMenu::Show(titleId, mods);
        if (menu.action == Solar::MenuAction::LaunchVanilla) {
            gSessionVanilla = true;
            Solar::Logger::Info("User selected vanilla launch for this title session");
            return;
        }

        if (menu.action == Solar::MenuAction::Failed) {
            Solar::Logger::Warn("Pre-launch menu failed; continuing with saved/default selections");
        }
    }

    if (gFileModsEnabled) {
        const Solar::ConflictReport conflicts = Solar::ConflictDetector::Analyze(mods);
        if (conflicts.conflictingPaths > 0) {
            Solar::Logger::Warn("Launching with %u conflicting replacement path(s)%s",
                                static_cast<unsigned int>(conflicts.conflictingPaths),
                                conflicts.truncated ? " (scan truncated)" : "");
        }

        if (Solar::RedirectEngine::IsAvailable()) {
            Solar::RedirectEngine::Apply(mods);
        } else {
            Solar::Logger::Warn("File replacement engine unavailable; continuing without file redirection");
        }
    } else {
        Solar::Logger::Info("File replacement mods are disabled");
    }

    if (gPatchModsEnabled) {
        const Solar::PatchApplyReport patchReport = Solar::PatchEngine::Apply(titleId, mods);
        Solar::Logger::Info("Patch summary: memory %u/%u applied, hooks %u/%u registered",
                            static_cast<unsigned int>(patchReport.memoryPatchesApplied),
                            static_cast<unsigned int>(patchReport.memoryPatchesFound),
                            static_cast<unsigned int>(patchReport.hooksApplied),
                            static_cast<unsigned int>(patchReport.hookRequestsFound));
    } else {
        Solar::Logger::Info("Memory/native patch mods are disabled");
    }
}

ON_APPLICATION_ENDS() {
    // Application-end can occur during Cuphead's internal reload path. Clean only
    // process-bound runtime state here; keep the menu session until another title starts.
    Solar::PatchEngine::Clear();
    Solar::RedirectEngine::Clear();
    Solar::GameAdapterRegistry::Reset();
    Solar::Logger::Info("Application-end callback received; preserving Solar pre-launch session");
}
