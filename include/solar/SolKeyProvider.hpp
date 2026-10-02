#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace Solar::SolKeyProvider {

// Returns the compile-time SOL package key when one was supplied to the build.
// No production key is committed to the repository.
bool GetKey(std::array<uint8_t, 32> &key, std::string *error = nullptr);

} // namespace Solar::SolKeyProvider
