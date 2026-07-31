#ifndef NAMEK_LUA_H
#define NAMEK_LUA_H

#include <string>
#include <vector>
#include <unordered_map>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace namek {

// Lua mod engine with a hardened sandbox. Mods may call the engine API to
// modify engine behavior (config, hooks, custom commands) but cannot touch
// files or reach the engine code / Lua internals.
class LuaModEngine {
private:
    lua_State* L;
    std::string db_filepath;

    // Runtime registries: mod-registered commands and event hooks.
    // Keys store Lua references (luaL_ref) owned by this instance.
    std::unordered_map<std::string, int> mod_commands;
    std::unordered_map<std::string, int> hooks;
    bool mods_preloaded = false;

    void register_namek_api();

public:
    // Active engine instance (used by the C API callbacks). Public by design.
    static LuaModEngine* s_active;
    static std::unordered_map<std::string, std::string> s_config;

    LuaModEngine(const std::string& db_path = "namek_db.json");
    ~LuaModEngine();

    bool run_script(const std::string& lua_code);
    bool run_file(const std::string& filepath);
    std::vector<std::string> list_mods(const std::string& mods_dir = "mods");
    int load_all_mods(const std::string& mods_dir = "mods");

    // Silently loads every mod in mods_dir (used at engine startup so mods can
    // register commands/hooks that persist across tb invocations).
    int preload_mods(const std::string& mods_dir = "mods");

    static bool create_mod_template(const std::string& mod_name, const std::string& mods_dir = "mods");

    // Packs a Lua mod into an encrypted .tb.bin (ChaCha20) so its code is
    // not readable on disk. Loaded transparently by run_file.
    static bool pack_mod(const std::string& lua_file, const std::string& out_bin = "");

    // Fires a mod-registered hook for the given event (no-op if unregistered).
    bool emit(const std::string& event, const std::string& payload = "");

    // True if a mod registered a handler for this command name.
    bool has_command(const std::string& cmd) const;

    // Registers (or replaces) a mod command / event hook handler.
    // Takes ownership of the Lua reference.
    bool register_command(const std::string& cmd, int lua_ref);
    bool register_hook(const std::string& event, int lua_ref);

    // Dispatches a mod-registered command. Returns false if unhandled.
    bool dispatch_command(const std::string& cmd, const std::vector<std::string>& args);

    // Config surface: lets mods modify engine behavior at runtime.
    static std::string get_config(const std::string& key, const std::string& def = "");
    static void set_config(const std::string& key, const std::string& value);
};

} // namespace namek

#endif // NAMEK_LUA_H
