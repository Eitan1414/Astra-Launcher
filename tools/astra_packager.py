#!/usr/bin/env python3
"""
Astra Packager - SOL format draft v1.

Builds one encrypted .sol package from an Astra mod folder.

Dependency:
    pip install cryptography

Example:
    python tools/astra_packager.py Cuphead4P -o Cuphead4P.sol --key-hex <64 hex chars>
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import zlib
from pathlib import Path
from typing import Any

try:
    from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
except ImportError:
    print("Missing dependency: cryptography")
    print("Install it with: pip install cryptography")
    raise SystemExit(2)

MAGIC = b"ASTRASOL"
FORMAT_VERSION = 1
FLAG_ENCRYPTED = 1 << 0
FLAG_COMPRESSED = 1 << 1

# big-endian: magic, version, flags, titleId, index nonce, encrypted index size
HEADER = struct.Struct(">8sHHQ12sI")


def parse_title_id(value: str) -> int:
    cleaned = value.lower().replace("0x", "").replace("-", "").replace(" ", "")
    if not cleaned:
        raise ValueError("empty title ID")
    return int(cleaned, 16)


def normalize_relpath(path: Path) -> str:
    return path.as_posix().lstrip("/")


def read_manifest(mod_dir: Path) -> dict[str, Any]:
    manifest_path = mod_dir / "mod.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(f"Missing {manifest_path}")

    with manifest_path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    if not isinstance(manifest, dict):
        raise ValueError("mod.json must contain a JSON object")

    for field in ("name", "version", "titleId"):
        if not manifest.get(field):
            raise ValueError(f"mod.json is missing required field: {field}")

    return manifest


def collect_files(mod_dir: Path) -> list[Path]:
    files: list[Path] = []
    for path in mod_dir.rglob("*"):
        if not path.is_file():
            continue
        if path.name == "mod.json" and path.parent == mod_dir:
            continue
        files.append(path)

    return sorted(
        files,
        key=lambda path: normalize_relpath(path.relative_to(mod_dir)).lower(),
    )


def build_package(mod_dir: Path, output: Path, key: bytes) -> None:
    if len(key) != 32:
        raise ValueError("ChaCha20-Poly1305 requires a 32-byte key")

    manifest = read_manifest(mod_dir)
    title_id = parse_title_id(str(manifest["titleId"]))
    cipher = ChaCha20Poly1305(key)

    payload_parts: list[bytes] = []
    file_records: list[dict[str, Any]] = []
    payload_offset = 0

    for source in collect_files(mod_dir):
        relative_path = normalize_relpath(source.relative_to(mod_dir))
        raw = source.read_bytes()
        compressed = zlib.compress(raw, level=9)

        nonce = os.urandom(12)
        aad = ("ASTRASOL-FILE-v1:" + relative_path).encode("utf-8")
        encrypted = cipher.encrypt(nonce, compressed, aad)

        file_records.append(
            {
                "path": relative_path,
                "offset": payload_offset,
                "encryptedSize": len(encrypted),
                "compressedSize": len(compressed),
                "originalSize": len(raw),
                "nonce": nonce.hex(),
                "compression": "zlib",
                "encryption": "chacha20-poly1305",
            }
        )

        payload_parts.append(encrypted)
        payload_offset += len(encrypted)

    index = {
        "format": "ASTRA-SOL",
        "formatVersion": FORMAT_VERSION,
        "manifest": manifest,
        "files": file_records,
    }

    index_plain = json.dumps(
        index,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")

    index_nonce = os.urandom(12)
    index_encrypted = cipher.encrypt(
        index_nonce,
        index_plain,
        b"ASTRASOL-INDEX-v1",
    )

    header = HEADER.pack(
        MAGIC,
        FORMAT_VERSION,
        FLAG_ENCRYPTED | FLAG_COMPRESSED,
        title_id,
        index_nonce,
        len(index_encrypted),
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as handle:
        handle.write(header)
        handle.write(index_encrypted)
        for payload in payload_parts:
            handle.write(payload)

    print(f"Created: {output}")
    print(f"Mod: {manifest.get('name')} {manifest.get('version')}")
    print(f"Title ID: {title_id:016X}")
    print(f"Files: {len(file_records)}")
    print("Format: SOL v1 draft / zlib + ChaCha20-Poly1305")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build an Astra .sol package (draft format v1)"
    )
    parser.add_argument(
        "mod_dir",
        type=Path,
        help="Astra mod folder containing mod.json",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="Output .sol file",
    )
    parser.add_argument(
        "--key-hex",
        help=(
            "32-byte encryption key as 64 hexadecimal characters. "
            "If omitted, ASTRA_SOL_KEY_HEX is used."
        ),
    )
    args = parser.parse_args()

    key_hex = args.key_hex or os.environ.get("ASTRA_SOL_KEY_HEX", "")
    if len(key_hex) != 64:
        print("A 32-byte key is required.")
        print("Use --key-hex <64 hex chars> or ASTRA_SOL_KEY_HEX.")
        return 2

    try:
        key = bytes.fromhex(key_hex)
    except ValueError:
        print("Invalid hexadecimal key")
        return 2

    try:
        build_package(args.mod_dir.resolve(), args.output.resolve(), key)
    except Exception as exc:
        print(f"ERROR: {exc}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
