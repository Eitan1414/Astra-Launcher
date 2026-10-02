<p align="center">
  <img src="Assets/Astra-logo.png" width="500" alt="Astra Launcher">
</p>

<h1 align="center">🌙 Astra Launcher</h1>

<p align="center">
  <b>Universal Wii U modding framework for Aroma</b>
</p>

<p align="center">
  Load. Combine. Expand.
</p>

---

## About

**Astra Launcher** is an experimental universal modding framework for the **Wii U**, designed to run under **Aroma**.

The goal is to go beyond traditional file replacement and provide one unified system for loading different kinds of mods.

Astra Launcher is designed to detect the game being launched through its **Title ID**, find compatible mods on the SD card, and allow the user to choose which ones should be enabled.

> ⚠️ Astra Launcher is currently in early development. Most features described below are planned and may not be implemented yet.

For a detailed record of external projects, libraries, tools, reverse-engineering references and AI assistance used during development, see **[PROVENANCE.md](PROVENANCE.md)**.

### Current development: v0.6 — SOL Package Engine

Active development branch:

~~~text
astra-v0.6-sol-packages
~~~

The v0.6 work introduces Astra's native encrypted mod container, `.sol`.

The first implementation milestones are:

- detect and validate SOL packages by magic, format version and Title ID
- add the PC-side **Astra Packager**
- encrypt package metadata and files with authenticated encryption
- decrypt package metadata on Wii U with ChaCha20-Poly1305 / mbedTLS
- expose packaged mods in the Astra pre-launch menu
- serve packaged files to games without permanently replacing the installed originals
- use the Cuphead 3–4 player project as the first advanced real-world SOL package test

The initial SOL v1 draft uses independent compressed/encrypted file records so Astra can eventually decrypt only the resource requested by the game instead of unpacking the entire package.

---

## 🌙 Goals

Astra Launcher aims to support:

- 🎨 Texture packs
- 📁 File replacement
- 🎵 Custom music and sounds
- ⚙️ Gameplay patches
- 🧠 Memory/function patches
- 🧩 Multiple mods at the same time
- ⚠️ Mod conflict detection
- 📦 SDCafiine-style mod packs
- 🗺️ Custom levels and maps
- 👤 Custom characters
- 🎮 Game-specific mod APIs
- ➕ Advanced addons that can add new content instead of only replacing existing content

---

## 🎮 How it should work

When a compatible Wii U title starts:

```text
Wii U Menu
     ↓
Game launched
     ↓
🌙 Astra Launcher
     ↓
Title ID detected
     ↓
Compatible mods found
     ↓
Select enabled mods
     ↓
Apply file replacements / patches / addons
     ↓
Start the game
```

If no compatible mod is installed, the game should simply launch normally.

---

## 🧩 Mod Types

### 📁 File Replacement

The simplest type of Astra mod.

Used for things such as:

- textures
- sprites
- music
- sound effects
- UI
- other game files

The goal is to provide functionality similar to **SDCafiine** while integrating it into the Astra mod manager.

Example:

```text
Original game file:

/vol/content/player/texture.dds

        ↓

Astra replacement:

SD:/wiiu/SolarLauncher/games/TITLE_ID/MyMod/content/player/texture.dds
```

---

### ⚙️ Gameplay Patches

Astra will eventually be able to apply modifications to the running game.

Examples:

- changing gameplay values
- changing player limits
- modifying mechanics
- hooking game functions
- changing game behavior
- memory patches
- function replacement

For example:

```text
Original game:

Maximum Players = 2

        ↓

Astra gameplay patch

        ↓

Maximum Players = 4
```

---

### ➕ Addons

The long-term goal of Astra is to support **real additional content**.

Instead of only replacing:

```text
Original Level
      ↓
Modified Level
```

a Astra addon could potentially allow:

```text
Original Levels
      +
New Fan-Made Level
      +
New Boss
      +
New Character
```

Advanced addon support will require **game-specific Astra APIs/adapters**, because every game handles levels, characters, saves and other content differently.

---

## 📂 Planned SD Structure

```text
SD:/wiiu/SolarLauncher/
├── games/
│   └── TITLE_ID/
│       ├── ModName/
│       │   ├── mod.json
│       │   ├── content/
│       │   ├── patches/
│       │   └── addons/
│       │
│       └── AnotherMod/
│           ├── mod.json
│           ├── content/
│           ├── patches/
│           └── addons/
│
├── config/
├── cache/
└── logs/
```

A basic Astra mod could contain:

```text
MyMod/
├── mod.json
├── content/
├── patches/
└── addons/
```

Example `mod.json`:

```json
{
  "name": "Example Mod",
  "author": "Example Author",
  "version": "1.0.0",
  "titleId": "00050000XXXXXXXX",
  "type": "replacement"
}
```

---

## ☕ SDCafiine Compatibility

One of Astra Launcher's goals is to support existing **SDCafiine-style file replacement packs** whenever possible.

This would allow users to keep using existing Wii U texture and file packs while benefiting from Astra's mod management system.

Astra Launcher is not intended to simply replace SDCafiine, but to build upon the same general idea and extend it toward more advanced types of modding.

Astra could support both structures:

### Astra native mods

```text
SD:/wiiu/SolarLauncher/
└── games/
    └── TITLE_ID/
        └── MyMod/
            ├── mod.json
            └── content/
```

### Existing SDCafiine packs

```text
SD:/wiiu/sdcafiine/
└── TITLE_ID/
    └── MyTexturePack/
        └── content/
```

Astra could detect both automatically.

---

## ⚠️ Mod Conflicts

Astra is planned to detect when multiple mods try to replace the same file.

For example:

```text
HD Texture Pack
└── player/
    └── character.texture

Custom Character
└── player/
    └── character.texture
```

Astra could warn the user:

```text
⚠ MOD CONFLICT DETECTED

2 mods modify:

player/character.texture

Priority:

1. Custom Character
2. HD Texture Pack
```

The mod with the highest priority would be loaded.

This would make it possible to combine multiple mods while reducing unexpected conflicts.

---

## 🪐 Mod Layer System

Astra could treat enabled mods as layers.

For example:

```text
Original Game
     ↓
HD Texture Pack
     ↓
Custom Music Pack
     ↓
Gameplay Mod
     ↓
Custom Character Mod
     ↓
Game starts
```

When multiple mods modify the same resource, Astra would follow the configured priority order.

---

## 🎯 First Advanced Test Project

One of the first advanced projects planned for Astra Launcher is a **3–4 player mod for the Wii U port of Cuphead**.

This project will help test several Astra systems at once:

- additional players
- additional controllers
- gameplay patches
- custom player sprites
- HUD modifications
- file replacement
- game-specific patches

The goal is to expand Cuphead's existing local multiplayer support beyond two players.

Concept:

```text
Player 1 → Cuphead
Player 2 → Mugman
Player 3 → Custom Mugman Variant
Player 4 → Custom Mugman Variant
```

Players 3 and 4 are planned to support custom visual variants based on existing characters.

This project could later serve as an early experiment for a future **Astra Cuphead API** capable of supporting more advanced fan-made content.

Examples could eventually include:

- fan-made levels
- new bosses
- new Run 'n Gun stages
- new playable characters
- custom islands
- new weapons
- new charms
- additional music
- additional visual content

---

## 🌍 Astra Game APIs

Some types of content cannot be loaded universally because every game handles its internal systems differently.

Astra therefore plans to support optional **game-specific APIs/adapters**.

For example:

```text
🌙 Astra Launcher
│
├── Astra Cuphead API
├── Astra Mario Kart 8 API
├── Astra Minecraft API
└── Other Game Adapters
```

These adapters could expose systems that mod creators can use without having to manually reverse-engineer every part of a game.

Conceptually, a game API could support actions such as:

```text
RegisterLevel()
RegisterCharacter()
RegisterMap()
RegisterBoss()
RegisterMusic()
RegisterWeapon()
RegisterItem()
```

The exact available functionality would depend on each supported game.

---

## 🗺️ Example: Cuphead Fan-Made Island

A future Cuphead addon could theoretically look like:

```text
FanmadeIsland/
├── addon.json
├── island.json
│
├── levels/
│   ├── boss01/
│   ├── boss02/
│   └── runngun01/
│
├── map/
│   └── island5/
│
├── sprites/
├── music/
└── sounds/
```

For example, `island.json` could describe the additional content:

```json
{
  "name": "Inkwell Island 5",
  "levels": [
    {
      "name": "Clockwork Chaos",
      "type": "boss",
      "path": "levels/boss01"
    },
    {
      "name": "Toon Town Trouble",
      "type": "run_and_gun",
      "path": "levels/runngun01"
    }
  ]
}
```

Astra would detect the addon and use the **Astra Cuphead API** to integrate the additional content into the game.

---

## 🛠️ Development

Astra Launcher is planned around the Wii U **Aroma** environment and the **Wii U Plugin System (WUPS)**.

The project is currently experimental and under active development.

---

## 🗓️ Development Roadmap

### v0.1 — Astra Core

- Title ID detection
- SD mod scanning
- `mod.json` support
- Enable/disable mods
- Basic configuration system
- Basic logging

### v0.2 — File Mods

- File redirection
- Texture/file packs
- Initial SDCafiine compatibility
- Multiple replacement packs
- Basic Astra mod menu

### v0.3 — Mod Management

- Multiple simultaneous mods
- Mod priorities
- Conflict detection
- Better mod metadata
- Dependency support

### v0.4 — Patch Engine

- Memory patches
- Function hooks
- Game/version-specific patches
- Improved debugging and logging

### v0.5 — Advanced Mods

- First advanced gameplay mods
- Initial Cuphead 3-player experiments
- Custom character support

### v0.6 — SOL Package Engine + Cuphead 4 Player

- Native encrypted `.sol` package format
- PC-side Astra Packager
- SOL header / Title ID validation
- Authenticated package index decryption on Wii U
- On-demand packaged file access and redirection
- Four local Cuphead players
- P3/P4 controller support
- Custom P3/P4 sprites
- HUD extensions
- Camera modifications
- Revive support
- Boss targeting modifications

### Future

- Advanced addons
- Game APIs
- Custom levels
- Custom maps
- Custom characters
- New gameplay content
- Addon dependencies
- Mod profiles
- Community-created Game APIs

---

## 🔧 Planned Astra Architecture

Astra Launcher is planned around several main systems:

```text
🌙 Astra Launcher
│
├── Title Manager
│   ├── Detect current game
│   ├── Read Title ID
│   └── Detect game version
│
├── Mod Manager
│   ├── Scan installed mods
│   ├── Read mod.json
│   ├── Enable / disable mods
│   ├── Handle dependencies
│   └── Handle priorities
│
├── Redirect Engine
│   ├── Textures
│   ├── Audio
│   ├── Sprites
│   ├── UI
│   └── Game files
│
├── Patch Engine
│   ├── Memory patches
│   ├── Function hooks
│   ├── Gameplay modifications
│   └── Version-specific patches
│
├── Conflict Manager
│   ├── Detect file conflicts
│   ├── Detect incompatible mods
│   └── Resolve priorities
│
└── Addon Engine
    ├── Game APIs
    ├── Levels
    ├── Maps
    ├── Characters
    ├── Bosses
    └── Additional content
```

---

## 🌙 Astra Launcher Flow

```text
              Wii U Menu
                   │
                   ↓
              Start a Game
                   │
                   ↓
          🌙 Astra Launcher
                   │
                   ↓
           Detect Title ID
                   │
                   ↓
       Search Compatible Mods
                   │
                   ↓
            Astra Mod Menu
                   │
          ┌────────┴────────┐
          │                 │
          ↓                 ↓
     File Mods         Code Patches
          │                 │
          └────────┬────────┘
                   │
                   ↓
              Addons/API
                   │
                   ↓
            Resolve Conflicts
                   │
                   ↓
              Launch Game
```

---

## 🔐 Native .sol Packages

Astra v0.6 is introducing a native single-file mod container:

~~~text
MyMod/
├── mod.json
├── content/
├── patches/
└── addons/
        ↓
Astra Packager
        ↓
MyMod.sol
        ↓
Astra Launcher
        ↓
select mod before game launch
        ↓
decrypt / redirect required resources
        ↓
game starts with the mod enabled
~~~

The installed game files remain untouched. Disabling the mod simply allows the game to load its original resources again.

The format is still experimental and is documented in the v0.6 development branch under `docs/SOL_FORMAT_DRAFT.md`.

---

## 📦 Astra Mod Types

Astra Launcher currently plans four main mod categories:

```text
[1] REPLACEMENT
    └── Textures, audio, sprites and game files

[2] PATCH
    └── Memory and gameplay modifications

[3] ADDON
    └── New levels, maps, characters and content

[4] TOTAL MOD
    └── Combination of replacements, patches and addons
```

Example:

```text
Cuphead 4 Player

Type:
TOTAL MOD

Uses:
├── File Replacement
│   └── P3/P4 sprites and HUD
│
├── Gameplay Patches
│   └── 4-player support
│
└── Game API
    └── Cuphead-specific integration
```

---

## 🤝 Contributions

Astra Launcher is intended to become an open modding framework for the Wii U community.

Contributions are welcome in areas such as:

- code
- testing
- documentation
- game research
- mod development
- bug reports
- UI design
- reverse engineering
- ideas
- feature suggestions
- game adapters
- addon development

Every contribution can help expand what is possible on the Wii U.

---

## ❤️ Credits

### 🌙 Project

**Astra Launcher**

Created and led by **[Eitan1414/Pixel Plugin Studios]**

Concept, project direction, testing, design and original idea by the Astra Launcher project creator.

---

### 🤖 Development Assistance

Special thanks to **OpenAI's GPT-5.6 Sol** for development assistance, technical research, brainstorming, architecture design and support throughout the creation of this project.

The project was originally named **Solar Launcher** in tribute to **GPT-5.6 Sol**, whose help contributed to this project and other Wii U development projects. Its yellow sun logo reflected that name.

As the project continues with **GPT-6 Astra**, it is now called **Astra Launcher**, extending that tribute to this new chapter. The original tribute to Sol remains part of the project's history.

The logo has evolved with the name: a **pale mauve moon** now replaces the yellow sun, while the familiar **white lightning-shaped L** is kept, enlarged and centered within the moon. **Astra** appears in glacier blue beside **Launcher** in white. This new visual identity connects the project's beginnings with its next chapter.

> Astra Launcher is an independent community project and is not officially affiliated with or endorsed by OpenAI.
(yep I'm using IA and what I need some moral and development help anyway I know that all of you already do a lot of crazy stuff with IA so don't blame me)

---

### 🛠️ Wii U Homebrew Community

Astra Launcher builds upon years of work from the Wii U homebrew community.

Special thanks to the developers and contributors behind projects and tools such as:

- **Aroma**
- **Wii U Plugin System (WUPS)**
- **wut**
- **devkitPro**
- **devkitPPC**
- **ContentRedirectionModule**
- **FunctionPatcherModule**
- **SDCafiine**
- **FTPiiU Everywhere**

Their work makes projects like Astra Launcher possible and continues to expand what the Wii U can do.

---

### 🎮 Cuphead Wii U

Special thanks to **The Latte Team** for their work on the Wii U port of **Cuphead**, which is planned to serve as one of Astra Launcher's first advanced modding test cases.

The Cuphead 3–4 player project is intended as a community modification and is separate from the original Wii U port.

**Cuphead**, its characters, artwork and related intellectual property belong to **Studio MDHR** and their respective rights holders.

---

### 💙 Community

Thanks to everyone who contributes:

- code
- documentation
- testing
- mods
- game research
- bug reports
- suggestions
- tools
- tutorials

Astra Launcher is intended to grow together with the Wii U modding and homebrew community.

---

## ⚠️ Disclaimer

Astra Launcher is an unofficial homebrew project.

It is not affiliated with or endorsed by:

- Nintendo
- OpenAI
- Studio MDHR
- The Latte Team
- any game publisher or developer unless explicitly stated otherwise

Users should provide their own legally obtained games and game files.

Astra Launcher does not aim to distribute copyrighted game assets.

Game names and trademarks belong to their respective owners.

---

<p align="center">
  🌙 <b>Astra Launcher</b><br>
  <i>Universal Wii U modding framework</i>
</p>

<p align="center">
  <b>Built for the Wii U modding community.</b>
</p>
