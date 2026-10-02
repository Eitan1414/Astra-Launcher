# Astra Launcher — Source & Provenance Record

This document records the external projects, libraries, tools, reference material and development inputs that have been used, linked, consulted, or materially relied on while developing **Astra Launcher** (formerly Solar Launcher).

Its purpose is transparency. It does **not** imply that Astra contains copied source code from every project listed here. Each entry states how that source relates to Astra.

> If a dependency, reference, code fragment, mod, binary, document, tutorial or tool materially influences Astra, it should be added here as soon as it is introduced.

---

## 1. Project authorship and AI assistance

### Astra Launcher

- Project: **Astra Launcher**
- Previous name: **Solar Launcher**
- Repository: https://github.com/Eitan1414/Solar-Launcher
- Project direction, design decisions, testing and maintenance: **Eitan1414 / Pixel Plugin Studios**
- Status: original community homebrew project, built on top of the Wii U homebrew ecosystem listed below.

### OpenAI / ChatGPT

- Source/tool: **OpenAI ChatGPT**
- Use in Astra: development assistance, architecture discussion, code drafting, code review, debugging, documentation, research planning and reverse-engineering assistance.
- Relationship: AI development assistance; OpenAI does not own or maintain Astra and is not an Astra dependency.
- Disclosure: AI assistance is intentionally disclosed publicly in the repository README.
- Important provenance note: an AI assistant cannot provide a reliable per-line list of training-data sources. Only sources that were actually consulted, linked, supplied, inspected or otherwise identifiable during development are recorded in this file.

---

## 2. Wii U build environment and direct dependencies

These projects are part of Astra's actual Wii U build/runtime environment or are directly linked by the project.

### Aroma

- Project: **Aroma**
- Upstream: https://github.com/wiiu-env/Aroma
- Use in Astra: runtime environment in which Astra is intended to run.
- Relationship: platform/runtime dependency.
- Code copied into Astra: none identified in the repository audit; Astra targets Aroma APIs through the Wii U homebrew toolchain.

### Wii U Plugin System (WUPS)

- Project: **WiiUPluginSystem**
- Upstream: https://github.com/wiiu-env/WiiUPluginSystem
- License reported by GitHub: **LGPL-3.0**
- Use in Astra: plugin lifecycle, configuration API, storage API and WUPS plugin build/runtime support.
- Evidence in Astra: WUPS headers/macros are used by `src/main.cpp`; the Makefile links `-lwups`.
- Relationship: direct build/runtime dependency.

### wut

- Project: **wut**
- Upstream: https://github.com/devkitPro/wut
- License reported by GitHub: **zlib License**
- Use in Astra: Wii U SDK/toolchain headers, system APIs and development environment.
- Evidence in Astra: Makefile uses the wut include/library paths and `-lwut`.
- Relationship: direct build dependency.

### devkitPro / devkitPPC

- Project ecosystem: **devkitPro / devkitPPC**
- Website: https://devkitpro.org/
- Package infrastructure: https://github.com/devkitPro/pacman-packages
- Wii U port packages: https://github.com/devkitPro/wut-packages
- Use in Astra: PowerPC cross-compilation toolchain and Wii U development packages.
- Evidence in Astra: Makefile requires `DEVKITPRO`; Docker build is based on the Wii U devkitPPC environment.
- Relationship: direct build-tool dependency.
- Note: the exact devkitPPC implementation repository is not linked here because Astra currently consumes it through the devkitPro/Wii U toolchain packages and build images rather than vendoring its source.

### libmappedmemory / WUMS mapped memory support

- Project: **libmappedmemory**
- Upstream: https://github.com/wiiu-env/libmappedmemory
- License reported by GitHub: **LGPL-3.0**
- Use in Astra: mapped-memory support required by the current plugin/toolchain architecture.
- Evidence in Astra: Docker image imports libmappedmemory artifacts; Makefile links `-lmappedmemory` and uses the WUMS mapped-memory linker script.
- Relationship: direct build/runtime dependency.

### ContentRedirectionModule / libcontentredirection

- Project: **ContentRedirectionModule**
- Upstream: https://github.com/wiiu-env/ContentRedirectionModule
- License reported by GitHub: **GPL-3.0**
- Use in Astra: file/content redirection layers used by `RedirectEngine`.
- Evidence in Astra: `ContentRedirection_InitLibrary`, `ContentRedirection_AddFSLayer`, `ContentRedirection_RemoveFSLayer`; Makefile links `-lcontentredirection`.
- Relationship: direct runtime dependency/API.
- Importance: this is one of the main technical foundations of Astra's SDCafiine-style replacement system.

### FunctionPatcherModule / libfunctionpatcher

- Project: **FunctionPatcherModule**
- Upstream: https://github.com/wiiu-env/FunctionPatcherModule
- License reported by GitHub: **GPL-3.0**
- Use in Astra: native function hooks and patch-engine support.
- Evidence in Astra: Makefile links `-lfunctionpatcher`; PatchEngine/NativeHookRegistry build on FunctionPatcher functionality.
- Relationship: direct runtime dependency/API.

### zlib

- Project: **zlib**
- Canonical source repository: https://github.com/madler/zlib
- License: zlib License.
- Use in Astra: compression/decompression support; linked as `-lz`.
- Use in SOL format: SOL v1 draft compresses individual package files with zlib before encryption.
- Relationship: direct build/runtime dependency.

### mbed TLS

- Project: **Mbed TLS**
- Upstream: https://github.com/Mbed-TLS/mbedtls
- Wii U package source: https://github.com/devkitPro/wut-packages
- License: Apache-2.0 for the 2.28.x package line used by the Wii U package definition consulted during v0.6 work.
- Use in Astra: ChaCha20-Poly1305 authenticated decryption for encrypted `.sol` metadata/files.
- Evidence in Astra v0.6 branch: `src/solar/SolCrypto.cpp` includes `mbedtls/chachapoly.h`; Makefile links `-lmbedcrypto`; Dockerfile installs `wiiu-mbedtls`.
- Relationship: direct v0.6 build/runtime dependency.
- Version note: Astra's Dockerfile currently installs the devkitPro package without pinning a specific package version.

---

## 3. PC-side Astra Packager dependencies

### Python

- Project/runtime: **Python 3**
- Website: https://www.python.org/
- Use in Astra: current prototype implementation of `tools/astra_packager.py`.
- Relationship: development/packaging tool runtime only; not required by the Wii U plugin at runtime.

### pyca/cryptography

- Project: **cryptography**
- Upstream: https://github.com/pyca/cryptography
- Website/docs: https://cryptography.io/
- Use in Astra: PC-side ChaCha20-Poly1305 encryption in the draft Astra Packager.
- Evidence: `tools/astra_packager.py` imports `cryptography.hazmat.primitives.ciphers.aead.ChaCha20Poly1305`.
- Relationship: direct dependency of the current PC packager prototype.
- Licensing: the project is distributed under permissive licensing; consult the upstream repository for the exact license terms of the version installed.

---

## 4. CI / repository automation

### actions/checkout

- Project: **actions/checkout**
- Upstream: https://github.com/actions/checkout
- License reported by GitHub: **MIT**
- Use in Astra: GitHub Actions repository checkout.
- Relationship: CI dependency only.

### actions/upload-artifact

- Project: **actions/upload-artifact**
- Upstream: https://github.com/actions/upload-artifact
- License reported by GitHub: **MIT**
- Use in Astra: upload of compiled Wii U `.wps` development artifacts.
- Relationship: CI dependency only.

### GitHub Actions / GitHub Container Registry

- Service: **GitHub Actions / GHCR**
- Use in Astra: automated builds and Wii U toolchain container images.
- Current build images include Wii U environment artifacts from `ghcr.io/wiiu-env`.
- Relationship: build infrastructure.

---

## 5. Cafiine / SDCafiine lineage and design references

### SDCafiine

- Project: **SDCafiine**
- Upstream mirror consulted/referenced: https://github.com/Maschell/SDCafiine
- License reported by GitHub: **GPL-3.0**
- Use in Astra: architectural inspiration and backward-compatibility target for file-replacement mod packs.
- Relationship: design/reference source; Astra aims to support SDCafiine-style directory layouts and behavior.
- Code copied into Astra: none identified in the current repository audit.
- Important distinction: Astra's implementation currently uses ContentRedirectionModule APIs rather than embedding the SDCafiine source tree.

### Cafiine

- Project/family: **Cafiine**
- Use in Astra: historical design reference for Wii U content/file redirection and the ecosystem that preceded SDCafiine.
- Relationship: historical/architectural reference.
- Provenance note: no specific original Cafiine source repository has yet been verified and recorded during this audit. Add the exact upstream URL here before making a claim that Astra code derives from a particular Cafiine implementation.

### FTPiiU Everywhere / ftpiiu_plugin

- Current Aroma-era project: **ftpiiu_plugin**
- Upstream: https://github.com/wiiu-env/ftpiiu_plugin
- License reported by GitHub: **GPL-3.0**
- Use in Astra: community/homebrew reference acknowledged in the README; useful as an example of established Aroma/WUPS development practices.
- Relationship: ecosystem/reference project.
- Code copied into Astra: none identified in the current repository audit.

---

## 6. Cuphead-specific research and source material

This section is particularly important because Astra's first advanced test project uses proprietary game code/assets and a third-party multiplayer mod as reverse-engineering references.

### Cuphead

- Game: **Cuphead**
- Rights holder/developer: **Studio MDHR** and respective rights holders.
- Use in Astra: target game for the first advanced Astra Game Adapter and 3–4 player experiment.
- Relationship: proprietary target software, not an Astra dependency that can be redistributed.
- Repository policy: Astra should not distribute Cuphead game assets, assemblies or other copyrighted game files.
- Testing/research files supplied by the project maintainer have included legally extracted Wii U managed assemblies and related game files. These are research inputs and are not to be committed to the Astra repository.

### Cuphead Wii U port

- Project/team: **The Latte Team**
- Use in Astra: the Wii U Cuphead port is the target runtime being researched for the Cuphead adapter and multiplayer work.
- Relationship: target port / reverse-engineering reference.
- Current provenance gap: the public upstream repository or canonical project page for The Latte Team's Cuphead Wii U port has **not yet been verified in this audit**.
- Action required: once the canonical upstream/page is confirmed, add the exact URL and any published license/credits here.

### PC Cuphead 4-player demo used as a behavioral/code reference

- Local research archive supplied during development: `cuphead_4p_demo_v075.zip`
- Contents inspected:
  - `READ ME.txt`
  - `Cuphead_Data/Managed/Assembly-CSharp.dll`
  - `Cuphead_Data/resources.assets`
- Use in Astra/Cuphead work: comparison against the Wii U `Assembly-CSharp.dll` to identify changes required for Player 3 / Player 4, PlayerManager, Level spawning, HUD behavior and related multiplayer logic.
- Relationship: third-party mod/reverse-engineering reference.
- Important provenance gap: the archive's included README does **not** state the author's name, original download URL or license.
- Repository policy: do not redistribute this archive, its modified game assembly or its assets through the Astra repository.
- Action required: identify the original author/project page and verify redistribution/modification permissions before publishing any material directly derived from this archive.
- Note: a public 4-player Cuphead mod exists in the wider modding community, but this document intentionally does not attribute `cuphead_4p_demo_v075.zip` to a specific author until its exact origin is verified.

### Rewired

- Product: **Rewired** (Unity input system)
- Website: https://guavaman.com/projects/rewired/
- Use in research: types/methods from Rewired appear in the PC Cuphead multiplayer mod and original Cuphead managed code and were examined while comparing input architecture.
- Relationship: third-party commercial library present in the target game's PC-side managed code; not an Astra dependency.
- Code copied into Astra: none identified.

### Unity / Mono runtime

- Unity: https://unity.com/
- Mono project: https://github.com/mono/mono
- Use in Astra: Cuphead Wii U research includes Unity/Mono runtime behavior, Mono metadata/runtime symbols and `Unity-master.rpx` analysis.
- Relationship: target runtime/API reference, not a library linked into Astra.
- Evidence in Astra: `MonoBridge.cpp`, Cuphead adapter/runtime hook work and Mono-related diagnostics.
- Note: Mono's upstream repository is a useful API/reference source, but the exact Mono build embedded in the Cuphead Wii U port may differ from current upstream Mono.

### dnSpyEx / dnSpy

- Project: **dnSpyEx**
- Upstream: https://github.com/dnSpyEx/dnSpy
- License reported by GitHub: **GPL-3.0**
- Use in Astra/Cuphead research: inspection and manual editing/recompilation of managed `.NET/Mono` assemblies such as Cuphead's `Assembly-CSharp.dll`.
- Relationship: reverse-engineering/development tool; not linked into Astra.

### Mono.Cecil

- Project: **Mono.Cecil**
- Upstream: https://github.com/jbevain/cecil
- License: **MIT**
- Use in the Cuphead P3 prototype: managed assembly inspection and controlled rewriting by the repository's `tools/CupheadP3Patcher` utility.
- Relationship: direct dependency of the PC-side development patcher; it is not linked into the Wii U Astra plugin.
- Purpose: make the Wii U `Assembly-CSharp.dll` modifications reproducible without committing or redistributing the proprietary game assembly.

---

## 7. APIs and technical documentation used during v0.6 SOL work

### Mbed TLS ChaCha20-Poly1305 API

- Upstream header/API consulted: `mbedtls/chachapoly.h`
- Upstream project: https://github.com/Mbed-TLS/mbedtls
- Specific API used: `mbedtls_chachapoly_auth_decrypt`, context initialization/set-key/free functions.
- Use: authenticated SOL index/file decryption on Wii U.

### devkitPro Wii U mbedTLS package definition

- Repository: https://github.com/devkitPro/wut-packages
- Package consulted: `wiiu-mbedtls`
- Use: verified that mbedTLS is available as a Wii U portlib and can be installed in the Astra build container.
- Relationship: package/build reference.

### ContentRedirection API

- Upstream: https://github.com/wiiu-env/ContentRedirectionModule
- APIs used include:
  - `ContentRedirection_InitLibrary`
  - `ContentRedirection_AddFSLayer`
  - `ContentRedirection_RemoveFSLayer`
  - `ContentRedirection_DeInitLibrary`
- Use: current directory-backed replacement layers; future SOL package-backed redirection work will build on or integrate with this subsystem.

### FunctionPatcher API

- Upstream: https://github.com/wiiu-env/FunctionPatcherModule
- Use: native hook registration/patching infrastructure in Astra's patch engine and game-adapter work.

---

## 8. Material that is NOT redistributed by Astra

The following have been used as development/research inputs but should not be committed or redistributed as part of Astra unless their licenses explicitly permit it:

- Cuphead Wii U game binaries and assets.
- `Assembly-CSharp-WiiU.dll`.
- `Cuphead_WiiU_Managed.zip`.
- `cuphead_4p_demo_v075.zip`.
- The modified PC `Assembly-CSharp.dll` and `resources.assets` contained in that 4-player demo.
- Any Studio MDHR Cuphead art, audio, sprites or other proprietary assets.
- Any proprietary Rewired binaries/code from Cuphead.

Astra should distribute its own loader/framework code, metadata, patch descriptions, adapters where legally appropriate, and tooling — not copyrighted game data.

---

## 9. Attribution policy for future contributions

When adding code or behavior based on an external project:

1. Record the source project and exact URL in this file.
2. Record the upstream author/organization where known.
3. Record the license and verify that Astra's use is permitted.
4. State whether the source was:
   - linked as a dependency,
   - called through an API,
   - adapted/ported,
   - studied as a reference,
   - or copied/modified.
5. If code was copied or adapted, identify the relevant upstream file/function and retain any notices required by the license.
6. Do not claim that “the AI supplied the source” unless a real, verifiable source was actually consulted.
7. Do not remove upstream attribution merely because AI assisted with the rewrite or port.

---

## 10. Current provenance limitations

This document is based on:

- the current Astra/Solar repository and its build files,
- development materials available during the Astra v0.6 work,
- the inspected Cuphead research archives,
- and external upstream repositories that were actually consulted or verified.

Two important items are still unresolved and should be completed when the information is available:

1. the canonical public source/project page for **The Latte Team's Cuphead Wii U port**;
2. the original author, download page and license for **`cuphead_4p_demo_v075.zip`**.

Until those are verified, this file deliberately marks them as unknown instead of inventing attribution.

---

## 11. Summary

Astra is an original project built on top of established Wii U homebrew APIs and toolchains. Its core external technical foundations currently include **Aroma, WUPS, wut, devkitPro/devkitPPC, libmappedmemory, ContentRedirectionModule, FunctionPatcherModule, zlib and mbed TLS**. The current PC SOL packager additionally uses **Python and pyca/cryptography**.

Cuphead multiplayer development also relies on reverse-engineering/reference material from the **Cuphead Wii U port**, a supplied **PC 4-player Cuphead demo**, **Unity/Mono**, **Rewired** and **dnSpyEx**. Proprietary game files and third-party mod assets are research inputs and are not intended for redistribution in the Astra repository.
