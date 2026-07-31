#include "namek_obfuscator.h"
#include "namek.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <unistd.h>

namespace namek {

std::string Obfuscator::generate_random_identifier(size_t length) {
    static const char charset[] = "0123456789abcdef";
    std::string res = "_0x";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    for (size_t i = 0; i < length; ++i) {
        res += charset[dis(gen)];
    }
    return res;
}

std::string Obfuscator::xor_encrypt(const std::string& input, char key) {
    std::string output = input;
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] ^ key;
    }
    return output;
}

std::string Obfuscator::obfuscate_high_level(const std::string& source_code, const std::string& language) {
    std::string key_id = generate_random_identifier(8);
    std::string decode_fn = generate_random_identifier(8);
    char xor_key = 0x7E;

    std::vector<int> encrypted_bytes;
    for (char c : source_code) {
        encrypted_bytes.push_back(static_cast<int>(c ^ xor_key));
    }

    std::ostringstream ss;
    if (language == "python") {
        ss << "# Namek High-Level Obfuscated Code\n";
        ss << key_id << " = " << (int)xor_key << "\n";
        ss << generate_random_identifier(10) << " = [102, 30, 44, 99]\n"; // Decoy array
        ss << "def " << decode_fn << "(b_arr):\n";
        ss << "    return ''.join(chr(b ^ " << key_id << ") for b in b_arr)\n";
        ss << key_id << "_payload = [";
        for (size_t i = 0; i < encrypted_bytes.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << encrypted_bytes[i];
        }
        ss << "]\n";
        ss << "exec(" << decode_fn << "(" << key_id << "_payload))\n";
    } else {
        // Node / JS
        ss << "// Namek High-Level Obfuscated JS\n";
        ss << "const " << key_id << " = " << (int)xor_key << ";\n";
        ss << "const " << decode_fn << " = (arr) => arr.map(b => String.fromCharCode(b ^ " << key_id << ")).join('');\n";
        ss << "const payload = [";
        for (size_t i = 0; i < encrypted_bytes.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << encrypted_bytes[i];
        }
        ss << "];\n";
        ss << "eval(" << decode_fn << "(payload));\n";
    }
    return ss.str();
}

namespace {

// Single-layer VM wrapper with random byte key + armor key.
// The bytecode is stored as a compact armored hex string (~2x per layer), so
// 5 nested layers stay practical. Keys are embedded as tagged XOR pairs so
// the bundled deobfuscator can deterministically recover them per layer.
std::string obfuscate_vm_single(const std::string& source_code, const std::string& language) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> key_dis(17, 251);

    int byte_key = key_dis(gen);
    int armor = key_dis(gen);
    int k1 = key_dis(gen);
    int k2 = armor ^ k1;   // k1 ^ k2 == armor
    int k3 = key_dis(gen);
    int k4 = byte_key ^ k3; // k3 ^ k4 == byte_key

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char c : source_code) {
        int b = (static_cast<int>(c) ^ byte_key) ^ armor;
        hex << std::setw(2) << (b & 0xFF);
    }
    std::string payload_hex = hex.str();

    std::string v1 = Obfuscator::generate_random_identifier(6);
    std::string v2 = Obfuscator::generate_random_identifier(6);
    std::string v3 = Obfuscator::generate_random_identifier(6);
    std::string v4 = Obfuscator::generate_random_identifier(6);
    std::string d = Obfuscator::generate_random_identifier(8);
    std::string p = Obfuscator::generate_random_identifier(8);

    std::ostringstream ss;
    if (language == "python") {
        ss << "# NAMEK VIRTUAL MACHINE (MV LEVEL) - capa armada\n";
        ss << "import sys\n";
        ss << v1 << " = " << k1 << "  #NK-KEY\n";
        ss << v2 << " = " << k2 << "  #NK-KEY\n";
        ss << v3 << " = " << k3 << "  #NK-KEY\n";
        ss << v4 << " = " << k4 << "  #NK-KEY\n";
        ss << "_arm = " << v1 << " ^ " << v2 << "\n";
        ss << "_bk = " << v3 << " ^ " << v4 << "\n";
        ss << d << " = \"" << payload_hex << "\"\n";
        ss << p << " = bytes(b ^ _arm ^ _bk for b in bytes.fromhex(" << d << "))\n";
        ss << "exec(" << p << ".decode())\n";
    } else {
        // Node JS VM engine
        ss << "// NAMEK VIRTUAL MACHINE (MV LEVEL) - capa armada\n";
        ss << "const " << v1 << " = " << k1 << "; //NK-KEY\n";
        ss << "const " << v2 << " = " << k2 << "; //NK-KEY\n";
        ss << "const " << v3 << " = " << k3 << "; //NK-KEY\n";
        ss << "const " << v4 << " = " << k4 << "; //NK-KEY\n";
        ss << "const _arm = " << v1 << " ^ " << v2 << ";\n";
        ss << "const _bk = " << v3 << " ^ " << v4 << ";\n";
        ss << "const " << d << " = Buffer.from(\"" << payload_hex << "\", \"hex\");\n";
        ss << "const " << p << " = Buffer.from(Array.from(" << d << ").map(b => (b ^ _arm) ^ _bk));\n";
        ss << "eval(" << p << ".toString(\"utf8\"));\n";
    }
    return ss.str();
}

std::vector<int> extract_int_array(const std::string& code) {
    size_t arr_start = code.find("[");
    size_t arr_end = code.find("]", arr_start);
    if (arr_start == std::string::npos || arr_end == std::string::npos) return {};

    std::string arr_str = code.substr(arr_start + 1, arr_end - arr_start - 1);
    auto tokens = Utils::split(arr_str, ',');
    std::vector<int> values;
    for (const auto& t : tokens) {
        std::string tok = Utils::trim(t);
        if (tok.empty()) continue;
        try {
            values.push_back(std::stoi(tok));
        } catch (...) {}
    }
    return values;
}

int extract_key_constants(const std::string& code, int out[4]) {
    // Collects ints from lines tagged with NK-KEY (in order of appearance).
    std::istringstream iss(code);
    std::string line;
    int count = 0;
    while (std::getline(iss, line)) {
        if (line.find("NK-KEY") == std::string::npos) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string rhs = Utils::trim(line.substr(eq + 1));
        // strip trailing comment markers
        size_t cm = rhs.find("//");
        if (cm != std::string::npos) rhs = Utils::trim(rhs.substr(0, cm));
        size_t hashes = rhs.find('#');
        if (hashes != std::string::npos) rhs = Utils::trim(rhs.substr(0, hashes));
        try {
            if (count < 4) out[count++] = std::stoi(rhs);
        } catch (...) {}
    }
    return count;
}

std::string extract_hex_payload(const std::string& code) {
    // Find the first quoted string that is fully valid hex (the armored payload).
    size_t pos = 0;
    while (pos < code.size()) {
        size_t q1 = code.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = code.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string hex = code.substr(q1 + 1, q2 - q1 - 1);
        if (!hex.empty() && hex.size() % 2 == 0) {
            bool valid = true;
            for (char c : hex) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) { valid = false; break; }
            }
            if (valid) return hex;
        }
        pos = q2 + 1;
    }
    return "";
}

int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

std::string decode_hex_layer(const std::string& code, int armor, int byte_key) {
    std::string hex = extract_hex_payload(code);
    if (hex.empty()) return "";
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int b = (hex_nibble(hex[i]) << 4) | hex_nibble(hex[i + 1]);
        out += static_cast<char>((b ^ armor) ^ byte_key);
    }
    return out;
}

std::string decode_legacy_opcode_layer(const std::string& code) {
    // Old single-layer format: fixed 0x3F key, opcode pairs 0x10/0x99.
    size_t arr_start = code.find("[");
    size_t arr_end = code.find("]", arr_start);
    if (arr_start == std::string::npos || arr_end == std::string::npos) return "";
    std::string arr_str = code.substr(arr_start + 1, arr_end - arr_start - 1);
    auto tokens = Utils::split(arr_str, ',');
    std::vector<int> opcodes;
    for (const auto& t : tokens) {
        std::string tok = Utils::trim(t);
        if (tok.empty()) continue;
        try { opcodes.push_back(std::stoi(tok)); } catch (...) {}
    }
    std::string decoded;
    char key = 0x3F;
    size_t i = 0;
    while (i < opcodes.size()) {
        if (opcodes[i] == 0x10) {
            if (i + 1 < opcodes.size()) {
                decoded += static_cast<char>(opcodes[i + 1] ^ key);
                i += 2;
            } else {
                i += 1;
            }
        } else if (opcodes[i] == 0x99) {
            break;
        } else {
            i += 1;
        }
    }
    return decoded;
}

} // anonymous namespace

std::string Obfuscator::obfuscate_vm(const std::string& source_code, const std::string& language, int layers) {
    if (layers < 1) layers = 1;
    std::string result = source_code;
    for (int i = 0; i < layers; ++i) {
        result = obfuscate_vm_single(result, language);
    }
    return result;
}

std::string Obfuscator::deobfuscate_vm(const std::string& obf_vm_code) {
    std::string code = obf_vm_code;
    int layer = 0;
    while (code.find("NK-KEY") != std::string::npos) {
        layer++;
        int keys[4] = {0, 0, 0, 0};
        if (extract_key_constants(code, keys) < 4) {
            return "// Error: No se pudieron extraer las claves de la capa " + std::to_string(layer) + ".";
        }
        int armor = keys[0] ^ keys[1];
        int byte_key = keys[2] ^ keys[3];

        std::string decoded = decode_hex_layer(code, armor, byte_key);
        if (decoded.empty()) {
            return "// Error: No se encontró payload VM en la capa " + std::to_string(layer) + ".";
        }
        code = decoded;
    }
    // Fallback for legacy single-layer opcode format (fixed 0x3F key)
    if (code.find("0x99") != std::string::npos) {
        std::string legacy = decode_legacy_opcode_layer(code);
        if (!legacy.empty()) return legacy;
    }
    return code;
}

std::string Obfuscator::deobfuscate_high_level(const std::string& obf_code) {
    size_t arr_start = obf_code.find("[");
    size_t arr_end = obf_code.find("]", arr_start);
    if (arr_start == std::string::npos || arr_end == std::string::npos) return "// Error: Formato no soportado.";

    std::string arr_str = obf_code.substr(arr_start + 1, arr_end - arr_start - 1);
    auto tokens = Utils::split(arr_str, ',');

    std::string decompiled;
    char key = 0x7E;

    for (const auto& t : tokens) {
        std::string tok = Utils::trim(t);
        if (!tok.empty() && tok[0] >= '0' && tok[0] <= '9') {
            int val = std::stoi(tok);
            decompiled += (char)(val ^ key);
        }
    }
    return decompiled;
}

std::string Obfuscator::inject_anti_debug_guard(const std::string& code) {
    std::string guard = R"(
import sys
# Namek Dynamic Anti-Analysis & Anti-Debug Trap
if hasattr(sys, 'gettrace') and sys.gettrace() is not None:
    print("[!] Anti-Debug Triggered: Execution Aborted.")
    sys.exit(1)
_xtrace = getattr(sys, 'gettrace', None)
if _xtrace is not None and _xtrace() is not None:
    print("[!] Anti-Debug Guard: Execution Aborted.")
    sys.exit(1)
)";
    return guard + "\n" + code;
}

bool Obfuscator::compile_native(const std::string& source_code, const std::string& output_binary_path, const std::string& language) {

    std::string temp_cpp = "/tmp/namek_obfuscated_app.cpp";
    char xor_key = 0x43;

    std::vector<int> encrypted;
    for (char c : source_code) {
        encrypted.push_back(static_cast<int>(c ^ xor_key));
    }

    std::ofstream out(temp_cpp);
    if (!out.is_open()) return false;

    out << "#include <iostream>\n";
    out << "#include <vector>\n";
    out << "#include <cstdlib>\n";
    out << "#include <fstream>\n";
    out << "#include <unistd.h>\n\n";

    out << "const unsigned char payload[] = {";
    for (size_t i = 0; i < encrypted.size(); ++i) {
        if (i > 0) out << ",";
        out << encrypted[i];
    }
    out << "};\n";
    out << "const size_t payload_len = sizeof(payload);\n";
    out << "const char key = " << (int)xor_key << ";\n\n";

    out << "int main() {\n";
    if (language == "python") {
        out << "  std::string script;\n";
        out << "  for (size_t i = 0; i < payload_len; ++i) script += (char)(payload[i] ^ key);\n";
        out << "  std::string tmp_file = \"/tmp/.namek_runtime_\" + std::to_string(getpid()) + \".py\";\n";
        out << "  std::ofstream f(tmp_file);\n";
        out << "  f << script;\n";
        out << "  f.close();\n";
        out << "  int res = system((\"python3 \" + tmp_file).c_str());\n";
        out << "  unlink(tmp_file.c_str());\n";
        out << "  return res;\n";
    } else {
        out << "  std::string script;\n";
        out << "  for (size_t i = 0; i < payload_len; ++i) script += (char)(payload[i] ^ key);\n";
        out << "  std::string tmp_file = \"/tmp/.namek_runtime_\" + std::to_string(getpid()) + \".js\";\n";
        out << "  std::ofstream f(tmp_file);\n";
        out << "  f << script;\n";
        out << "  f.close();\n";
        out << "  int res = system((\"node \" + tmp_file).c_str());\n";
        out << "  unlink(tmp_file.c_str());\n";
        out << "  return res;\n";
    }
    out << "}\n";
    out.close();

    std::string build_cmd = "g++ -O3 -s " + temp_cpp + " -o " + output_binary_path;
    int code = system(build_cmd.c_str());
    unlink(temp_cpp.c_str());
    return (code == 0);
}

} // namespace namek
