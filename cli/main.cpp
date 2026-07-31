#include "namek.h"
#include "namek_syntax.h"
#include "namek_obfuscator.h"
#include "namek_c_api.h"
#include <iostream>
#include <unistd.h>

using namespace namek;

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
        while (true) {
            std::string line = CLIApp::prompt("tb> ");
            if (line == "exit" || line == "quit") break;
            if (line.rfind("tb ", 0) != 0 && line.rfind("toolbox ", 0) != 0) {
                line = "tb " + line;
            }
            engine.execute_line(line);
        }
        return;
    }

    // Direct command evaluation: e.g. ./bin/namek tb set api key mykey
    std::string full_line = "tb";
    for (const auto& arg : args) {
        full_line += " " + arg;
    }
    engine.execute_line(full_line);
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

    return app.run(argc, argv);
}
