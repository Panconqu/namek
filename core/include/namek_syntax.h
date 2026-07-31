#ifndef NAMEK_SYNTAX_H
#define NAMEK_SYNTAX_H

#include "namek.h"
#include "namek_obfuscator.h"
#include "namek_lua.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace namek {

class SyntaxEngine {
private:
    NamekDB db;
    std::unique_ptr<LuaModEngine> lua_engine;

    // Lazily creates the persistent Lua mod engine (survives across DSL lines).
    LuaModEngine& mod_engine();

public:
    SyntaxEngine(const std::string& db_path = "namek_db.json");

    // Execute single line statement in Namek Syntax DSL (e.g. "tb set api key 'sk-12345'")
    bool execute_line(const std::string& line);

    // Execute full script containing multiple Namek DSL statements
    bool execute_script(const std::string& script_content);

    // Execute script from file path (.tb file)
    bool execute_file(const std::string& filepath);

    // Boot the release bundle: reads release_manifest.json, validates and
    // executes every .tb.bin module (core -> toolbox -> mv_engine)
    bool boot_release(const std::string& modules_dir = "modules");

    // Parse tokens respecting quotes
    static std::vector<std::string> parse_tokens(const std::string& line);
};

} // namespace namek

#endif // NAMEK_SYNTAX_H
