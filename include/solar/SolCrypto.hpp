#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Solar::SolCrypto {

constexpr size_t KeySize = 32;
constexpr size_t NonceSize = 12;
constexpr size_t TagSize = 16;

bool DecryptChaCha20Poly1305(const uint8_t *key,
                             const uint8_t *nonce,
                             const uint8_t *aad,
                             size_t aadSize,
                             const uint8_t *encrypted,
                             size_t encryptedSize,
                             std::vector<uint8_t> &plaintext,
                             std::string *error = nullptr);

} // namespace Solar::SolCrypto
