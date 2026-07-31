#include "namek_mod_marketplace.h"
#include "namek.h"
#include "namek_toolbox_suite.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>

namespace namek {

std::string ModMarketplace::default_registry_url =
    "https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/registry.json";

// Minimal HTTP GET that captures the response body (curl via popen).
static std::string http_get(const std::string& url) {
    std::string cmd = "curl -s --max-time 20 -L \"" + url + "\"";
    std::string out;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) out += buf;
    pclose(fp);
    return out;
}

// Splits a JSON array into its top-level {...} objects (string-aware).
static std::vector<std::string> split_json_objects(const std::string& s) {
    std::vector<std::string> objs;
    size_t i = 0;
    while (i < s.size()) {
        size_t start = s.find('{', i);
        if (start == std::string::npos) break;
        int depth = 0;
        bool in_string = false;
        size_t j = start;
        for (; j < s.size(); ++j) {
            char c = s[j];
            if (c == '"') in_string = !in_string;
            else if (!in_string && c == '{') depth++;
            else if (!in_string && c == '}') {
                depth--;
                if (depth == 0) break;
            }
        }
        objs.push_back(s.substr(start, j - start + 1));
        i = j + 1;
    }
    return objs;
}

// Extracts "key":"value" string pairs from a JSON object (escape-aware).
static std::vector<std::pair<std::string, std::string>> extract_json_fields(const std::string& obj) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t i = 0;
    while (i < obj.size()) {
        size_t ks = obj.find('"', i);
        if (ks == std::string::npos) break;
        size_t ke = obj.find('"', ks + 1);
        if (ke == std::string::npos) break;
        std::string key = obj.substr(ks + 1, ke - ks - 1);

        size_t colon = obj.find(':', ke);
        if (colon == std::string::npos) break;
        size_t vs = obj.find('"', colon + 1);
        if (vs == std::string::npos) break;

        size_t ve = vs + 1;
        std::string val;
        while (ve < obj.size()) {
            char c = obj[ve];
            if (c == '\\' && ve + 1 < obj.size()) { val += obj[ve + 1]; ve += 2; continue; }
            if (c == '"') break;
            val += c;
            ve++;
        }
        out.emplace_back(key, val);
        i = ve + 1;
    }
    return out;
}

// Built-in offline fallback catalog (matches the published namek repo mods).
static std::vector<RemoteModInfo> builtin_fallback() {
    return {
        {"security_sentinel", "2.1.0",
         "Monitor de seguridad del motor: sesión auditada y comando 'lockdown'.",
         "Namek Security Lab",
         "https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/mods/security_sentinel.lua",
         "1b2cffe2871344d6a6707822fda33b080713865d30f601684eef8140b0ffa201"},
        {"database_replicator", "1.4.0",
         "Replicación y respaldo de NamekDB con SHA-256.",
         "Namek Core Team",
         "https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/mods/database_replicator.lua",
         "9e96e736e4c21fd0a0d5cdcba006517c8bba4226db6df53bfe133bf802aed12d"},
        {"api_benchmarker_plus", "3.0.0",
         "Benchmarks sintéticos del motor usando el reloj del sandbox.",
         "DevOps Tools Inc",
         "https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/mods/api_benchmarker_plus.lua",
         "1e3e2f48a1f92ec3d528a535a3d2853f9f65fdedfea7b8372ad227f5f70e71c5"},
        {"engine_booster", "1.0.0",
         "Demo open-close: hooks on_command, comandos propios y config runtime.",
         "Namek Community",
         "https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/mods/engine_booster.lua",
         "bf56ca5c0e9ac5fb515827a2456be99467dd1aa1d7bc6554ae924a203556b00b"},
    };
}

std::vector<RemoteModInfo> ModMarketplace::fetch_remote_registry(const std::string& registry_url) {
    std::string url = registry_url.empty() ? default_registry_url : registry_url;
    std::string body = http_get(url);

    std::vector<RemoteModInfo> registry;
    if (body.empty() || body.find('{') == std::string::npos) {
        std::cerr << color::YELLOW << "[!] No se pudo descargar el catálogo remoto (offline?). "
                  << "Usando catálogo offline integrado." << color::RESET << "\n";
        return builtin_fallback();
    }

    for (const auto& obj : split_json_objects(body)) {
        auto fields = extract_json_fields(obj);
        RemoteModInfo m;
        bool ok = false;
        for (const auto& [k, v] : fields) {
            if (k == "name") m.name = v;
            else if (k == "version") m.version = v;
            else if (k == "description") m.description = v;
            else if (k == "author") m.author = v;
            else if (k == "download_url") m.download_url = v;
            else if (k == "sha256") m.checksum = v;
            else if (k == "checksum") m.checksum = v;
        }
        if (!m.name.empty()) {
            if (m.checksum.empty() && m.name == "security_sentinel") m.checksum = "sha256_";
            registry.push_back(std::move(m));
        }
    }
    return registry;
}

void ModMarketplace::show_remote_mods() {
    auto mods = fetch_remote_registry();
    std::cout << color::CYAN << color::BOLD << "==========================================================" << color::RESET << "\n";
    std::cout << color::CYAN << color::BOLD << "  REPOSITORIO REMOTO DE MODS DE NAMEK" << color::RESET << "\n";
    std::cout << color::CYAN << color::BOLD << "==========================================================" << color::RESET << "\n\n";

    std::cout << "Se encontraron " << color::YELLOW << mods.size() << color::RESET
              << " Mods verificados disponibles:\n\n";

    for (size_t i = 0; i < mods.size(); ++i) {
        const auto& m = mods[i];
        std::cout << "  " << color::YELLOW << (i + 1) << ") " << color::GREEN << color::BOLD << m.name << color::RESET
                  << color::DIM << " (v" << m.version << ") - Autor: " << m.author << color::RESET << "\n";
        std::cout << "     " << m.description << "\n\n";
    }
    std::cout << "Para instalar directamente ejecuta: " << color::CYAN << "tb mod install <nombre_del_mod>" << color::RESET << "\n";
}

bool ModMarketplace::install_remote_mod(const std::string& mod_name, bool auto_confirm) {
    auto registry = fetch_remote_registry();
    const RemoteModInfo* target = nullptr;
    for (const auto& m : registry) {
        if (m.name == mod_name) {
            target = &m;
            break;
        }
    }

    if (!target) {
        std::cerr << color::RED << "Error: Mod '" << mod_name << "' no fue encontrado en el repositorio remoto." << color::RESET << "\n";
        std::cerr << "Ejecuta 'tb mod search' para ver los mods disponibles.\n";
        return false;
    }

    std::cout << color::CYAN << color::BOLD << "[+] DETALLES DEL MOD:" << color::RESET << "\n";
    std::cout << "  Nombre: " << color::GREEN << target->name << color::RESET << "\n";
    std::cout << "  Versión: " << target->version << "\n";
    std::cout << "  Autor: " << target->author << "\n";
    std::cout << "  Descripción: " << target->description << "\n";
    std::cout << "  SHA-256: " << (target->checksum.empty() ? "(no verificado)" : target->checksum) << "\n\n";

    if (!auto_confirm) {
        bool confirm = CLIApp::confirm("¿Deseas descargar e instalar automáticamente este Mod en la carpeta 'mods/'?");
        if (!confirm) {
            std::cout << color::YELLOW << "Instalación cancelada por el usuario." << color::RESET << "\n";
            return false;
        }
    }

    system("mkdir -p mods");
    std::string dest_path = "mods/" + target->name + ".lua";
    std::string tmp_path = dest_path + ".tmp";

    std::cout << color::CYAN << "[+] Descargando (" << target->download_url << ")..." << color::RESET << "\n";
    std::string cmd = "curl -s --max-time 30 -L \"" + target->download_url + "\" -o \"" + tmp_path + "\"";
    system(cmd.c_str());

    if (!Utils::file_exists(tmp_path)) {
        std::cerr << color::RED << "Error: no se pudo descargar el Mod (offline?)." << color::RESET << "\n";
        return false;
    }

    // Integrity check: SHA-256 must match the published registry.
    if (!target->checksum.empty()) {
        std::string expected = Utils::to_lower(target->checksum);
        if (expected.rfind("sha256_", 0) == 0) expected = expected.substr(7);
        std::string actual = Utils::to_lower(CryptoTools::sha256(Utils::read_file(tmp_path)));
        if (expected != actual) {
            std::cerr << color::RED << "✗ FALLO DE INTEGRIDAD: SHA-256 no coincide." << color::RESET << "\n";
            std::cerr << "  esperado: " << expected << "\n";
            std::cerr << "  actual:   " << actual << "\n";
            std::cerr << color::YELLOW << "  El archivo se descartó (posible manipulación o versión incorrecta)." << color::RESET << "\n";
            std::remove(tmp_path.c_str());
            return false;
        }
        std::cout << color::GREEN << "✓ Integridad verificada (SHA-256)." << color::RESET << "\n";
    }

    if (rename(tmp_path.c_str(), dest_path.c_str()) != 0) {
        std::cerr << color::RED << "Error: no se pudo mover el Mod a '" << dest_path << "'." << color::RESET << "\n";
        std::remove(tmp_path.c_str());
        return false;
    }

    for (int i = 1; i <= 10; ++i) {
        CLIApp::show_progress("Instalando Mod en mods/", i, 10);
        usleep(20000);
    }

    std::cout << color::GREEN << color::BOLD << "\n✓ ¡Mod '" << target->name << "' instalado exitosamente en '" << dest_path << "'!" << color::RESET << "\n";
    std::cout << "Para ejecutarlo: " << color::CYAN << "tb mod run " << dest_path << color::RESET << "\n";
    return true;
}

} // namespace namek
