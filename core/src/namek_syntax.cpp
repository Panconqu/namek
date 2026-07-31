#include "namek_syntax.h"
#include "namek_toolbox_suite.h"
#include "namek_binary_format.h"
#include "namek_packager.h"
#include "namek_lua.h"
#include "namek_mod_sandbox.h"
#include "namek_mod_marketplace.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <dirent.h>

namespace namek {

SyntaxEngine::SyntaxEngine(const std::string& db_path) : db(db_path) {}

LuaModEngine& SyntaxEngine::mod_engine() {
    if (!lua_engine) {
        lua_engine = std::make_unique<LuaModEngine>("namek_db.json");
    }
    return *lua_engine;
}

std::vector<std::string> SyntaxEngine::parse_tokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"' || c == '\'') {
            in_quotes = !in_quotes;
        } else if ((c == ' ' || c == '\t') && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool SyntaxEngine::execute_line(const std::string& line) {
    std::string trimmed = Utils::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return true;

    auto tokens = parse_tokens(trimmed);
    if (tokens.empty()) return true;

    std::string prefix = tokens[0];
    if (prefix != "tb" && prefix != "toolbox") {
        std::cerr << color::RED << "Error de sintaxis: Las sentencias deben comenzar con 'tb' o 'toolbox'." << color::RESET << "\n";
        return false;
    }

    if (tokens.size() < 2) {
        std::cerr << color::YELLOW << "Namek DSL: Uso -> tb [set|get|obfuscate|deobfuscate|compile|crypto|data|net|sys] ..." << color::RESET << "\n";
        return false;
    }

    std::string action = tokens[1];

    // Mods may hook every command dispatched by the engine.
    if (lua_engine) lua_engine->emit("on_command", action);

    // ==========================================
    // 1. tb set api key ... / endpoint / generate api
    // ==========================================
    if (action == "set") {
        if (tokens.size() >= 5 && tokens[2] == "api" && tokens[3] == "key") {
            db.set("api_key", tokens[4]);
            std::cout << color::GREEN << "✓ Namek DSL: API Key guardada -> " << color::CYAN << tokens[4] << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 5 && tokens[2] == "api" && tokens[3] == "endpoint") {
            db.set("api_endpoint", tokens[4]);
            std::cout << color::GREEN << "✓ Namek DSL: API Endpoint guardado -> " << color::CYAN << tokens[4] << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 5 && tokens[2] == "generate" && tokens[3] == "api") {
            db.set("generated_api", tokens[4]);
            std::cout << color::GREEN << "✓ Namek DSL: Configuración 'generate api' guardada -> " << color::CYAN << tokens[4] << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4) {
            db.set(tokens[2], tokens[3]);
            std::cout << color::GREEN << "✓ Namek DSL: Variable '" << tokens[2] << "' = '" << tokens[3] << "'" << color::RESET << "\n";
            return true;
        }
    }

    // ==========================================
    // 2. tb get ...
    // ==========================================
    if (action == "get") {
        if (tokens.size() >= 4 && tokens[2] == "api" && tokens[3] == "key") {
            std::cout << color::CYAN << "API Key: " << color::RESET << color::YELLOW << db.get("api_key", "(no configurada)") << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "api" && tokens[3] == "endpoint") {
            std::cout << color::CYAN << "API Endpoint: " << color::RESET << color::YELLOW << db.get("api_endpoint", "(no configurado)") << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 3) {
            std::cout << color::CYAN << tokens[2] << ": " << color::RESET << color::YELLOW << db.get(tokens[2], "(no encontrado)") << color::RESET << "\n";
            return true;
        }
    }

    // ==========================================
    // 3. tb obfuscate <file> --level <high|mv|compile>
    // ==========================================
    if (action == "obfuscate") {
        if (tokens.size() < 3) {
            std::cerr << color::RED << "Uso: tb obfuscate <archivo> --level <high|mv|compile> [--output <dest>]" << color::RESET << "\n";
            return false;
        }
        std::string file_path = tokens[2];
        std::string level = "mv";
        std::string output_path = file_path + ".obf";
        int layers = 5;

        for (size_t i = 3; i < tokens.size(); ++i) {
            if (tokens[i] == "--level" && i + 1 < tokens.size()) level = tokens[++i];
            else if (tokens[i] == "--output" && i + 1 < tokens.size()) output_path = tokens[++i];
            else if (tokens[i] == "--layers" && i + 1 < tokens.size()) {
                try { layers = std::stoi(tokens[++i]); } catch (...) {}
            }
        }

        if (!Utils::file_exists(file_path)) {
            std::cerr << color::RED << "Error: El archivo '" << file_path << "' no existe." << color::RESET << "\n";
            return false;
        }

        std::string source = Utils::read_file(file_path);
        std::string lang = (file_path.rfind(".js") != std::string::npos) ? "javascript" : "python";

        std::cout << color::CYAN << "[+] Ofuscando '" << file_path << "' con Nivel [" << level << "]..." << color::RESET << "\n";

        if (level == "compile" || level == "native") {
            return Obfuscator::compile_native(source, output_path, lang);
        } else if (level == "mv" || level == "vm") {
            std::string res = Obfuscator::obfuscate_vm(source, lang, layers);
            Utils::write_file(output_path, res);
            std::cout << color::GREEN << color::BOLD << "✓ ¡Ofuscación Nivel VIRTUAL MACHINE (MV) exitosa! Archivo: " << output_path << " (" << layers << " capas)" << color::RESET << "\n";
            return true;
        } else {
            std::string res = Obfuscator::obfuscate_high_level(source, lang);
            Utils::write_file(output_path, res);
            std::cout << color::GREEN << color::BOLD << "✓ ¡Ofuscación Nivel ALTO NVEL exitosa! Archivo: " << output_path << color::RESET << "\n";
            return true;
        }
    }

    // ==========================================
    // 4. tb deobfuscate <file.obf> [--output <dest>]
    // ==========================================
    if (action == "deobfuscate") {
        if (tokens.size() < 3) {
            std::cerr << color::RED << "Uso: tb deobfuscate <archivo_ofuscado> [--output <salida>]" << color::RESET << "\n";
            return false;
        }
        std::string input_file = tokens[2];
        std::string output_file = (tokens.size() >= 5 && tokens[3] == "--output") ? tokens[4] : (input_file + ".decompiled.py");

        if (!Utils::file_exists(input_file)) {
            std::cerr << color::RED << "Error: Archivo no encontrado " << input_file << color::RESET << "\n";
            return false;
        }

        std::string obf_content = Utils::read_file(input_file);
        std::cout << color::CYAN << "[+] Desofuscando Máquina Virtual (MV) en memoria..." << color::RESET << "\n";
        std::string decompiled = Obfuscator::deobfuscate_vm(obf_content);

        Utils::write_file(output_file, decompiled);
        std::cout << color::GREEN << color::BOLD << "✓ Código Desofuscado generado exitosamente en: " << output_file << color::RESET << "\n";
        return true;
    }

    // ==========================================
    // 5. tb compile --monster --size <size_mb>
    // ==========================================
    if (action == "compile") {
        std::string out_bin = "namek_monster_cli";
        size_t target_size = 100; // Default 100MB
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (tokens[i] == "--size" && i + 1 < tokens.size()) {
                std::string s_val = tokens[++i];
                if (s_val == "1GB" || s_val == "1000MB") target_size = 1000;
                else if (s_val == "500MB") target_size = 500;
                else target_size = std::stoi(s_val);
            } else if (tokens[i] == "--output" && i + 1 < tokens.size()) {
                out_bin = tokens[++i];
            }
        }
        return SystemTools::build_monster_cli(out_bin, target_size);
    }

    // ==========================================
    // 6. tb crypto / tb data / tb net / tb sys
    // ==========================================
    if (action == "crypto") {
        if (tokens.size() >= 4 && tokens[2] == "base64") {
            std::cout << CryptoTools::base64_encode(tokens[3]) << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "sha256") {
            std::cout << color::CYAN << "SHA-256: " << color::YELLOW << CryptoTools::sha256(tokens[3]) << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "md5") {
            std::cout << color::CYAN << "MD5: " << color::YELLOW << CryptoTools::md5(tokens[3]) << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "hex") {
            std::cout << color::CYAN << "Hex: " << color::YELLOW << CryptoTools::hex_encode(tokens[3]) << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 5 && tokens[2] == "xor") {
            std::cout << color::CYAN << "XOR: " << color::YELLOW << CryptoTools::xor_cipher(tokens[3], tokens[4]) << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 3 && tokens[2] == "key") {
            std::cout << color::CYAN << "API Key generada: " << color::BOLD << CryptoTools::generate_api_key(32) << color::RESET << "\n";
            return true;
        }
        std::cerr << color::YELLOW << "Uso: tb crypto [base64|sha256|md5|hex|xor|key] ..." << color::RESET << "\n";
        return false;
    }

    if (action == "data") {
        if (tokens.size() >= 3 && tokens[2] == "fake") {
            std::cout << "Nombre: " << color::YELLOW << DataGenerator::fake_name() << color::RESET << "\n";
            std::cout << "Email: " << color::CYAN << DataGenerator::fake_email() << color::RESET << "\n";
            std::cout << "Tarjeta: " << color::MAGENTA << DataGenerator::fake_credit_card() << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 3 && tokens[2] == "fake-ip") {
            std::cout << "IP Ficticia: " << color::CYAN << DataGenerator::fake_ip() << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 3 && tokens[2] == "jwt") {
            std::string payload = (tokens.size() >= 4) ? tokens[3] : "{\"sub\":\"namek-user\"}";
            std::cout << "JWT Mock: " << color::CYAN << DataGenerator::generate_jwt_mock(payload) << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 3 && tokens[2] == "format") {
            std::string raw = (tokens.size() >= 4) ? tokens[3] : "{\"a\":1,\"b\":[1,2,3]}";
            std::cout << DataGenerator::format_json(raw) << "\n";
            return true;
        }
        std::cerr << color::YELLOW << "Uso: tb data [fake|fake-ip|jwt|format]" << color::RESET << "\n";
        return false;
    }

    if (action == "net") {
        if (tokens.size() >= 3 && tokens[2] == "ip") {
            std::cout << color::CYAN << "Información de red: " << color::YELLOW << NetworkTools::get_ip_info() << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 5 && tokens[2] == "scan") {
            std::string host = tokens[3];
            try {
                int port = std::stoi(tokens[4]);
                bool open = NetworkTools::scan_port(host, port);
                std::cout << "Puerto " << port << " en " << host << ": " << (open ? color::GREEN : color::RED)
                          << (open ? "[ABIERTO]" : "[CERRADO/FILTRADO]") << color::RESET << "\n";
            } catch (...) {
                std::cerr << color::RED << "Error: puerto inválido." << color::RESET << "\n";
            }
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "benchmark") {
            std::string url = tokens[3];
            int n = 50;
            if (tokens.size() >= 5) { try { n = std::stoi(tokens[4]); } catch (...) {} }
            NetworkTools::http_benchmark(url, n);
            return true;
        }
        std::cerr << color::YELLOW << "Uso: tb net [ip|scan <host> <puerto>|benchmark <url> <n>]" << color::RESET << "\n";
        return false;
    }

    if (action == "sys") {
        SystemTools::print_system_info();
        return true;
    }

    // ==========================================
    // 7. tb mod list / search / install / run / load / create / pack
    // ==========================================
    if (action == "mod") {
        auto& lua_engine = mod_engine();
        if (tokens.size() >= 3 && (tokens[2] == "search" || tokens[2] == "repo")) {
            ModMarketplace::show_remote_mods();
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "install") {
            std::string target_mod = tokens[3];
            bool auto_confirm = (tokens.size() >= 5 && (tokens[4] == "--yes" || tokens[4] == "-y"));
            return ModMarketplace::install_remote_mod(target_mod, auto_confirm);
        }
        if (tokens.size() >= 3 && tokens[2] == "list") {
            if (tokens.size() >= 4 && tokens[3] == "--remote") {
                ModMarketplace::show_remote_mods();
                return true;
            }
            auto mods = lua_engine.list_mods();
            std::cout << color::CYAN << color::BOLD << "=== MODS DE LUA INSTALADOS EN LOCAL (" << mods.size() << ") ===" << color::RESET << "\n";
            for (const auto& m : mods) {
                std::cout << "  - " << color::YELLOW << m << color::RESET << "\n";
            }
            std::cout << "\nPara buscar e instalar mods desde GitHub ejecuta: " << color::CYAN << "tb mod search" << color::RESET << "\n";
            std::cout << "Para proteger un mod en disco: " << color::CYAN << "tb mod pack mods/mi_mod.lua" << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "run") {
            return lua_engine.run_file(tokens[3]);
        }
        if (tokens.size() >= 4 && tokens[2] == "pack") {
            std::string out = (tokens.size() >= 5) ? tokens[4] : "";
            return LuaModEngine::pack_mod(tokens[3], out);
        }
        if (tokens.size() >= 3 && tokens[2] == "load") {
            int loaded = lua_engine.load_all_mods();
            std::cout << color::GREEN << "✓ Total de Mods ejecutados: " << loaded << color::RESET << "\n";
            return true;
        }
        if (tokens.size() >= 4 && tokens[2] == "create") {
            bool ok = LuaModEngine::create_mod_template(tokens[3]);
            if (ok) std::cout << color::GREEN << "✓ Plantilla de Mod de Lua creada: mods/" << tokens[3] << ".lua" << color::RESET << "\n";
            return ok;
        }
        std::cerr << color::YELLOW << "Uso: tb mod [search|install|list|run|load|create|pack]" << color::RESET << "\n";
        return false;
    }


    // ==========================================
    // 8. tb pack / tb bundle  y  tb boot / tb launch
    // ==========================================
    if (action == "pack" || action == "bundle") {
        PackOptions opts;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (tokens[i] == "--name" && i + 1 < tokens.size()) opts.bundle_name = tokens[++i];
            else if (tokens[i] == "--key" && i + 1 < tokens.size()) opts.release_key = tokens[++i];
            else if (tokens[i] == "--layers" && i + 1 < tokens.size()) {
                try { opts.mv_layers = std::stoi(tokens[++i]); } catch (...) {}
            }
            else if (tokens[i] == "--skip-runtimes") opts.skip_runtimes = true;
            else if (!tokens[i].empty() && tokens[i][0] != '-') opts.dist_path = tokens[i];
        }
        return Packager::build_release_package(opts);
    }

    if (action == "boot" || action == "launch") {
        std::string dir = (tokens.size() >= 3) ? tokens[2] : "modules";
        return boot_release(dir);
    }


    if (action == "print") {
        std::cout << color::GREEN;
        for (size_t i = 2; i < tokens.size(); ++i) std::cout << tokens[i] << " ";
        std::cout << color::RESET << "\n";
        return true;
    }

    if (action == "uuid") {
        std::cout << color::CYAN << "UUID: " << color::BOLD << Utils::generate_uuid() << color::RESET << "\n";
        return true;
    }

    if (action == "run" || action == "exec") {
        if (tokens.size() < 3) return false;
        std::string target_file = tokens[2];
        if (target_file.rfind(".tb.bin") != std::string::npos) {
            std::cout << color::CYAN << "[+] Decodificando y Ejecutando Binario .tb.bin en memoria..." << color::RESET << "\n";
            std::string release_key = TBBinaryCompiler::resolve_release_key_for(target_file);
            std::string script_text = TBBinaryCompiler::decode_binary(target_file, release_key);
            if (script_text.empty()) return false;
            return execute_script(script_text);
        }
        return execute_file(target_file);
    }

    // Mods can register new engine commands at runtime: tb <modcmd> <args>
    // Installed mods are loaded on demand (once) so their commands and hooks
    // extend the engine.
    auto& eng = mod_engine();
    eng.preload_mods("mods");
    if (eng.has_command(action)) {
        std::vector<std::string> args(tokens.begin() + 2, tokens.end());
        return eng.dispatch_command(action, args);
    }

    std::cerr << color::RED << "Comando DSL Namek desconocido: " << action << color::RESET << "\n";
    return false;
}


bool SyntaxEngine::execute_script(const std::string& script_content) {
    std::stringstream ss(script_content);
    std::string line;
    bool success = true;
    while (std::getline(ss, line)) {
        if (!execute_line(line)) success = false;
    }
    return success;
}

bool SyntaxEngine::execute_file(const std::string& filepath) {
    if (!Utils::file_exists(filepath)) return false;
    return execute_script(Utils::read_file(filepath));
}

bool SyntaxEngine::boot_release(const std::string& modules_dir) {
    std::cout << color::CYAN << color::BOLD << "==========================================================" << color::RESET << "\n";
    std::cout << color::CYAN << color::BOLD << "  NAMEK RELEASE BOOT — CARGANDO MODULOS BINARIOS" << color::RESET << "\n";
    std::cout << color::CYAN << color::BOLD << "==========================================================" << color::RESET << "\n\n";

    std::string manifest_path = modules_dir + "/release_manifest.json";
    if (!Utils::file_exists(manifest_path)) manifest_path = modules_dir + "/../release_manifest.json";
    if (!Utils::file_exists(manifest_path)) {
        std::cerr << color::RED << "Error: No se encontró release_manifest.json en '" << modules_dir << "'." << color::RESET << "\n";
        return false;
    }

    std::string release_key = TBBinaryCompiler::resolve_release_key_for(manifest_path);

    DIR* dir = opendir(modules_dir.c_str());
    if (!dir) {
        std::cerr << color::RED << "Error: No se pudo abrir el directorio de módulos '" << modules_dir << "'." << color::RESET << "\n";
        return false;
    }

    std::vector<std::string> modules;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string fname = ent->d_name;
        if (fname.rfind(".tb.bin") != std::string::npos) {
            modules.push_back(modules_dir + "/" + fname);
        }
    }
    closedir(dir);

    if (modules.empty()) {
        std::cerr << color::RED << "Error: No hay archivos .tb.bin en '" << modules_dir << "'." << color::RESET << "\n";
        return false;
    }

    // Ejecución en orden: core -> toolbox -> mv_engine
    auto priority = [](const std::string& path) -> int {
        if (path.find("core") != std::string::npos) return 0;
        if (path.find("toolbox") != std::string::npos) return 1;
        if (path.find("mv") != std::string::npos) return 2;
        return 3;
    };
    std::sort(modules.begin(), modules.end(), [&](const std::string& a, const std::string& b) {
        int pa = priority(a), pb = priority(b);
        return pa != pb ? pa < pb : a < b;
    });

    bool all_ok = true;
    for (const auto& path : modules) {
        std::cout << color::YELLOW << "[+] Cargando módulo binario: " << path << color::RESET << "\n";
        std::string script = TBBinaryCompiler::decode_binary(path, release_key);
        if (script.empty()) {
            all_ok = false;
            continue;
        }
        if (!execute_script(script)) all_ok = false;
        std::cout << "\n";
    }

    if (all_ok) {
        std::cout << color::GREEN << color::BOLD << "✓ BOOT COMPLETO: TODOS LOS MODULOS BINARIOS EJECUTADOS CORRECTAMENTE." << color::RESET << "\n";
    } else {
        std::cout << color::RED << color::BOLD << "✗ BOOT CON ERRORES: ALGUNOS MODULOS BINARIOS FALLARON." << color::RESET << "\n";
    }
    return all_ok;
}

} // namespace namek
