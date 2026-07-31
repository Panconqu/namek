#ifndef NAMEK_CRYPTO_H
#define NAMEK_CRYPTO_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace namek {
namespace crypto {

struct Sha256Digest {
    uint8_t bytes[32];
    std::string to_hex() const;
};

// SHA-256 one-shot hashing
Sha256Digest sha256(const void* data, size_t len);
std::string sha256_hex(const std::string& input);

// MD5 one-shot hashing (hex string)
std::string md5_hex(const std::string& input);

// ChaCha20 stream cipher (RFC 7539: 256-bit key, 12-byte nonce, 32-bit counter)
// XORs `data` in place with the keystream.
void chacha20_xor(uint8_t* data, size_t len, const uint8_t key[32],
                  const uint8_t nonce[12], uint32_t counter = 0);

// Fill `out` with `len` random bytes (getrandom / std::random_device)
void random_bytes(uint8_t* out, size_t len);

// Constant-time comparison
bool safe_equal(const uint8_t* a, const uint8_t* b, size_t len);

} // namespace crypto
} // namespace namek

#endif // NAMEK_CRYPTO_H
