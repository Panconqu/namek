#include "namek_crypto.h"

#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

#if defined(__linux__)
#include <sys/random.h>
#endif

namespace namek {
namespace crypto {

std::string Sha256Digest::to_hex() const {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 32; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

namespace {

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct SHA256Ctx {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t block[64];
    size_t block_len;

    SHA256Ctx() { init(); }

    void init() {
        h[0] = 0x6a09e667; h[1] = 0xbb67ae85;
        h[2] = 0x3c6ef372; h[3] = 0xa54ff53a;
        h[4] = 0x510e527f; h[5] = 0x9b05688c;
        h[6] = 0x1f83d9ab; h[7] = 0x5be0cd19;
        total_len = 0; block_len = 0;
    }

    void compress(const uint8_t* p) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
                   ((uint32_t)p[i * 4 + 2] << 8) | ((uint32_t)p[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        total_len += len;
        while (len > 0) {
            size_t space = 64 - block_len;
            size_t take = len < space ? len : space;
            std::memcpy(block + block_len, p, take);
            block_len += take;
            p += take;
            len -= take;
            if (block_len == 64) {
                compress(block);
                block_len = 0;
            }
        }
    }

    void finalize(uint8_t out[32]) {
        uint64_t bit_len = total_len * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0x00;
        while (block_len != 56) update(&zero, 1);
        uint8_t len_bytes[8];
        for (int i = 0; i < 8; ++i) {
            len_bytes[i] = static_cast<uint8_t>((bit_len >> (8 * (7 - i))) & 0xFF);
        }
        update(len_bytes, 8);
        for (int i = 0; i < 8; ++i) {
            out[i * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xFF);
            out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFF);
            out[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xFF);
            out[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xFF);
        }
    }
};

// --- MD5 ---
static const uint32_t S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};

static const uint32_t K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};

inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

std::string md5_hex_impl(const uint8_t* data, size_t len) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    size_t padded_len = (((len + 8) / 64) + 1) * 64;
    std::vector<uint8_t> buf(padded_len, 0);
    std::memcpy(buf.data(), data, len);
    buf[len] = 0x80;
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) {
        buf[padded_len - 8 + i] = static_cast<uint8_t>((bit_len >> (8 * i)) & 0xFF);
    }

    for (size_t off = 0; off < padded_len; off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; ++i) {
            M[i] = (uint32_t)buf[off + i * 4] | ((uint32_t)buf[off + i * 4 + 1] << 8) |
                   ((uint32_t)buf[off + i * 4 + 2] << 16) | ((uint32_t)buf[off + i * 4 + 3] << 24);
        }
        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i) {
            uint32_t F, g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
            else { F = C ^ (B | ~D); g = (7 * i) % 16; }
            F = F + A + K[i] + M[g];
            A = D; D = C; C = B;
            B = B + rotl(F, S[i]);
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    uint32_t out4[4] = {a0, b0, c0, d0};
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) {
        for (int b = 0; b < 4; ++b) {
            ss << std::setw(2) << ((out4[i] >> (8 * b)) & 0xFF);
        }
    }
    return ss.str();
}

// --- ChaCha20 core ---
#define QR(a, b, c, d) \
    (a += b, d ^= a, d = rotl(d, 16), c += d, b ^= c, b = rotl(b, 12), \
     a += b, d ^= a, d = rotl(d, 8), c += d, b ^= c, b = rotl(b, 7))

void chacha_block(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12],
                  uint32_t out[16]) {
    static const uint32_t constants[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
    uint32_t x[16];
    x[0] = constants[0]; x[1] = constants[1]; x[2] = constants[2]; x[3] = constants[3];
    for (int i = 0; i < 8; ++i) {
        x[4 + i] = (uint32_t)key[4 * i] | ((uint32_t)key[4 * i + 1] << 8) |
                   ((uint32_t)key[4 * i + 2] << 16) | ((uint32_t)key[4 * i + 3] << 24);
    }
    x[12] = counter;
    x[13] = (uint32_t)nonce[0] | ((uint32_t)nonce[1] << 8) | ((uint32_t)nonce[2] << 16) |
            ((uint32_t)nonce[3] << 24);
    x[14] = (uint32_t)nonce[4] | ((uint32_t)nonce[5] << 8) | ((uint32_t)nonce[6] << 16) |
            ((uint32_t)nonce[7] << 24);
    x[15] = (uint32_t)nonce[8] | ((uint32_t)nonce[9] << 8) | ((uint32_t)nonce[10] << 16) |
            ((uint32_t)nonce[11] << 24);

    uint32_t orig[16];
    std::memcpy(orig, x, sizeof(orig));
    for (int i = 0; i < 10; ++i) {
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; ++i) out[i] = x[i] + orig[i];
}

} // anonymous namespace

Sha256Digest sha256(const void* data, size_t len) {
    SHA256Ctx ctx;
    ctx.update(data, len);
    Sha256Digest digest;
    ctx.finalize(digest.bytes);
    return digest;
}

std::string sha256_hex(const std::string& input) {
    return sha256(input.data(), input.size()).to_hex();
}

std::string md5_hex(const std::string& input) {
    return md5_hex_impl(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

void chacha20_xor(uint8_t* data, size_t len, const uint8_t key[32],
                  const uint8_t nonce[12], uint32_t counter) {
    size_t offset = 0;
    while (offset < len) {
        uint32_t ks[16];
        chacha_block(key, counter++, nonce, ks);
        size_t chunk = (len - offset) < 64 ? (len - offset) : 64;
        for (size_t i = 0; i < chunk; ++i) {
            uint8_t kbyte = static_cast<uint8_t>((ks[i >> 2] >> (8 * (i & 3))) & 0xFF);
            data[offset + i] ^= kbyte;
        }
        offset += chunk;
    }
}

void random_bytes(uint8_t* out, size_t len) {
#if defined(__linux__)
    if (getrandom(out, len, 0) == static_cast<ssize_t>(len)) return;
#endif
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    for (size_t i = 0; i < len; ++i) {
        out[i] = static_cast<uint8_t>(dis(gen) & 0xFF);
    }
}

bool safe_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i) diff |= (a[i] ^ b[i]);
    return diff == 0;
}

} // namespace crypto
} // namespace namek
