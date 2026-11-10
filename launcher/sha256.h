// sha256.h -- SHA-256 (FIPS 180-4), for checking the downloaded game files.
#pragma once

#include <cstddef>
#include <string>

// The digest of `size` bytes at `data`, as 64 lowercase hex digits.
std::string Sha256Hex(const void* data, size_t size);
