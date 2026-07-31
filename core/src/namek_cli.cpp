#include "namek.h"
#include <iostream>
#include <iomanip>

namespace namek {

CLIApp::CLIApp(const std::string& name, const std::string& version, const std::string& desc)
    : app_name(name), app_version(version), app_description(desc) {
    add_global_option("--help", "Muestra el menú de ayuda", true);
    add_global_option("-h", "Muestra el menú de ayuda", true);
    add_global_option("--version", "Muestra la versión de Namek", true);
    add_global_option("-v", "Muestra la versión de Namek", true);
}

CLIApp& CLIApp::add_global_option(const std::string& flag, const std::string& desc, bool is_bool) {
    global_options[flag] = Option{flag, flag, desc, "", false, is_bool};
    return *this;
}

CLIApp& CLIApp::add_command(const Command& cmd) {
    commands[cmd.name] = cmd;
    return *this;
}

void CLIApp::print_banner() {
    std::cout << color::CYAN << color::BOLD << R"(
  N   N   AAA   M   M   EEEEE  K  K
  NN  N  A   A  MM MM  E      K K
  N N N  AAAAA  M M M  EEE    KK
  N  NN  A   A  M   M  E      K K
  N   N  A   A  M   M  EEEEE  K  K
)" << color::RESET << color::DIM << "Namek CLI Developer Framework & Toolbox v" << app_version << "\n" << color::RESET << "\n";
}

void CLIApp::print_help() {
    print_banner();
    if (!app_description.empty()) {
        std::cout << color::WHITE << app_description << color::RESET << "\n\n";
    }
    std::cout << color::YELLOW << color::BOLD << "USO:" << color::RESET << "\n";
    std::cout << "  " << app_name << " <comando> [opciones] [argumentos]\n\n";

    std::cout << color::YELLOW << color::BOLD << "COMANDOS DISPONIBLES:" << color::RESET << "\n";
    for (const auto& [name, cmd] : commands) {
        std::cout << "  " << color::GREEN << std::left << std::setw(15) << name << color::RESET << " " << cmd.description << "\n";
    }
    std::cout << "\n" << color::YELLOW << color::BOLD << "OPCIONES GLOBALES:" << color::RESET << "\n";
    for (const auto& [flag, opt] : global_options) {
        std::cout << "  " << color::CYAN << std::left << std::setw(15) << flag << color::RESET << " " << opt.description << "\n";
    }
    std::cout << "\nUsa '" << app_name << " <comando> --help' para más detalles de cada comando.\n";
}

int CLIApp::run(int argc, char* argv[]) {
    if (argc <= 1) {
        print_help();
        return 0;
    }

    std::string arg1 = argv[1];
    if (arg1 == "--help" || arg1 == "-h") {
        print_help();
        return 0;
    }
    if (arg1 == "--version" || arg1 == "-v") {
        std::cout << app_name << " v" << app_version << "\n";
        return 0;
    }

    auto cmd_it = commands.find(arg1);
    if (cmd_it == commands.end()) {
        std::cerr << color::RED << "Error: Comando desconocido '" << arg1 << "'." << color::RESET << "\n";
        std::cerr << "Ejecuta '" << app_name << " --help' para listar los comandos válidos.\n";
        return 1;
    }

    Command& cmd = cmd_it->second;
    std::unordered_map<std::string, std::string> flags;
    std::vector<std::string> args;

    for (int i = 2; i < argc; ++i) {
        std::string token = argv[i];
        if (token.rfind("-", 0) == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                flags[token] = argv[i + 1];
                i++;
            } else {
                flags[token] = "true";
            }
        } else {
            args.push_back(token);
        }
    }

    if (flags.count("--help") || flags.count("-h")) {
        std::cout << color::YELLOW << "Comando: " << color::GREEN << cmd.name << color::RESET << "\n";
        std::cout << cmd.description << "\n\nOpciones:\n";
        for (const auto& [flag, opt] : cmd.options) {
            std::cout << "  " << color::CYAN << std::left << std::setw(15) << flag << color::RESET << " " << opt.description;
            if (!opt.default_value.empty()) {
                std::cout << " (Predeterminado: " << opt.default_value << ")";
            }
            std::cout << "\n";
        }
        return 0;
    }

    try {
        if (cmd.handler) {
            cmd.handler(flags, args);
        }
    } catch (const std::exception& e) {
        std::cerr << color::RED << "Error ejecutando " << cmd.name << ": " << e.what() << color::RESET << "\n";
        return 1;
    }
    return 0;
}

std::string CLIApp::prompt(const std::string& question, const std::string& default_val) {
    std::cout << color::CYAN << "? " << color::BOLD << question << color::RESET;
    if (!default_val.empty()) {
        std::cout << color::DIM << " (" << default_val << ")" << color::RESET;
    }
    std::cout << ": ";

    std::string response;
    std::getline(std::cin, response);
    response = Utils::trim(response);
    if (response.empty()) return default_val;
    return response;
}

bool CLIApp::confirm(const std::string& question, bool default_yes) {
    std::string prompt_str = question + (default_yes ? " [Y/n]" : " [y/N]");
    std::string res = prompt(prompt_str);
    if (res.empty()) return default_yes;
    res = Utils::to_lower(res);
    return (res == "y" || res == "yes" || res == "s" || res == "si");
}

int CLIApp::select(const std::string& question, const std::vector<std::string>& choices) {
    std::cout << color::CYAN << "? " << color::BOLD << question << color::RESET << "\n";
    for (size_t i = 0; i < choices.size(); ++i) {
        std::cout << "  " << color::YELLOW << (i + 1) << ") " << color::RESET << choices[i] << "\n";
    }
    std::string choice_str = prompt("Selecciona una opción [1-" + std::to_string(choices.size()) + "]");
    try {
        int idx = std::stoi(choice_str) - 1;
        if (idx >= 0 && idx < static_cast<int>(choices.size())) return idx;
    } catch (...) {}
    return 0;
}

void CLIApp::show_progress(const std::string& label, int current, int total, int width) {
    float ratio = (float)current / total;
    int pos = width * ratio;
    std::cout << "\r" << color::GREEN << label << " [" << color::RESET;
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << color::GREEN << "] " << int(ratio * 100.0) << "%" << color::RESET << std::flush;
    if (current >= total) std::cout << "\n";
}

} // namespace namek
