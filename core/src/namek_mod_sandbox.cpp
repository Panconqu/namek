#include "namek_mod_sandbox.h"
#include "namek.h"
#include <iostream>
#include <algorithm>

namespace namek {

size_t ModSecuritySandbox::max_instruction_limit = 10000000; // 10M instructions limit
size_t ModSecuritySandbox::current_instruction_count = 0;

static const char* kEnvRegistryKey = "namek.sandbox.env";

// Whitelist of globals a mod may see. Anything not listed here (io, os,
// package, require, debug, dofile, loadfile, load, loadstring, rawget,
// rawset, getfenv, setfenv, collectgarbage, ...) is unreachable by design.
static const char* kSafeGlobals[] = {
    "print", "_VERSION",
    "assert", "error", "tonumber", "tostring", "type",
    "pairs", "ipairs", "next", "select", "pcall", "xpcall",
    "string", "math", "table", "coroutine", "utf8",
    "namek",
    nullptr
};

void ModSecuritySandbox::lua_instruction_hook(lua_State* L, lua_Debug* ar) {
    (void)ar;
    current_instruction_count += 1000;
    if (current_instruction_count >= max_instruction_limit) {
        luaL_error(L, "[NAMEK SANDBOX TRAP] Exceso de consumo de CPU / Bucle Infinito detectado en el Mod. Ejecución abortada.");
    }
}

// Copies _G[name] into the table currently on top of the stack, if present.
static void copy_safe_global(lua_State* L, const char* name) {
    lua_getglobal(L, name);
    if (!lua_isnil(L, -1)) {
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_settable(L, -4);
    }
    lua_pop(L, 1);
}

void ModSecuritySandbox::apply_sandbox_policy(lua_State* L) {
    current_instruction_count = 0;

    // 1. CPU quota hook (fires every 1000 instructions)
    lua_sethook(L, lua_instruction_hook, LUA_MASKCOUNT, 1000);

    // 2. Build the safe-globals whitelist table
    lua_createtable(L, 0, 32); // safe
    for (int i = 0; kSafeGlobals[i] != nullptr; ++i) {
        copy_safe_global(L, kSafeGlobals[i]);
    }

    // 2b. Expose a RESTRICTED os: only time/clock/date (no execute, no env,
    //     no files). Enough for timing-aware mods without any shell escape.
    lua_getglobal(L, "os");              // safe, os
    lua_createtable(L, 0, 3);            // safe, os, safe_os
    lua_getfield(L, -2, "time");  lua_setfield(L, -2, "time");   // safe_os.time = os.time
    lua_getfield(L, -2, "clock"); lua_setfield(L, -2, "clock");  // safe_os.clock = os.clock
    lua_getfield(L, -2, "date");  lua_setfield(L, -2, "date");   // safe_os.date = os.date
    lua_remove(L, 2);                    // pop os -> safe, safe_os
    lua_pushstring(L, "os");
    lua_pushvalue(L, -2);                // safe_os
    lua_settable(L, -4);                 // safe["os"] = safe_os
    lua_pop(L, 1);                       // pop safe_os

    // 3. env = {} with metatable { __index = safe }
    //    Mod globals resolve through the whitelist; the real _G is unreachable.
    lua_createtable(L, 0, 4);      // env
    lua_createtable(L, 0, 1);      // mt
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -4);          // safe
    lua_settable(L, -3);           // mt.__index = safe
    lua_setmetatable(L, -2);       // setmetatable(env, mt); pops mt
    // stack: safe, env

    // 4. Persist env in the registry for run_locked
    lua_pushstring(L, kEnvRegistryKey);
    lua_pushvalue(L, -2);          // env
    lua_settable(L, LUA_REGISTRYINDEX);
    lua_pop(L, 2);                 // pop env and safe
}

bool ModSecuritySandbox::run_locked(lua_State* L, const std::string& code,
                                    const std::string& chunkname) {
    lua_pushstring(L, kEnvRegistryKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        apply_sandbox_policy(L);
        lua_pushstring(L, kEnvRegistryKey);
        lua_gettable(L, LUA_REGISTRYINDEX);
    }
    int env_abs = lua_absindex(L, -1); // absolute index of the locked env
    // stack: env

    if (luaL_loadbuffer(L, code.c_str(), code.size(), chunkname.c_str()) != LUA_OK) {
        std::cerr << color::RED << "[LUA ERROR " << chunkname << "] "
                  << lua_tostring(L, -1) << color::RESET << "\n";
        lua_pop(L, 2); // env + error message
        return false;
    }
    // stack: env, chunk

    lua_pushvalue(L, env_abs);       // push env on top
    const char* upname = lua_setupvalue(L, -2, 1); // chunk._ENV = env, pops env
    if (upname == nullptr) {
        lua_pop(L, 2); // chunk + env
        std::cerr << color::RED << "[LUA ERROR] No se pudo aislar el entorno del Mod (sin _ENV)." << color::RESET << "\n";
        return false;
    }
    lua_remove(L, 1); // drop the leftover env reference below the chunk
    // stack: chunk

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        std::cerr << color::RED << "[LUA ERROR " << chunkname << "] "
                  << lua_tostring(L, -1) << color::RESET << "\n";
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool ModSecuritySandbox::is_path_safe(const std::string& path) {
    std::string lower = Utils::to_lower(path);
    // Block parent traversal
    if (lower.find("..") != std::string::npos) return false;
    // Block engine source / build trees
    if (lower.find("/core/") != std::string::npos) return false;
    if (lower.find("/cli/") != std::string::npos) return false;
    if (lower.find("/sdk/") != std::string::npos) return false;
    if (lower.find("/examples/") != std::string::npos) return false;
    // Block release bundles, manifests and secrets
    if (lower.find("/dist/") != std::string::npos) return false;
    if (lower.find("/bin/") != std::string::npos) return false;
    if (lower.find("/build/") != std::string::npos) return false;
    if (lower.find("modules/") != std::string::npos) return false;
    if (lower.find("release_manifest") != std::string::npos) return false;
    // Block env / key files and build config
    if (lower.find(".env") != std::string::npos) return false;
    if (lower.find("main.cpp") != std::string::npos) return false;
    if (lower.find("cmake") != std::string::npos) return false;
    return true;
}

} // namespace namek
