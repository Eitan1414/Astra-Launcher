#include "solar/SolCrypto.hpp"

#include <mbedtls/chachapoly.h>

namespace Solar::SolCrypto {
namespace {

void SetError(std::string *error, const char *message) {
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

bool DecryptChaCha20Poly1305(const uint8_t *key,
                             const uint8_t *nonce,
                             const uint8_t *aad,
                             size_t aadSize,
                             const uint8_t *encrypted,
                             size_t encryptedSize,
                             std::vector<uint8_t> &plaintext,
                             std::string *error) {
    plaintext.clear();

    if (key == nullptr || nonce == nullptr || encrypted == nullptr) {
        SetError(error, "invalid SOL crypto argument");
        return false;
    }

    if (encryptedSize < TagSize) {
        SetError(error, "encrypted SOL record is smaller than authentication tag");
        return false;
    }

    const size_t ciphertextSize = encryptedSize - TagSize;
    const uint8_t *tag = encrypted + ciphertextSize;

    plaintext.resize(ciphertextSize);

    mbedtls_chachapoly_context context;
    mbedtls_chachapoly_init(&context);

    int result = mbedtls_chachapoly_setkey(&context, key);
    if (result == 0) {
        result = mbedtls_chachapoly_auth_decrypt(
            &context,
            ciphertextSize,
            nonce,
            aad,
            aadSize,
            tag,
            encrypted,
            plaintext.data());
    }

    mbedtls_chachapoly_free(&context);

    if (result != 0) {
        plaintext.clear();
        SetError(error, "SOL authentication/decryption failed");
        return false;
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace Solar::SolCrypto
