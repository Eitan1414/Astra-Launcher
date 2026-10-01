# Astra .sol format — draft v1

> Development format for Astra Launcher v0.6. This is not yet a frozen public ABI.

## Goal

A .sol file packages one Astra mod into a single encrypted container while leaving the installed game untouched.

Typical source folder:

~~~text
MyMod/
├── mod.json
├── content/
├── aoc/
├── patches/
└── addons/
~~~

Output:

~~~text
MyMod.sol
~~~

## Container layout

~~~text
+---------------------------+
| Header                    |
+---------------------------+
| Encrypted index/manifest  |
+---------------------------+
| Encrypted file #0         |
+---------------------------+
| Encrypted file #1         |
+---------------------------+
| ...                       |
+---------------------------+
~~~

### Fixed header

All integer fields are big-endian.

~~~text
magic[8]      = "ASTRASOL"
version u16   = 1
flags u16
titleId u64
indexNonce[12]
indexSize u32
~~~

Current v1 flags:

~~~text
bit 0 = encrypted
bit 1 = compressed
~~~

The fixed header is 36 bytes.

## Encrypted index

The packager serializes a compact JSON index and encrypts it with ChaCha20-Poly1305.

The index contains:

- the original mod.json metadata
- virtual file paths
- payload-relative offsets
- encrypted, compressed and original sizes
- one unique nonce per file
- compression/encryption identifiers

The index AAD is:

~~~text
ASTRASOL-INDEX-v1
~~~

## File records

Every file is handled independently:

1. read the original file
2. compress with zlib
3. encrypt the compressed bytes with ChaCha20-Poly1305
4. append the encrypted record to the package

Per-file AAD:

~~~text
ASTRASOL-FILE-v1:<virtual path>
~~~

Independent records are intentional: Astra should eventually be able to decrypt only the file the game requests instead of unpacking the entire mod.

## PC packager

Current development tool:

~~~text
tools/astra_packager.py
~~~

Dependency:

~~~bash
pip install cryptography
~~~

Example:

~~~bash
python tools/astra_packager.py MyMod -o MyMod.sol --key-hex <64 hex characters>
~~~

The key may also be provided with:

~~~text
ASTRA_SOL_KEY_HEX
~~~

## Astra v0.6 loader milestones

### Milestone 1 — implemented

- detect *.sol files in the title mod directory
- validate the ASTRASOL magic
- validate format version
- read flags
- read package Title ID
- reject packages for another title
- validate the encrypted-index size before reading it
- expose the package in ModManager as a SOL package

### Milestone 2 — in progress

Implemented:

- authenticated ChaCha20-Poly1305 decryption primitive on Wii U through mbedTLS
- encrypted package-index reading
- authenticated index decryption API

Still to do:

- choose/provision the matching Astra package key without committing production key material
- parse the decrypted embedded manifest
- show real package name/author/version in the Astra menu
- reject tampered packages cleanly at the ModManager/UI boundary

### Milestone 3

- look up a virtual file inside a package
- decrypt and zlib-decompress one record on demand
- connect packaged files to Astra's redirection layer
- avoid writing decrypted game files back to SD

### Milestone 4

- package signing / trust metadata
- dependency/conflict integration
- stable public format versioning

## Important implementation note

The current RedirectEngine uses ContentRedirection_AddFSLayer, which expects a real directory on the SD card.

A final encrypted .sol implementation should not simply unpack the whole package into a temporary SD directory. Astra v0.6 therefore needs either a package-backed virtual filesystem/device layer or a lower-level redirection path capable of serving decrypted bytes on demand.

A temporary development-only extraction path may be useful for debugging, but it should not become the final package architecture.

## Key handling

The draft packager deliberately does not hard-code a production key.

A key embedded in a public Wii U plugin can ultimately be recovered by reverse engineering, so .sol encryption should be treated as protection against casual extraction and direct file browsing, not as unbreakable DRM.
