#include "namek_binary_format.h"
#include "namek_crypto.h"
#include "namek.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace namek {

namespace {

const uint8_t* magic_for_type(TBBModuleType type) {
    switch (type) {
        case TBBModuleType::CORE:    return TBB_MAGIC_CORE;
        case TBBModuleType::TOOLBOX: return TBB_MAGIC_TOOLBOX;
        case TBBModuleType::MV:      return TBB_MAGIC_MV;
        default:                     return TBB_MAGIC_GENERIC;
    }
}

bool is_known_magic(const uint8_t* m) {
    return std::memcmp(m, TBB_MAGIC_CORE, 4) == 0 ||
           std::memcmp(m, TBB_MAGIC_TOOLBOX, 4) == 0 ||
           std::memcmp(m, TBB_MAGIC_MV, 4) == 0 ||
           std::memcmp(m, TBB_MAGIC_GENERIC, 4) == 0;
}

std::string module_type_name(TBBModuleType type) {
    switch (type) {
        case TBBModuleType::CORE:    return "core";
        case TBBModuleType::TOOLBOX: return "toolbox";
        case TBBModuleType::MV:      return "mv_engine";
        default:                     return "generic";
    }
}

void serialize_header(std::ostream& out, const TBBinaryHeader& h) {
    out.write(reinterpret_cast<const char*>(h.magic), 4);
    out.write(reinterpret_cast<const char*>(&h.version), sizeof(h.version));
    out.write(reinterpret_cast<const char*>(&h.flags), sizeof(h.flags));
    out.write(reinterpret_cast<const char*>(&h.module_type), sizeof(h.module_type));
    out.write(reinterpret_cast<const char*>(&h.payload_size), sizeof(h.payload_size));
    out.write(reinterpret_cast<const char*>(h.salt), 16);
    out.write(reinterpret_cast<const char*>(h.nonce), 12);
    out.write(reinterpret_cast<const char*>(h.checksum), 32);
}

bool deserialize_header(std::istream& in, TBBinaryHeader& h) {
    in.read(reinterpret_cast<char*>(h.magic), 4);
    in.read(reinterpret_cast<char*>(&h.version), sizeof(h.version));
    in.read(reinterpret_cast<char*>(&h.flags), sizeof(h.flags));
    in.read(reinterpret_cast<char*>(&h.module_type), sizeof(h.module_type));
    in.read(reinterpret_cast<char*>(&h.payload_size), sizeof(h.payload_size));
    in.read(reinterpret_cast<char*>(h.salt), 16);
    in.read(reinterpret_cast<char*>(h.nonce), 12);
    in.read(reinterpret_cast<char*>(h.checksum), 32);
    return in.good();
}

crypto::Sha256Digest derive_key(const uint8_t salt[16], const std::string& release_key) {
    std::string material(reinterpret_cast<const char*>(salt), 16);
    material += release_key;
    return crypto::sha256(material.data(), material.size());
}

} // anonymous namespace

bool TBBinaryCompiler::compile_to_binary(const std::string& script_content,
                                         const std::string& output_tb_bin_path,
                                         TBBModuleType type,
                                         const std::string& release_key,
                                         uint8_t flags) {
    std::string plaintext = std::string(TBB_PLAINTEXT_MARKER) + script_content;

    uint8_t salt[16];
    uint8_t nonce[12];
    crypto::random_bytes(salt, sizeof(salt));
    crypto::random_bytes(nonce, sizeof(nonce));

    crypto::Sha256Digest key = derive_key(salt, release_key);

    std::vector<uint8_t> cipher(plaintext.begin(), plaintext.end());
    crypto::chacha20_xor(cipher.data(), cipher.size(), key.bytes, nonce);

    crypto::Sha256Digest checksum = crypto::sha256(cipher.data(), cipher.size());

    TBBinaryHeader header;
    std::memcpy(header.magic, magic_for_type(type), 4);
    header.version = TBB_VERSION;
    header.flags = flags;
    header.module_type = static_cast<uint8_t>(type);
    header.payload_size = static_cast<uint32_t>(cipher.size());
    std::memcpy(header.salt, salt, 16);
    std::memcpy(header.nonce, nonce, 12);
    std::memcpy(header.checksum, checksum.bytes, 32);

    std::ofstream out(output_tb_bin_path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << color::RED << "Error: No se pudo escribir '" << output_tb_bin_path << "'." << color::RESET << "\n";
        return false;
    }
    serialize_header(out, header);
    out.write(reinterpret_cast<const char*>(cipher.data()), cipher.size());
    out.close();
    return true;
}

std::string TBBinaryCompiler::decode_binary(const std::string& tb_bin_path,
                                            const std::string& release_key) {
    std::ifstream in(tb_bin_path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << color::RED << "Error: No se pudo abrir '" << tb_bin_path << "'." << color::RESET << "\n";
        return "";
    }

    TBBinaryHeader header;
    if (!deserialize_header(in, header)) {
        std::cerr << color::RED << "Error: Cabecera incompleta en '" << tb_bin_path << "'." << color::RESET << "\n";
        return "";
    }

    if (!is_known_magic(header.magic)) {
        std::cerr << color::RED << "Error: '" << tb_bin_path << "' no es un binario .tb.bin válido." << color::RESET << "\n";
        return "";
    }
    if (header.version != TBB_VERSION) {
        std::cerr << color::RED << "Error: Versión de formato no soportada (" << header.version << ") en '" << tb_bin_path << "'." << color::RESET << "\n";
        return "";
    }

    std::vector<uint8_t> cipher(header.payload_size);
    in.read(reinterpret_cast<char*>(cipher.data()), header.payload_size);
    in.close();

    crypto::Sha256Digest actual = crypto::sha256(cipher.data(), cipher.size());
    if (!crypto::safe_equal(actual.bytes, header.checksum, 32)) {
        std::cerr << color::RED << "Error: Checksum SHA-256 corrupto en '" << tb_bin_path << "'." << color::RESET << "\n";
        return "";
    }

    crypto::Sha256Digest key = derive_key(header.salt, release_key);
    crypto::chacha20_xor(cipher.data(), cipher.size(), key.bytes, header.nonce);

    const size_t marker_len = std::strlen(TBB_PLAINTEXT_MARKER);
    if (cipher.size() < marker_len ||
        std::memcmp(cipher.data(), TBB_PLAINTEXT_MARKER, marker_len) != 0) {
        std::cerr << color::RED << "Error: Clave de descifrado incorrecta o marcador ausente en '" << tb_bin_path << "'." << color::RESET << "\n";
        return "";
    }

    return std::string(cipher.begin() + marker_len, cipher.end());
}

std::string TBBinaryCompiler::resolve_release_key_for(const std::string& tb_bin_path) {
    const char* env_key = std::getenv("NAMEK_KEY");
    if (env_key && *env_key) return std::string(env_key);

    // Look for release_manifest.json next to the binary, or one directory up
    std::vector<std::string> candidates;
    size_t slash = tb_bin_path.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? "." : tb_bin_path.substr(0, slash);
    candidates.push_back(dir + "/release_manifest.json");
    candidates.push_back(dir + "/../release_manifest.json");

    for (const auto& path : candidates) {
        if (!Utils::file_exists(path)) continue;
        std::string content = Utils::read_file(path);
        size_t key_pos = content.find("\"release_key\"");
        if (key_pos == std::string::npos) continue;
        size_t colon = content.find(':', key_pos);
        if (colon == std::string::npos) continue;
        size_t q1 = content.find('"', colon);
        if (q1 == std::string::npos) continue;
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        return content.substr(q1 + 1, q2 - q1 - 1);
    }
    return "";
}

bool TBBinaryCompiler::build_release_binary_trio(const std::string& output_dir,
                                                 const std::string& release_key) {
    std::string core_script = "# Namek Core Binary Execution Engine (core.tb.bin)\n"
                              "tb print \"[+] Inicializando Motor Core Binario Namek v2\"\n"
                              "tb set sys_mode \"release_binary_v2\"\n"
                              "tb set engine_status \"active\"\n";

    std::string toolbox_script = "# Namek Toolbox Utility Registry Binary (toolbox.tb.bin)\n"
                                 "tb print \"[+] Registro de Herramientas Binarias Cargado (Crypto, Net, Data, DB)\"\n"
                                 "tb set api key \"sk_prod_binary_encrypted_v2\"\n"
                                 "tb set api endpoint \"https://api.namek.bin/v2\"\n";

    std::string mv_script = "# Namek Custom MV Obfuscated Runtime (mv_engine.tb.bin)\n"
                            "tb print \"[+] Máquina Virtual Nivel 5 Activada (5 capas)\"\n"
                            "tb set mv_level \"5\"\n"
                            "tb set mv_protection \"chacha20_armored_layered\"\n";

    bool ok1 = compile_to_binary(core_script, output_dir + "/core.tb.bin",
                                 TBBModuleType::CORE, release_key,
                                 TBB_FLAG_ENCRYPTED | TBB_FLAG_MV_LEVEL5);
    bool ok2 = compile_to_binary(toolbox_script, output_dir + "/toolbox.tb.bin",
                                 TBBModuleType::TOOLBOX, release_key, TBB_FLAG_ENCRYPTED);
    bool ok3 = compile_to_binary(mv_script, output_dir + "/mv_engine.tb.bin",
                                 TBBModuleType::MV, release_key,
                                 TBB_FLAG_ENCRYPTED | TBB_FLAG_MV_LEVEL5);

    return ok1 && ok2 && ok3;
}

} // namespace namek
