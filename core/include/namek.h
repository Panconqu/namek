#ifndef NAMEK_H
#define NAMEK_H

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <functional>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

namespace namek {

// ==========================================
// 1. CONSOLE COLORS & TERMINAL FORMATTING
// ==========================================
namespace color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string DIM     = "\033[2m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN    = "\033[36m";
    const std::string WHITE   = "\033[37m";

    const std::string BG_RED   = "\033[41m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_BLUE  = "\033[44m";
}

// ==========================================
// 2. NAMEK UTILS
// ==========================================
class Utils {
public:
    static std::string trim(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string to_lower(const std::string& str);
    static std::string to_upper(const std::string& str);
    static std::string read_file(const std::string& filepath);
    static bool write_file(const std::string& filepath, const std::string& content);
    static bool file_exists(const std::string& filepath);
    static std::string generate_uuid();
    static std::string escape_json(const std::string& input);
};

// ==========================================
// 3. NAMEK NOSQL DATABASE ENGINE (NamekDB)
// ==========================================
struct Document {
    std::string id;
    std::unordered_map<std::string, std::string> fields;

    std::string to_json() const;
    static Document from_json(const std::string& json_str);
};

class NamekDB {
private:
    std::string db_filepath;
    std::unordered_map<std::string, std::string> key_value_store;
    std::unordered_map<std::string, std::unordered_map<std::string, Document>> collections;

public:
    NamekDB(const std::string& filepath = "namek_db.json");
    ~NamekDB();

    bool load();
    bool save();

    // Key-Value API
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key, const std::string& default_val = "");
    bool has(const std::string& key);
    bool del(const std::string& key);
    std::vector<std::string> keys();

    // NoSQL Document Store API
    std::string insert(const std::string& collection_name, const std::unordered_map<std::string, std::string>& fields);
    bool update(const std::string& collection_name, const std::string& doc_id, const std::unordered_map<std::string, std::string>& fields);
    bool remove(const std::string& collection_name, const std::string& doc_id);
    Document get_doc(const std::string& collection_name, const std::string& doc_id);
    std::vector<Document> find(const std::string& collection_name, const std::string& field_name, const std::string& expected_value);
    std::vector<Document> find_all(const std::string& collection_name);
    size_t count(const std::string& collection_name);
    std::vector<std::string> list_collections();

    std::string export_json_string();
    bool import_json_string(const std::string& json_data);
};

// ==========================================
// 4. NAMEK CLI BUILDER ENGINE
// ==========================================
struct Option {
    std::string name;
    std::string flag; // e.g. "-v" or "--verbose"
    std::string description;
    std::string default_value;
    bool required;
    bool is_boolean;
};

class Command {
public:
    std::string name;
    std::string description;
    std::unordered_map<std::string, Option> options;
    std::function<void(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args)> handler;

    Command() = default;
    Command(const std::string& n, const std::string& desc) : name(n), description(desc) {}

    Command& add_option(const std::string& flag, const std::string& desc, const std::string& default_val = "", bool required = false, bool is_bool = false) {
        Option opt{flag, flag, desc, default_val, required, is_bool};
        options[flag] = opt;
        return *this;
    }

    Command& set_handler(std::function<void(const std::unordered_map<std::string, std::string>& flags, const std::vector<std::string>& args)> h) {
        handler = h;
        return *this;
    }
};

class CLIApp {
private:
    std::string app_name;
    std::string app_version;
    std::string app_description;
    std::unordered_map<std::string, Command> commands;
    std::unordered_map<std::string, Option> global_options;

public:
    CLIApp(const std::string& name, const std::string& version, const std::string& desc = "");

    CLIApp& add_global_option(const std::string& flag, const std::string& desc, bool is_bool = true);
    CLIApp& add_command(const Command& cmd);
    void print_banner();
    void print_help();
    int run(int argc, char* argv[]);

    // Interactive helper prompts
    static std::string prompt(const std::string& question, const std::string& default_val = "");
    static bool confirm(const std::string& question, bool default_yes = true);
    static int select(const std::string& question, const std::vector<std::string>& choices);
    static void show_progress(const std::string& label, int current, int total, int width = 30);
};

} // namespace namek

#endif // NAMEK_H
