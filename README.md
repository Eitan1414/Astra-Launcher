<p align="center">
  <img src="Assets/Solar-logo.png" width="500" alt="Solar Launcher">
</p>

<h1 align="center">🌙 Astra Launcher — v0.6 development</h1>

<p align="center">
  <b>Universal Wii U modding framework for Aroma</b><br>
  Load. Combine. Expand.
</p>

---

## What is Astra Launcher?

**Astra Launcher** is an experimental modding framework for the **Wii U** running under **Aroma**. The project was previously named Solar Launcher; internal paths/namespaces are being kept compatible during the transition.

Its goal is to provide one common launcher and runtime for different kinds of Wii U mods instead of requiring a completely separate loader for every project.

Astra currently combines two layers:

1. **Universal mod loading** — file replacement, SDCafiine compatibility, priorities, saved selections, conflict detection and declarative patches.
2. **Game Adapters** — trusted game-specific code for deeper gameplay modifications that cannot be expressed as simple file replacements.

### Astra as a Cafiine / SDCafiine successor

Solar Launcher is designed as a **modern, more feature-rich and more efficient alternative to Cafiine/SDCafiine** for Wii U modding under Aroma.

Instead of limiting the project to file replacement, Solar expands the same basic idea into a complete modding framework with features such as:

- multiple mods enabled at the same time
- mod enable/disable from a pre-launch interface
- configurable priorities
- replacement-file conflict detection
- saved per-game selections
- one-time vanilla launch
- AOC replacement
- declarative memory patches
- FunctionPatcher-based native hooks
- game-specific adapters
- future Texture / Behavior / addon-style systems
- diagnostics and file-backed logs
- future beginner-oriented compatibility tooling

Solar is also intended to reduce unnecessary overhead by keeping these systems inside one integrated Aroma/WUPS framework instead of stacking multiple independent loaders. Formal performance benchmarks against Cafiine/SDCafiine are still planned, so the performance goal should not yet be interpreted as a published benchmark result.

Most importantly, Solar is designed to remain **backward-compatible with existing Cafiine/SDCafiine-style file replacement mods**. Existing packs can be detected from the normal legacy SD structure and used from Solar without forcing creators to rebuild every old mod in a new format.

In other words:

```text
Existing Cafiine / SDCafiine mods
              ↓
        Solar compatibility
              ↓
   old packs keep working
              +
 priorities / conflicts / UI / patches / adapters / new features
```

Solar-native mods can then use additional features that did not exist in the original Cafiine workflow.

> ⚠️ Solar Launcher is still in active development. Do not treat current development builds as a stable release.

---

# 🚧 Current status

Current launcher work:

~~~text
Astra Launcher v0.6 — SOL Package Engine
Branch: astra-v0.6-sol-packages
~~~

Current v0.6 implementation:

- SOL v1 fixed-header reader and validation
- Title ID validation during mod scanning
- .sol package detection in ModManager
- draft PC-side Astra Packager
- SOL v1 format documentation
- next milestone: authenticated index decryption on Wii U

Current Cuphead multiplayer work:

```text
Cuphead Player 3 — Test 2
Branch: cuphead-player3-test2
```

The previous Cuphead Mono verification branch is kept separately:

```text
cuphead-test1b-verification
```

## ✅ Confirmed on real Wii U hardware

The Cuphead **Test 1B** runtime verification was successfully completed on a real Wii U.

Confirmed:

- Solar launches under Aroma
- Cuphead is detected correctly
- the Cuphead Game Adapter is registered
- `Unity-master.rpx` is found at runtime
- `mono_compile_method` is located and hooked successfully
- the runtime relocation delta is resolved correctly
- Mono metadata helpers are resolved and validated
- managed Cuphead methods are traced with their native compiled addresses
- the normal executable-offset FunctionPatcher hook becomes **ACTIVE**
- Solar's pre-launch menu session survives Cuphead boss death/restart/map reload callbacks without reopening

The successful runtime delta observed during Test 1B was:

```text
linked mono_compile_method : 0x02067430
runtime address            : 0x02067450
delta                      : +0x20
```

Example methods successfully observed include:

```text
Level::OnPlayerJoined
LevelHUD::OnPlayerJoined
Level::OnPlayerDeath
Level::OnPlayerRevive
AbstractPlayerController::OnPreRevive
AbstractPlayerController::OnRevive
LevelHUDPlayer::OnHealthChanged
LevelHUDPlayer::OnWeaponChanged
LevelHUDPlayer::OnSuperChanged
```

This means the Mono bridge research phase is no longer blocking development of the actual Cuphead multiplayer mod.

---

# ✅ Implemented Solar features

## Core

- WUPS/Aroma plugin
- current Title ID detection
- per-game Solar directories
- `mod.json` parsing
- file-backed logging
- Aroma configuration menu
- per-title mod scanning

Solar data is stored under:

```text
SD:/wiiu/SolarLauncher/
├── games/
├── config/
├── cache/
└── logs/
    └── solar.log
```

## File mods

Solar uses `ContentRedirectionModule` for replacement layers.

Supported generic payloads:

```text
content/   -> /vol/content
aoc/       -> /vol/aoc
```

Features:

- multiple mods at once
- priority ordering
- fallback to original game files
- SDCafiine pack detection
- Solar and SDCafiine packs in the same selector

## Mod management

The pre-launch menu currently supports:

- enable / disable mods
- per-game saved selections
- change priority with L/R
- reset a mod to its defaults
- one-time vanilla launch
- replacement-file conflict detection
- technical details view
- GamePad + Wii controller input
- enabled-mod counter
- page indicator
- launch/save status messages

## Patch Engine

Supported foundations include:

- `patches/` payloads
- declarative JSON memory patches
- address validation
- expected-byte checks
- overlap protection
- original-byte restoration
- `FunctionPatcherModule` integration
- Native Hook Registry
- game-specific trusted hook IDs

## Game Adapter system

Solar includes a reusable adapter layer for deeper game-specific mods.

Current built-in research target:

```text
Cuphead Wii U
```

The generic Mono bridge is separated from Cuphead-specific metadata so other Unity/Mono titles can potentially reuse the same infrastructure later.

---

# ☕ Cuphead Wii U support

Verified target:

```text
Title ID:      0005000021000000
Title version: 0
Executable:    Unity-master.rpx
Runtime:       Unity / Mono
Gameplay DLL:  Assembly-CSharp.dll
```

Important managed systems identified so far include:

```text
PlayerManager
PlayerInput
AbstractPlayerController
PlayerCameraController
Level
LevelHUD
LevelHUDPlayer
CreatePlayerTwoOnJoin
SetupPlayerTwo
RevivePlayer
PlayerSuperGhost
```

The planned third player ID is:

```text
Player 1 = 0
Player 2 = 1
Player 3 = 2
```

> Player 3 is **not functional yet**. Test 2 is the first implementation phase after the successful Test 1B runtime verification.

---

# 🎮 Cuphead Player 3 — Test 2

Test 2 is being developed on:

```text
cuphead-player3-test2
```

The first Test 2 runtime can inspect the managed gameplay image and enumerate relevant Cuphead classes/methods with:

- method names
- parameter types
- return types
- static / instance flags
- native compiled addresses for selected candidates

The purpose is to identify the exact existing Cuphead join/setup path before invoking it for **Player ID 2**.

Planned implementation order:

```text
Player 3 creation / registration
        ↓
input/controller source
        ↓
spawn + OnPlayerJoined
        ↓
HUD
        ↓
camera behavior
        ↓
death / revive / ghost behavior
        ↓
boss targeting and remaining 2-player assumptions
```

A third physical controller is not required for the earliest spawn/HUD development tests; independent P3 input will be validated later.

---

# 🎨 Cuphead Texture Packs & Behavior Packs

The `solar-launcher-v0.5.1-polish` branch now contains the first Cuphead-specific pack support.

For Cuphead Title ID `0005000021000000`, Solar recognizes:

```text
textures/
texture_pack/
behavior/
behavior_pack/
```

### Texture Pack

A `textures/` or `texture_pack/` folder is treated as a `/vol/content` replacement layer.

Example:

```text
SD:/wiiu/SolarLauncher/games/0005000021000000/MyTexturePack/
├── mod.json
└── textures/
    └── <same relative path as the original Cuphead content file>
```

### Behavior Pack

A `behavior/` or `behavior_pack/` folder is also mounted against `/vol/content` and is intended for game-behavior files such as managed assemblies or other Cuphead content-level logic files.

Example:

```text
SD:/wiiu/SolarLauncher/games/0005000021000000/MyBehaviorPack/
├── mod.json
├── behavior/
│   └── <same relative path as the original Cuphead file>
└── patches/
    └── <optional Solar patch definitions>
```

Behavior Packs are **not** a generic arbitrary-C# loader. They currently combine file replacement with Solar's Patch Engine / trusted adapter system when deeper runtime changes are needed.

Texture and Behavior payloads participate in Solar's normal:

- enable/disable system
- priorities
- conflict detection
- saved selections

The launcher reports payloads as:

```text
C / T / B / A / P

C = content
T = texture pack
B = behavior pack
A = AOC
P = patches
```

> ⚠️ Cuphead Texture/Behavior Pack support is implemented in the development branch but still requires dedicated real-console validation with known test files.

More details: [`docs/CUPHEAD_PACKS.md`](docs/CUPHEAD_PACKS.md)

---

# 🖼️ Mod icons

Mod icons are the next launcher UI feature being developed.

Planned convention:

```text
MyMod/
├── mod.json
└── icon.png
```

Solar will display the custom icon for the selected mod when available.

If a mod has no icon, the fallback will be the **Solar sun emblem** from the launcher logo.

> This icon loader is planned/currently being worked on and should not yet be considered implemented until the PNG loading/rendering path is committed and validated.

---

# 🕹️ Current launcher UI

The V0.5.1 polish branch uses a dedicated layout for each display instead of squeezing the TV layout onto the GamePad.

Current visual direction:

- black background
- embedded monochrome Solar binary/ASCII artwork derived from the approved Solar reference
- Solar-orange separators and selection marker
- separate full-width GamePad layout
- 4 mods per GamePad page for more spacing
- selected-mod information section
- technical details toggle
- enabled count, conflicts and page status

Controls:

```text
D-Pad Up/Down   Select mod
A               Enable / disable
X               Normal / technical details
L / R           Change priority
Y               Reset selected mod
+               Save selection and launch
B               Launch vanilla once
```

Current development build label:

```text
SOLAR v0.5.1-polish
```

---

# 📦 Basic Solar mod structure

A normal Solar mod lives under the game's Title ID:

```text
SD:/wiiu/SolarLauncher/games/<TITLE_ID>/<MOD_NAME>/
```

Example:

```text
MyMod/
├── mod.json
├── content/          # optional
├── aoc/              # optional
└── patches/          # optional
```

Example `mod.json`:

```json
{
  "name": "My Solar Mod",
  "author": "Author",
  "version": "1.0",
  "type": "content",
  "titleId": "0005000012345678",
  "enabled": true,
  "priority": 0
}
```

Cuphead mods can additionally use the Texture/Behavior payload folders described above.

---

# 📦 Legacy Cafiine / SDCafiine compatibility

Solar is intentionally **backward-compatible with the legacy Cafiine/SDCafiine file-replacement ecosystem**.

Solar can discover existing SDCafiine packs under:

```text
SD:/wiiu/sdcafiine/<TITLE_ID>/
```

Those packs can be shown alongside Solar-native mods in the same pre-launch selector and can benefit from Solar's management layer without requiring the original mod to be rewritten as a Solar-native package.

For ordinary legacy replacement packs, the intended migration path is therefore:

```text
Old Cafiine / SDCafiine pack
          ↓
Keep existing replacement files
          ↓
Solar detects the legacy pack
          ↓
Use it through Solar
```

Solar-native mods can additionally use features that legacy packs do not provide by themselves, including priorities, conflict handling, memory patches, trusted runtime hooks and Game Adapters.

This backward compatibility is important to the project: **moving to Solar should not mean abandoning the existing Wii U mod library.**

Do not run a separate standalone SDCafiine replacement system in parallel when Solar is already applying the same game's replacement layers, as both systems can compete for content redirection.

---

# 🔐 User-owned game files

Solar does **not** aim to distribute copyrighted game files.

Game-specific research, conversion or compatibility tools should work from files legally supplied/extracted by the user from their own game installation.

The Cuphead adapter development follows this model: Solar contains compatibility/hooking code, while the required Cuphead game files are supplied separately by the user for verification and testing.

---

# 🧪 What still needs testing?

Current important validation work includes:

- V0.5.1-polish GamePad layout on hardware
- mod icon rendering once implemented
- Cuphead `textures/` redirection with an obvious single-file visual test
- Cuphead `behavior/` replacement with a minimal safe behavior test
- combined Texture + Behavior pack loading
- priority/conflict behavior between `content/`, Texture and Behavior layers
- Player 3 creation Test 2
- independent third-controller input later
- long-session stability
- formal Cafiine/SDCafiine vs Solar performance benchmarking

---

# 🧰 Planned Game Compatibility Kit

A future **Solar Game Compatibility Kit** is planned to make adding support for new Wii U games much easier, including for people with little coding experience.

The goal is not to remove all reverse engineering, but to avoid forcing every contributor to build a Game Adapter from an empty C++ file.

Planned beginner-friendly helpers include:

- a ready-to-fill Game Adapter template
- guided Title ID / game version / executable configuration
- example `mod.json`, `content/`, `textures/`, `behavior/` and `patches/` layouts
- automatic generation of starter adapter files
- checks for common RPX/RPL, Unity and Mono information when available
- signature/address verification helpers
- clear logs explaining what succeeded or failed
- example adapters based on already-supported games
- step-by-step documentation for basic file replacement before advanced hooks
- PC-side helper tools for tasks that are impractical to perform directly on the Wii U

A beginner should eventually be able to follow a workflow similar to:

```text
Select / identify game
        ↓
Enter Title ID + version
        ↓
Provide legally extracted executable/files for analysis
        ↓
Solar Compatibility Kit checks the game structure
        ↓
Generate starter Game Adapter
        ↓
Add simple mod payloads
        ↓
Test on Wii U
        ↓
Use advanced hooks only if the mod actually needs them
```

Simple games or file-replacement-only mods should require little code. Deep gameplay modifications may still require C/C++, reverse engineering or game-specific research.

> The Game Compatibility Kit is a **planned future release/tooling project** and is not available yet.

---

# 🗺️ Roadmap

## v0.6 — SOL Package Engine

- finish authenticated SOL index decryption on Wii U
- parse embedded package manifests
- display package metadata in the pre-launch menu
- decrypt/decompress individual package records on demand
- connect SOL records to Astra file redirection without permanent game-file replacement
- use Cuphead 3–4 player as the first advanced SOL package
- continue P3 spawn → input → HUD → camera → revive

## Near term

- finish V0.5.1 launcher polish
- custom `icon.png` support
- Solar sun fallback icon
- validate Cuphead Texture Packs
- validate Cuphead Behavior Packs
- continue Cuphead Player 3 Test 2
- P3 spawn → input → HUD → camera → revive
- benchmark Solar file redirection against legacy Cafiine/SDCafiine workflows

## Later

Possible future Solar targets/features include:

- **Solar Game Compatibility Kit for beginner contributors**
- Minecraft Wii U texture/resource pack conversion experiments
- Minecraft Wii U skin-pack conversion/import
- selected Bedrock/Java asset conversion where technically possible
- richer game-specific adapters
- automatic signature scanning
- PC-side RPX/RPL analyzer
- reusable addon APIs
- additional Unity/Mono game adapters

Large standalone ports such as a hypothetical Minecraft Java runtime are intentionally **not a current priority** while Solar and existing mods are still under active development.

---

# 🛠️ Building

Solar uses the Wii U homebrew toolchain and WUPS.

The repository also contains GitHub Actions workflows for development branches so test `.wps` builds can be produced without manually rebuilding the Wii U toolchain every time.

Development artifacts are intentionally given distinct names where possible to avoid mixing experimental builds, for example:

```text
SolarLauncher-Cuphead-Test1B-AllInOne-wps
SolarLauncher-Cuphead-Player3-Test2-wps
SolarLauncher-v0.5.1-Polish-wps
```

---

# 📄 Logs

Runtime logs are written to:

```text
SD:/wiiu/SolarLauncher/logs/solar.log
```

When reporting a crash, failed hook, missing mod or redirection problem, include this file whenever possible.

---

# ⚠️ Development warning

Solar Launcher currently contains experimental runtime hooks and game-specific research code.

Always keep backups of important SD-card data and game saves when testing development builds.

---

<p align="center">
  <b>☀️ Solar Launcher — Load. Combine. Expand.</b>
</p>
