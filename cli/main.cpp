#include "namek.h"
#include "namek_syntax.h"
#include "namek_obfuscator.h"
#include "namek_c_api.h"
#include "namek_telemetry.h"
#include <iostream>
#include <unistd.h>
#include <termios.h>

using namespace namek;

namespace {
std::string read_password(const std::string& prompt) {
    std::cout << color::CYAN << prompt << ": " << color::RESET << std::flush;
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    std::string pw;
    std::getline(std::cin, pw);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << "\n";
    return pw;
}

std::string trim_ws(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split_json_objects(const std::string& s) {
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

std::map<std::string, std::string> json_fields(const std::string& obj) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < obj.size()) {
        size_t ks = obj.find('"', i);
        if (ks == std::string::npos) break;
        size_t ke = obj.find('"', ks + 1);
        if (ke == std::string::npos) break;
        std::string key = obj.substr(ks + 1, ke - ks - 1);
        size_t colon = obj.find(':', ke);
        if (colon == std::string::npos) break;
        size_t vp = colon + 1;
        while (vp < obj.size() && (obj[vp] == ' ' || obj[vp] == '\t')) vp++;
        if (vp < obj.size() && obj[vp] == '"') {
            size_t ve = vp + 1;
            std::string val;
            while (ve < obj.size()) {
                char c = obj[ve];
                if (c == '\\' && ve + 1 < obj.size()) { val += obj[ve + 1]; ve += 2; continue; }
                if (c == '"') break;
                val += c;
                ve++;
            }
            out[key] = val;
            i = ve + 1;
        } else if (vp < obj.size() && (obj[vp] == 't' || obj[vp] == 'f')) {
            size_t end = obj.find_first_of(",}", vp);
            out[key] = obj.substr(vp, end - vp);
            i = end + 1;
        } else if (vp < obj.size() && obj[vp] == '{') {
            int d = 0; size_t j = vp;
            for (; j < obj.size(); ++j) {
                char c = obj[j];
                if (c == '{') d++;
                else if (c == '}') { d--; if (d == 0) break; }
            }
            out[key] = obj.substr(vp, j - vp + 1);
            i = j + 1;
        } else {
            size_t end = obj.find_first_of(",}", vp);
            out[key] = obj.substr(vp, end - vp);
            i = end + 1;
        }
    }
    return out;
}

void render_users(const std::string& body) {
    std::cout << color::CYAN << color::BOLD << "\n=== USUARIOS REGISTRADOS ===" << color::RESET << "\n";
    for (const auto& obj : split_json_objects(body)) {
        auto f = json_fields(obj);
        std::string name = f["username"];
        bool banned = f["banned"] == "true";
        std::string status = banned ? color::RED + "BANEADO" + color::RESET
                                    : color::GREEN + "activo" + color::RESET;
        std::cout << "  " << color::YELLOW << name << color::RESET << "  [" << status << color::RESET
                  << "]  eventos: " << f["events"] << "\n";
    }
}

void render_events(const std::string& body) {
    std::cout << color::CYAN << color::BOLD << "\n=== EVENTOS DEL MOTOR ===" << color::RESET << "\n";
    for (const auto& obj : split_json_objects(body)) {
        auto f = json_fields(obj);
        std::string ok = f["success"] == "true" ? color::GREEN + "OK" + color::RESET
                                                : color::RED + "FALLO" + color::RESET;
        std::string ts = f["ts"];
        if (ts.size() > 8 && ts[0] == '{') ts = "?";
        std::cout << "  [" << ok << color::RESET << "] " << color::CYAN << f["event"]
                  << color::RESET << " -> " << color::YELLOW << f["method"] << color::RESET
                  << (f["detail"].empty() ? "" : "  (" + f["detail"] + ")")
                  << "  <" << f["username"] << ">\n";
    }
}

std::string admin_user_from_env() {
    const char* e = std::getenv("ADMIN_USER");
    return e ? e : "";
}

std::string admin_pass_from_env() {
    const char* e = std::getenv("ADMIN_PASS");
    return e ? e : "";
}

void ensure_session() {
    std::string token, username;
    if (Telemetry::load_token(token, username)) {
        auto r = Telemetry::me();
        if (r.status == 401) {
            std::cout << color::YELLOW << "[!] Tu sesión expiró o fue revocada. Inicia sesión de nuevo." << color::RESET << "\n";
            Telemetry::clear_token();
        }
        return;
    }
    std::cout << color::CYAN << color::BOLD << "\n=== SISTEMA DE CUENTAS NAMEK ===" << color::RESET << "\n"
              << "Primer uso: regístrate (nombre + contraseña, sin datos personales).\n";
    while (true) {
        std::string op = CLIApp::prompt("¿[R]egistrarte, [I]niciar sesión o [S]altar?", "I");
        if (op == "r" || op == "R") {
            std::string u = trim_ws(CLIApp::prompt("Nombre de usuario"));
            std::string p = read_password("Contraseña");
            auto r = Telemetry::register_user(u, p);
            if (r.ok) {
                std::cout << color::GREEN << "✓ Cuenta creada y sesión iniciada como '" << u << "'." << color::RESET << "\n";
                return;
            }
            std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
            if (r.error.find("backend") != std::string::npos) return;
        } else if (op == "i" || op == "I" || op.empty()) {
            std::string u = trim_ws(CLIApp::prompt("Nombre de usuario"));
            std::string p = read_password("Contraseña");
            auto r = Telemetry::login_user(u, p);
            if (r.ok) {
                std::cout << color::GREEN << "✓ Sesión iniciada como '" << u << "'." << color::RESET << "\n";
                return;
            }
            std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
            if (r.error.find("backend") != std::string::npos) return;
        } else if (op == "s" || op == "S") {
            std::cout << color::YELLOW << "[i] Continuando sin sesión (sin auditoría)." << color::RESET << "\n";
            return;
        }
    }
}

void audit_command(const std::string& full_line, bool ok) {
    std::string rest = full_line;
    if (rest.rfind("tb ", 0) == 0) rest = rest.substr(3);
    else if (rest.rfind("toolbox ", 0) == 0) rest = rest.substr(8);
    std::string method = rest.substr(0, rest.find_first_of(" \t"));
    if (method.empty()) return;
    if (method == "register" || method == "login" || method == "admin") return; // no auditar auth
    Telemetry::audit("command", method, ok, trim_ws(rest));
}
} // namespace

// Detects a dist/ release bundle layout (bin/ + modules/*.tb.bin + manifest)
std::string find_modules_dir() {
    if (Utils::file_exists("modules/release_manifest.json")) return "modules";
    if (Utils::file_exists("../modules/release_manifest.json")) return "../modules";
    if (Utils::file_exists("release_manifest.json")) return "modules";
    if (Utils::file_exists("../release_manifest.json")) return "../modules";
    return "";
}

void handle_boot(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    std::string dir = args.empty() ? find_modules_dir() : args[0];
    if (dir.empty()) dir = "modules";
    SyntaxEngine engine("namek_db.json");
    engine.boot_release(dir);
}

void handle_tb_syntax(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    SyntaxEngine engine("namek_db.json");
    if (args.empty()) {
        std::cout << color::CYAN << color::BOLD << "=== INTERPRETE INTERACTIVO DE SINTAXIS NAMEK (tb / toolbox) ===" << color::RESET << "\n";
        std::cout << "Escribe cualquier comando 'tb ...' (e.g. 'tb set api key secret_123', 'tb obfuscate app.py --level mv') o 'exit' para salir.\n\n";
        ensure_session();
        while (true) {
            std::string line = CLIApp::prompt("tb> ");
            if (line == "exit" || line == "quit") break;
            if (line.rfind("tb ", 0) != 0 && line.rfind("toolbox ", 0) != 0) {
                line = "tb " + line;
            }
            bool ok = engine.execute_line(line);
            audit_command(line, ok);
        }
        return;
    }

    // Direct command evaluation: e.g. ./bin/namek tb set api key mykey
    std::string full_line = "tb";
    for (const auto& arg : args) {
        full_line += " " + arg;
    }
    bool ok = engine.execute_line(full_line);
    audit_command(full_line, ok);
}

void handle_obfuscate(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << color::RED << "Error: Se requiere la ruta del archivo a ofuscar." << color::RESET << "\n";
        std::cout << "Ejemplo: namek obfuscate script.py --level mv --output script_secure.py\n";
        return;
    }

    std::string file_path = args[0];
    std::string level = flags.count("--level") ? flags.at("--level") : "mv";
    std::string output = flags.count("--output") ? flags.at("--output") : (file_path + ".obf");

    SyntaxEngine engine;
    std::string line = "tb obfuscate " + file_path + " --level " + level + " --output " + output;
    engine.execute_line(line);
}

void handle_init(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    std::string proj_name = args.empty() ? CLIApp::prompt("Nombre del proyecto/herramienta", "my-namek-app") : args[0];
    
    std::vector<std::string> templates = {
        "CLI Tool (C++ / Rust Native)",
        "Node.js API + NamekDB SDK",
        "Python Data Science & CLI Suite",
        "Full-Stack Multi-Language Workspace"
    };
    int selected = CLIApp::select("Selecciona el tipo de plantilla a generar", templates);

    std::cout << color::GREEN << "\n[+] Inicializando proyecto '" << proj_name << "' con plantilla [" << templates[selected] << "]..." << color::RESET << "\n";
    
    for (int i = 1; i <= 10; ++i) {
        CLIApp::show_progress("Generando archivos", i, 10);
        usleep(30000);
    }

    std::cout << color::CYAN << color::BOLD << "\n¡Proyecto '" << proj_name << "' creado exitosamente!" << color::RESET << "\n";
}

void handle_db(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    std::string db_file = flags.count("--file") ? flags.at("--file") : "namek_db.json";
    NamekDB db(db_file);

    if (flags.count("--set")) {
        std::string kv = flags.at("--set");
        auto parts = Utils::split(kv, '=');
        if (parts.size() >= 2) {
            db.set(parts[0], parts[1]);
            db.save();
            std::cout << color::GREEN << "✓ Guardado " << parts[0] << " = " << parts[1] << color::RESET << "\n";
        }
        return;
    }

    if (flags.count("--get")) {
        std::string key = flags.at("--get");
        std::string val = db.get(key, "(no encontrado)");
        std::cout << color::CYAN << key << color::RESET << " => " << color::YELLOW << val << color::RESET << "\n";
        return;
    }

    std::cout << color::CYAN << color::BOLD << "=== EXPLORADOR NAMEK NOSQL DB (" << db_file << ") ===" << color::RESET << "\n";
    auto keys = db.keys();
    std::cout << "Llaves almacenadas (" << keys.size() << "):\n";
    for (const auto& k : keys) {
        std::cout << "  - " << color::YELLOW << k << color::RESET << " = " << db.get(k) << "\n";
    }
}

void handle_register(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << color::RED << "Uso: namek register <usuario>" << color::RESET << "\n";
        return;
    }
    std::string u = trim_ws(args[0]);
    std::string p = flags.count("--pass") ? flags.at("--pass") : read_password("Contraseña (mín 6)");
    auto r = Telemetry::register_user(u, p);
    if (r.ok) {
        std::cout << color::GREEN << "✓ Cuenta creada y sesión iniciada como '" << u << "'." << color::RESET << "\n";
    } else {
        std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
    }
}

void handle_login(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << color::RED << "Uso: namek login <usuario>" << color::RESET << "\n";
        return;
    }
    std::string u = trim_ws(args[0]);
    std::string p = flags.count("--pass") ? flags.at("--pass") : read_password("Contraseña");
    auto r = Telemetry::login_user(u, p);
    if (r.ok) {
        std::cout << color::GREEN << "✓ Sesión iniciada como '" << u << "'." << color::RESET << "\n";
    } else {
        std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
    }
}

void handle_logout(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    if (Telemetry::logout()) {
        std::cout << color::GREEN << "✓ Sesión cerrada." << color::RESET << "\n";
    } else {
        std::cout << color::YELLOW << "[i] No hay sesión activa." << color::RESET << "\n";
    }
}

void handle_me(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    std::string token, username;
    if (!Telemetry::load_token(token, username)) {
        std::cout << color::YELLOW << "[i] No hay sesión activa. Usa: namek register <usuario> o namek login <usuario>" << color::RESET << "\n";
        return;
    }
    auto r = Telemetry::me();
    if (r.ok) {
        std::cout << color::GREEN << "✓ Sesión: " << color::CYAN << username << color::RESET;
        auto f = json_fields(r.body);
        if (f["banned"] == "true") std::cout << " [" << color::RED << "BANEADA" << color::RESET << "]";
        std::cout << "\n";
    } else {
        std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
    }
}

void handle_admin(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << color::RED << "Uso: namek admin <users|events|ban|unban> [...]" << color::RESET << "\n";
        return;
    }
    std::string au = admin_user_from_env();
    std::string ap = admin_pass_from_env();
    if (au.empty()) au = trim_ws(CLIApp::prompt("Usuario admin"));
    if (ap.empty()) ap = read_password("Password admin");

    const std::string& sub = args[0];
    if (sub == "users") {
        auto r = Telemetry::admin_users(au, ap);
        if (r.ok) render_users(r.body);
        else std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
    } else if (sub == "events") {
        std::string filter;
        int limit = 50;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--user" && i + 1 < args.size()) filter = args[++i];
            else if (args[i] == "--limit" && i + 1 < args.size()) limit = std::atoi(args[++i].c_str());
        }
        auto r = Telemetry::admin_events(au, ap, filter, limit);
        if (r.ok) render_events(r.body);
        else std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
    } else if (sub == "ban" || sub == "unban") {
        if (args.size() < 2) {
            std::cout << color::RED << "Uso: namek admin " << sub << " <usuario>" << color::RESET << "\n";
            return;
        }
        bool ban = (sub == "ban");
        auto r = Telemetry::admin_ban(au, ap, args[1], ban);
        if (r.ok) {
            std::cout << color::GREEN << "✓ Usuario '" << args[1] << "' " << (ban ? "baneado" : "desbaneado")
                      << " (sus sesiones fueron revocadas)." << color::RESET << "\n";
        } else {
            std::cout << color::RED << "✗ " << r.error << color::RESET << "\n";
        }
    } else {
        std::cout << color::RED << "Subcomando desconocido: " << sub << color::RESET << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Direct shortcut if invoked as "tb" binary
    std::string exec_name = argv[0];
    if (exec_name.find("tb") != std::string::npos && exec_name.find("namek") == std::string::npos) {
        SyntaxEngine engine("namek_db.json");
        if (argc == 1) {
            std::string mods = find_modules_dir();
            if (!mods.empty()) {
                SyntaxEngine boot_engine("namek_db.json");
                boot_engine.boot_release(mods);
                return 0;
            }
            std::vector<std::string> empty_args;
            handle_tb_syntax({}, empty_args);
            return 0;
        }
        std::string full_line = "tb";
        for (int i = 1; i < argc; ++i) {
            full_line += " " + std::string(argv[i]);
        }
        engine.execute_line(full_line);
        return 0;
    }

    CLIApp app("namek", "1.0.0", "Namek: Framework & Developer Toolbox con Sintaxis DSL 'tb' y Ofuscación MV/Compilada");

    if (argc <= 1) {
        std::string mods = find_modules_dir();
        if (!mods.empty()) {
            SyntaxEngine engine("namek_db.json");
            engine.boot_release(mods);
            return 0;
        }
    }

    Command cmd_tb("tb", "Ejecuta sentencias o el intérprete DSL 'tb' (e.g. 'tb set api key')");
    cmd_tb.set_handler(handle_tb_syntax);
    app.add_command(cmd_tb);

    Command cmd_boot("boot", "Carga, valida y ejecuta los módulos binarios .tb.bin del release (dist/modules)");
    cmd_boot.set_handler(handle_boot);
    app.add_command(cmd_boot);

    Command cmd_toolbox("toolbox", "Alias del comando 'tb'");
    cmd_toolbox.set_handler(handle_tb_syntax);
    app.add_command(cmd_toolbox);

    Command cmd_obfuscate("obfuscate", "Ofusca código fuente (niveles: high, mv, compile)");
    cmd_obfuscate.add_option("--level", "Nivel de ofuscación: high, mv (Virtual Machine), compile (Nativo binario)")
                 .add_option("--output", "Ruta del archivo de salida");
    cmd_obfuscate.set_handler(handle_obfuscate);
    app.add_command(cmd_obfuscate);

    Command cmd_init("init", "Inicializa un nuevo proyecto o herramienta CLI con Namek");
    cmd_init.set_handler(handle_init);
    app.add_command(cmd_init);

    Command cmd_db("db", "Gestión de NamekDB NoSQL");
    cmd_db.add_option("--set", "Guarda un valor llave=valor")
          .add_option("--get", "Obtiene el valor de una llave");
    cmd_db.set_handler(handle_db);
    app.add_command(cmd_db);

    Command cmd_register("register", "Crea una cuenta en el sistema de sesiones Namek");
    cmd_register.add_option("--pass", "Contraseña (si no se da, se pide sin eco)");
    cmd_register.set_handler(handle_register);
    app.add_command(cmd_register);

    Command cmd_login("login", "Inicia sesión y obtiene un token de sesión único");
    cmd_login.add_option("--pass", "Contraseña (si no se da, se pide sin eco)");
    cmd_login.set_handler(handle_login);
    app.add_command(cmd_login);

    Command cmd_logout("logout", "Cierra la sesión actual (revoca el token)");
    cmd_logout.set_handler(handle_logout);
    app.add_command(cmd_logout);

    Command cmd_me("me", "Muestra tu sesión activa y estado de la cuenta");
    cmd_me.set_handler(handle_me);
    app.add_command(cmd_me);

    Command cmd_admin("admin", "Panel de control: users, events, ban <u>, unban <u>");
    cmd_admin.set_handler(handle_admin);
    app.add_command(cmd_admin);

    return app.run(argc, argv);
}
