#ifndef NAMEK_MOD_SANDBOX_H
#define NAMEK_MOD_SANDBOX_H

#include <string>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace namek {

// Hardened mod sandbox: mods can call the engine API (open) but cannot touch
// the filesystem, read the engine code, or escape to the real Lua globals
// (closed). Implements a whitelist environment + CPU quota + path policy.
class ModSecuritySandbox {
private:
    static size_t max_instruction_limit;
    static size_t current_instruction_count;

    static void lua_instruction_hook(lua_State* L, lua_Debug* ar);

public:
    // Builds the locked whitelist environment on the Lua state and stores it
    // in the registry. Strips io/os/package/debug/load* from mod reach.
    static void apply_sandbox_policy(lua_State* L);

    // Compiles and runs a chunk inside the locked sandbox environment,
    // replacing its _ENV upvalue (Lua 5.3). Returns false on compile/runtime error.
    static bool run_locked(lua_State* L, const std::string& code,
                           const std::string& chunkname = "=(namek mod)");

    // Path policy: mods may only reach their own files; blocks source dirs,
    // dist bundles, manifests, env files and parent traversal.
    static bool is_path_safe(const std::string& path);
};

} // namespace namek

#endif // NAMEK_MOD_SANDBOX_H
