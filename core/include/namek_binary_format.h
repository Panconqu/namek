#ifndef NAMEK_BINARY_FORMAT_H
#define NAMEK_BINARY_FORMAT_H

#include <string>
#include <vector>
#include <cstdint>

namespace namek {

// Module types for the release binary trio
enum class TBBModuleType : uint8_t {
    GENERIC = 0,
    CORE    = 1,
    TOOLBOX = 2,
    MV      = 3
};

// Per-type magic signatures for Namek Binary format v2
const uint8_t TBB_MAGIC_CORE[4]    = {'N', 'K', 'C', 'R'};
const uint8_t TBB_MAGIC_TOOLBOX[4] = {'N', 'K', 'T', 'B'};
const uint8_t TBB_MAGIC_MV[4]      = {'N', 'K', 'M', 'V'};
const uint8_t TBB_MAGIC_GENERIC[4] = {'N', 'K', 'B', '1'};

constexpr uint16_t TBB_VERSION          = 2;
constexpr uint8_t  TBB_FLAG_ENCRYPTED   = 0x01;
constexpr uint8_t  TBB_FLAG_MV_LEVEL5   = 0x02;
constexpr size_t   TBB_HEADER_SIZE      = 72;

// Plaintext marker prepended to the payload so wrong keys are detected
constexpr char TBB_PLAINTEXT_MARKER[] = "NK2";

struct TBBinaryHeader {
    uint8_t magic[4];        // per-type magic
    uint16_t version;        // TBB_VERSION
    uint8_t flags;           // TBB_FLAG_*
    uint8_t module_type;     // TBBModuleType
    uint32_t payload_size;   // ciphertext bytes after header
    uint8_t salt[16];        // per-file random salt
    uint8_t nonce[12];       // per-file random nonce
    uint8_t checksum[32];    // SHA-256 of ciphertext
};

class TBBinaryCompiler {
public:
    // Compiles a text script into an encrypted .tb.bin (format v2, ChaCha20)
    // release_key can be empty: a key is derived only from the salt.
    static bool compile_to_binary(const std::string& script_content,
                                  const std::string& output_tb_bin_path,
                                  TBBModuleType type = TBBModuleType::GENERIC,
                                  const std::string& release_key = "",
                                  uint8_t flags = TBB_FLAG_ENCRYPTED);

    // Reads and decrypts an encrypted .tb.bin file (validates magic, version,
    // SHA-256 checksum and plaintext marker).
    static std::string decode_binary(const std::string& tb_bin_path,
                                     const std::string& release_key = "");

    // Resolves the release key for a given .tb.bin:
    // 1) NAMEK_KEY environment variable
    // 2) release_manifest.json next to the file (or ../release_manifest.json)
    static std::string resolve_release_key_for(const std::string& tb_bin_path);

    // Creates the 3 core release binaries: core.tb.bin, toolbox.tb.bin,
    // mv_engine.tb.bin under output_dir using the given release_key.
    static bool build_release_binary_trio(const std::string& output_dir,
                                          const std::string& release_key);
};

} // namespace namek

#endif // NAMEK_BINARY_FORMAT_H
