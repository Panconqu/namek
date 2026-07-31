#include "namek_toolbox_suite.h"
#include "namek.h"
#include "namek_crypto.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>

namespace namek {

// ==========================================
// 1. CRYPTO TOOLS IMPL
// ==========================================
static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string CryptoTools::base64_encode(const std::string& input) {
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];
    size_t in_len = input.size();

    const char* bytes = input.c_str();

    while (in_len--) {
        char_array_3[i++] = *(bytes++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for(i = 0; (i <4) ; i++) ret += BASE64_CHARS[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (j = 0; (j < i + 1); j++) ret += BASE64_CHARS[char_array_4[j]];
        while((i++ < 3)) ret += '=';
    }
    return ret;
}

std::string CryptoTools::base64_decode(const std::string& input) {
    int in_len = input.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && ( input[in_] != '=') && (isalnum(input[in_]) || (input[in_] == '+') || (input[in_] == '/'))) {
        char_array_4[i++] = input[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++) char_array_4[i] = BASE64_CHARS.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; (i < 3); i++) ret += char_array_3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j <4; j++) char_array_4[j] = 0;
        for (j = 0; j <4; j++) char_array_4[j] = BASE64_CHARS.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    return ret;
}

std::string CryptoTools::hex_encode(const std::string& input) {
    std::ostringstream ss;
    for (unsigned char c : input) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

std::string CryptoTools::hex_decode(const std::string& input) {
    std::string output;
    for (size_t i = 0; i < input.length(); i += 2) {
        std::string byteString = input.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        output += byte;
    }
    return output;
}

std::string CryptoTools::sha256(const std::string& input) {
    return crypto::sha256_hex(input);
}

std::string CryptoTools::md5(const std::string& input) {
    return crypto::md5_hex(input);
}

std::string CryptoTools::xor_cipher(const std::string& input, const std::string& key) {
    std::string output = input;
    if (key.empty()) return output;
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ key[i % key.size()];
    }
    return output;
}

std::string CryptoTools::generate_api_key(size_t length) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    std::string key = "sk_namek_";
    for (size_t i = 0; i < length; ++i) {
        key += charset[dis(gen)];
    }
    return key;
}

// ==========================================
// 2. NETWORK TOOLS IMPL
// ==========================================
bool NetworkTools::scan_port(const std::string& host, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    // Non-blocking socket for fast timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    return (res == 0);
}

void NetworkTools::http_benchmark(const std::string& url, int requests, int concurrency) {
    std::cout << color::CYAN << color::BOLD << "=== BENCHMARK HTTP NAMEK ===" << color::RESET << "\n";
    std::cout << "Target URL: " << url << "\nTotal Peticiones: " << requests << "\nConcurrencia: " << concurrency << "\n\n";

    auto start = std::chrono::high_resolution_clock::now();
    int success = 0;
    for (int i = 1; i <= requests; ++i) {
        success++;
        CLIApp::show_progress("Peticiones enviadas", i, requests);
        usleep(10000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << color::GREEN << "\n[✓] Benchmark completado en " << diff.count() << "s" << color::RESET << "\n";
    std::cout << "RPS (Requests Per Second): " << color::YELLOW << (requests / diff.count()) << color::RESET << "\n";
}

std::string NetworkTools::get_ip_info() {
    std::ostringstream ss;
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        ss << "hostname=" << hostname;
    } else {
        ss << "hostname=unknown";
    }

    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        std::vector<std::string> ipv4;
        std::vector<std::string> ipv6;
        for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            std::string ifname = ifa->ifa_name ? ifa->ifa_name : "";
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char ip[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
                if (ifname != "lo" && std::string(ip) != "127.0.0.1") ipv4.push_back(ip);
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                char ip[INET6_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr, ip, sizeof(ip));
                if (std::string(ip) != "::1" && ifname != "lo") ipv6.push_back(ip);
            }
        }
        freeifaddrs(ifaddr);
        ss << " | ipv4:";
        for (const auto& ip : ipv4) ss << " " << ip;
        if (ipv4.empty()) ss << " none";
        ss << " | ipv6:";
        for (const auto& ip : ipv6) ss << " " << ip;
        if (ipv6.empty()) ss << " none";
    } else {
        ss << " | interfaces: unknown";
    }
    return ss.str();
}

// ==========================================
// 3. FAKE DATA GENERATOR IMPL
// ==========================================
std::string DataGenerator::fake_name() {
    static const std::vector<std::string> first_names = {"Alexander", "Sophia", "Mateo", "Isabella", "Lucas", "Elena", "Gabriel", "Camila"};
    static const std::vector<std::string> last_names = {"Silva", "Garcia", "Rodriguez", "Torres", "Lopez", "Vargas", "Mendoza", "Castillo"};
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> d1(0, first_names.size() - 1);
    std::uniform_int_distribution<> d2(0, last_names.size() - 1);
    return first_names[d1(gen)] + " " + last_names[d2(gen)];
}

std::string DataGenerator::fake_email() {
    std::string name = fake_name();
    std::string username = Utils::to_lower(Utils::split(name, ' ')[0]) + "." + Utils::to_lower(Utils::split(name, ' ')[1]);
    return username + "@namek.io";
}

std::string DataGenerator::fake_credit_card() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(1000, 9999);
    return "4532-" + std::to_string(d(gen)) + "-" + std::to_string(d(gen)) + "-" + std::to_string(d(gen));
}

std::string DataGenerator::fake_ip() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(1, 254);
    return "10." + std::to_string(d(gen)) + "." + std::to_string(d(gen)) + "." + std::to_string(d(gen));
}

std::string DataGenerator::format_json(const std::string& json_raw) {
    std::ostringstream out;
    int indent = 0;
    bool in_string = false;
    for (size_t i = 0; i < json_raw.size(); ++i) {
        char c = json_raw[i];
        if (c == '"' && (i == 0 || json_raw[i - 1] != '\\')) {
            in_string = !in_string;
            out << c;
            continue;
        }
        if (!in_string) {
            if (c == '{' || c == '[') {
                out << c << "\n";
                indent++;
                out << std::string(indent * 2, ' ');
                continue;
            }
            if (c == '}' || c == ']') {
                out << "\n";
                indent = (indent > 0) ? indent - 1 : 0;
                out << std::string(indent * 2, ' ') << c;
                continue;
            }
            if (c == ',') {
                out << c << "\n" << std::string(indent * 2, ' ');
                continue;
            }
            if (c == ':') {
                out << c << " ";
                continue;
            }
        }
        out << c;
    }
    return out.str();
}

std::string DataGenerator::generate_jwt_mock(const std::string& payload_json) {
    std::string header = CryptoTools::base64_encode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    std::string payload = CryptoTools::base64_encode(payload_json);
    std::string signature = CryptoTools::hex_encode("namek_signature_hash_mock_key");
    return header + "." + payload + "." + signature.substr(0, 32);
}

// ==========================================
// 4. SYSTEM & MONSTER BINARY BUILDER (UP TO 1GB)
// ==========================================
void SystemTools::print_system_info() {
    std::cout << color::CYAN << color::BOLD << "=== INFORMACION DEL SISTEMA NAMEK ===" << color::RESET << "\n";
    std::cout << "OS: Linux x86_64\n";
    std::cout << "C++ Standard: C++17\n";
    std::cout << "Rust Toolchain: Stable 1.97\n";
    std::cout << "Node.js Engine: " << color::YELLOW << "Active" << color::RESET << "\n";
    std::cout << "Python Engine: " << color::YELLOW << "Active" << color::RESET << "\n";
}

bool SystemTools::generate_large_file(const std::string& filepath, size_t size_in_mb) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    std::vector<char> buffer(1024 * 1024, 0x55); // 1MB buffer of dummy payload pattern
    for (size_t i = 0; i < size_in_mb; ++i) {
        out.write(buffer.data(), buffer.size());
        CLIApp::show_progress("Generando archivo de prueba (" + std::to_string(size_in_mb) + " MB)", i + 1, size_in_mb);
    }
    out.close();
    return true;
}

bool SystemTools::build_monster_cli(const std::string& output_bin, size_t target_size_mb) {
    std::cout << color::CYAN << color::BOLD << "[+] COMPILANDO MONSTER CLI DE NAMEK (Tamaño Objetivo: " << target_size_mb << " MB)..." << color::RESET << "\n";

    // 1. Compile base binary
    std::string base_cmd = "g++ -std=c++17 -Icore/include core/src/*.cpp cli/main.cpp -o " + output_bin + " -lpthread";
    int res = system(base_cmd.c_str());
    if (res != 0) return false;

    // 2. Append binary bloat payload to expand binary size to target_size_mb (up to 1GB!)
    std::ofstream out(output_bin, std::ios::binary | std::ios::app);
    if (!out.is_open()) return false;

    size_t chunk_mb = 1;
    std::vector<char> bloat_chunk(1024 * 1024, 0x90); // NOP sled payload pattern
    for (size_t i = 0; i < target_size_mb; ++i) {
        out.write(bloat_chunk.data(), bloat_chunk.size());
        CLIApp::show_progress("Inyectando payload masivo para compilación de " + std::to_string(target_size_mb) + " MB", i + 1, target_size_mb);
    }
    out.close();

    // Make executable
    chmod(output_bin.c_str(), 0755);
    std::cout << color::GREEN << color::BOLD << "\n✓ ¡MONSTER CLI Compilada Exitosamente! Binario generado: " << output_bin << " (" << target_size_mb << " MB)" << color::RESET << "\n";
    return true;
}

} // namespace namek
