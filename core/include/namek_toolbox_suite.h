#ifndef NAMEK_TOOLBOX_SUITE_H
#define NAMEK_TOOLBOX_SUITE_H

#include <string>
#include <vector>
#include <map>

namespace namek {

// ==========================================
// 1. CRYPTO & SECURITY SUITE
// ==========================================
class CryptoTools {
public:
    static std::string base64_encode(const std::string& input);
    static std::string base64_decode(const std::string& input);
    static std::string hex_encode(const std::string& input);
    static std::string hex_decode(const std::string& input);
    static std::string sha256(const std::string& input);
    static std::string md5(const std::string& input);
    static std::string xor_cipher(const std::string& input, const std::string& key);
    static std::string generate_api_key(size_t length = 32);
};

// ==========================================
// 2. NETWORK & API INSPECTOR
// ==========================================
class NetworkTools {
public:
    static bool scan_port(const std::string& host, int port, int timeout_ms = 1000);
    static void http_benchmark(const std::string& url, int requests = 50, int concurrency = 5);
    static std::string get_ip_info();
};

// ==========================================
// 3. FAKE DATA & SEED GENERATOR
// ==========================================
class DataGenerator {
public:
    static std::string fake_name();
    static std::string fake_email();
    static std::string fake_ip();
    static std::string fake_credit_card();
    static std::string generate_jwt_mock(const std::string& payload_json);
    static std::string format_json(const std::string& json_raw);
};

// ==========================================
// 4. SYSTEM & MONSTER BINARY BUILDER (UP TO 1GB)
// ==========================================
class SystemTools {
public:
    static void print_system_info();
    static bool generate_large_file(const std::string& filepath, size_t size_in_mb);
    static bool build_monster_cli(const std::string& output_bin, size_t target_size_mb);
};

} // namespace namek

#endif // NAMEK_TOOLBOX_SUITE_H
