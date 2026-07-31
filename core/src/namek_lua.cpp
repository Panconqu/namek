#include "namek_lua.h"
#include "namek.h"
#include "namek_toolbox_suite.h"
#include "namek_obfuscator.h"
#include "namek_mod_sandbox.h"
#include "namek_binary_format.h"
#include "namek_telemetry.h"
#include <iostream>
#include <ctime>
#include <unistd.h>

#include <fstream>
#include <dirent.h>

namespace namek {

// Global pointers for C-style Lua callbacks
static NamekDB* g_lua_db_instance = nullptr;
LuaModEngine* LuaModEngine::s_active = nullptr;
std::unordered_map<std::string, std::string> LuaModEngine::s_config;

static time_t g_engine_start = std::time(nullptr);

// ==========================================
// C-FUNCTIONS EXPOSED TO LUA (namek.* API)
// ==========================================
static int l_namek_print(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    std::cout << color::CYAN << "[LUA MOD] " << color::GREEN << str << color::RESET << "\n";
    return 0;
}

static int l_namek_db_set(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* val = luaL_checkstring(L, 2);
    if (g_lua_db_instance) {
        g_lua_db_instance->set(key, val);
        g_lua_db_instance->save();
    }
    return 0;
}

static int l_namek_db_get(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* def_val = luaL_optstring(L, 2, "");
    if (g_lua_db_instance) {
        std::string res = g_lua_db_instance->get(key, def_val);
        lua_pushstring(L, res.c_str());
    } else {
        lua_pushstring(L, def_val);
    }
    return 1;
}

static int l_namek_uuid(lua_State* L) {
    std::string u = Utils::generate_uuid();
    lua_pushstring(L, u.c_str());
    return 1;
}

static int l_namek_base64_encode(lua_State* L) {
    const char* input = luaL_checkstring(L, 1);
    std::string enc = CryptoTools::base64_encode(input);
    lua_pushstring(L, enc.c_str());
    return 1;
}

static int l_namek_base64_decode(lua_State* L) {
    const char* input = luaL_checkstring(L, 1);
    std::string dec = CryptoTools::base64_decode(input);
    lua_pushstring(L, dec.c_str());
    return 1;
}

static int l_namek_sha256(lua_State* L) {
    const char* input = luaL_checkstring(L, 1);
    std::string digest = CryptoTools::sha256(input);
    lua_pushstring(L, digest.c_str());
    return 1;
}

static int l_namek_md5(lua_State* L) {
    const char* input = luaL_checkstring(L, 1);
    std::string digest = CryptoTools::md5(input);
    lua_pushstring(L, digest.c_str());
    return 1;
}

static int l_namek_fake_name(lua_State* L) {
    std::string name = DataGenerator::fake_name();
    lua_pushstring(L, name.c_str());
    return 1;
}

static int l_namek_fake_email(lua_State* L) {
    std::string email = DataGenerator::fake_email();
    lua_pushstring(L, email.c_str());
    return 1;
}

static int l_namek_obfuscate_mv(lua_State* L) {
    const char* code = luaL_checkstring(L, 1);
    const char* lang = luaL_optstring(L, 2, "python");
    std::string obf = Obfuscator::obfuscate_vm(code, lang);
    lua_pushstring(L, obf.c_str());
    return 1;
}

// Read-only engine information (the "info" mods are allowed to see).
static int l_namek_info(lua_State* L) {
    lua_newtable(L);

    lua_pushstring(L, "engine");   lua_pushstring(L, "NamekToolbox");         lua_settable(L, -3);
    lua_pushstring(L, "version");  lua_pushstring(L, "1.0.0");                lua_settable(L, -3);
    lua_pushstring(L, "pid");      lua_pushinteger(L, (lua_Integer)getpid()); lua_settable(L, -3);
    lua_pushstring(L, "uptime");   lua_pushinteger(L, (lua_Integer)(std::time(nullptr) - g_engine_start)); lua_settable(L, -3);
    lua_pushstring(L, "sandbox");  lua_pushstring(L, "locked");               lua_settable(L, -3);
    lua_pushstring(L, "file_io");  lua_pushstring(L, "denied");               lua_settable(L, -3);
    lua_pushstring(L, "os");       lua_pushstring(L, "denied");               lua_settable(L, -3);
    return 1;
}

// Runtime config surface: lets mods modify engine behavior.
static int l_namek_config_get(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* def = luaL_optstring(L, 2, "");
    std::string val = LuaModEngine::get_config(key, def);
    lua_pushstring(L, val.c_str());
    return 1;
}

static int l_namek_config_set(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    const char* val = luaL_checkstring(L, 2);
    LuaModEngine::set_config(key, val);
    return 0;
}

// Hook registration: namek.hook("event", function(payload))
static int l_namek_hook(lua_State* L) {
    const char* event = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        return luaL_error(L, "namek.hook: se esperaba una función como segundo argumento");
    }
    if (LuaModEngine* eng = LuaModEngine::s_active) {
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (eng->register_hook(event, ref)) return 0;
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        return luaL_error(L, "namek.hook: sin instancia de motor activa");
    }
    return luaL_error(L, "namek.hook: sin instancia de motor activa");
}

// Fire a hook from Lua: namek.emit("event", payload)
static int l_namek_emit(lua_State* L) {
    const char* event = luaL_checkstring(L, 1);
    const char* payload = luaL_optstring(L, 2, "");
    if (LuaModEngine* eng = LuaModEngine::s_active) {
        eng->emit(event, payload);
    }
    return 0;
}

// Custom command registration: namek.register_command("cmd", function(args))
static int l_namek_register_command(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        return luaL_error(L, "namek.register_command: se esperaba una función como segundo argumento");
    }
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (LuaModEngine* eng = LuaModEngine::s_active) {
        if (eng->register_command(cmd, ref)) return 0;
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        return luaL_error(L, "namek.register_command: sin instancia de motor activa");
    }
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
    return luaL_error(L, "namek.register_command: sin instancia de motor activa");
}

// ==========================================
// LUA ENGINE CLASS IMPLEMENTATION
// ==========================================
LuaModEngine::LuaModEngine(const std::string& db_path) : db_filepath(db_path) {
    L = luaL_newstate();
    luaL_openlibs(L);

    if (!g_lua_db_instance) {
        g_lua_db_instance = new NamekDB(db_filepath);
    }
    register_namek_api();

    if (!s_active) s_active = this;

    // Build the locked whitelist environment (mods never see io/os/debug/etc.)
    ModSecuritySandbox::apply_sandbox_policy(L);
}

LuaModEngine::~LuaModEngine() {
    for (auto& kv : mod_commands) luaL_unref(L, LUA_REGISTRYINDEX, kv.second);
    for (auto& kv : hooks) luaL_unref(L, LUA_REGISTRYINDEX, kv.second);
    mod_commands.clear();
    hooks.clear();

    if (s_active == this) s_active = nullptr;

    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

bool LuaModEngine::register_command(const std::string& cmd, int lua_ref) {
    auto it = mod_commands.find(cmd);
    if (it != mod_commands.end()) luaL_unref(L, LUA_REGISTRYINDEX, it->second);
    mod_commands[cmd] = lua_ref;
    return true;
}

bool LuaModEngine::register_hook(const std::string& event, int lua_ref) {
    auto it = hooks.find(event);
    if (it != hooks.end()) luaL_unref(L, LUA_REGISTRYINDEX, it->second);
    hooks[event] = lua_ref;
    return true;
}

bool LuaModEngine::has_command(const std::string& cmd) const {
    return mod_commands.find(cmd) != mod_commands.end();
}

bool LuaModEngine::dispatch_command(const std::string& cmd, const std::vector<std::string>& args) {
    auto it = mod_commands.find(cmd);
    if (it == mod_commands.end()) return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
    lua_pushstring(L, cmd.c_str());
    lua_createtable(L, (int)args.size(), 0);
    for (size_t i = 0; i < args.size(); ++i) {
        lua_pushstring(L, args[i].c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        std::cerr << color::RED << "[MOD COMMAND " << cmd << "] "
                  << lua_tostring(L, -1) << color::RESET << "\n";
        lua_pop(L, 1);
    }
    return true;
}

bool LuaModEngine::emit(const std::string& event, const std::string& payload) {
    auto it = hooks.find(event);
    if (it == hooks.end()) return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
    lua_pushstring(L, event.c_str());
    lua_pushstring(L, payload.c_str());
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        std::cerr << color::RED << "[HOOK " << event << "] "
                  << lua_tostring(L, -1) << color::RESET << "\n";
        lua_pop(L, 1);
    }
    return true;
}

std::string LuaModEngine::get_config(const std::string& key, const std::string& def) {
    auto it = s_config.find(key);
    return it != s_config.end() ? it->second : def;
}

void LuaModEngine::set_config(const std::string& key, const std::string& value) {
    s_config[key] = value;
}

void LuaModEngine::register_namek_api() {
    lua_newtable(L);

    lua_pushcfunction(L, l_namek_print);
    lua_setfield(L, -2, "print");

    lua_pushcfunction(L, l_namek_db_set);
    lua_setfield(L, -2, "db_set");

    lua_pushcfunction(L, l_namek_db_get);
    lua_setfield(L, -2, "db_get");

    lua_pushcfunction(L, l_namek_uuid);
    lua_setfield(L, -2, "uuid");

    lua_pushcfunction(L, l_namek_base64_encode);
    lua_setfield(L, -2, "base64_encode");

    lua_pushcfunction(L, l_namek_base64_decode);
    lua_setfield(L, -2, "base64_decode");

    lua_pushcfunction(L, l_namek_sha256);
    lua_setfield(L, -2, "sha256");

    lua_pushcfunction(L, l_namek_md5);
    lua_setfield(L, -2, "md5");

    lua_pushcfunction(L, l_namek_fake_name);
    lua_setfield(L, -2, "fake_name");

    lua_pushcfunction(L, l_namek_fake_email);
    lua_setfield(L, -2, "fake_email");

    lua_pushcfunction(L, l_namek_obfuscate_mv);
    lua_setfield(L, -2, "obfuscate_mv");

    lua_pushcfunction(L, l_namek_info);
    lua_setfield(L, -2, "info");

    lua_pushcfunction(L, l_namek_config_get);
    lua_setfield(L, -2, "get_config");

    lua_pushcfunction(L, l_namek_config_set);
    lua_setfield(L, -2, "set_config");

    lua_pushcfunction(L, l_namek_hook);
    lua_setfield(L, -2, "hook");

    lua_pushcfunction(L, l_namek_emit);
    lua_setfield(L, -2, "emit");

    lua_pushcfunction(L, l_namek_register_command);
    lua_setfield(L, -2, "register_command");

    lua_setglobal(L, "namek");
}

bool LuaModEngine::run_script(const std::string& lua_code) {
    return ModSecuritySandbox::run_locked(L, lua_code, "=(namek script)");
}

bool LuaModEngine::run_file(const std::string& filepath) {
    if (!ModSecuritySandbox::is_path_safe(filepath)) {
        std::cerr << color::RED << "Error: El Mod intenta acceder a una ruta prohibida: "
                  << filepath << color::RESET << "\n";
        Telemetry::audit("sandbox_block", "file_path", false, filepath);
        return false;
    }
    if (!Utils::file_exists(filepath)) {
        std::cerr << color::RED << "Error: Mod de Lua no encontrado '" << filepath << "'" << color::RESET << "\n";
        Telemetry::audit("mod_run", filepath, false, "archivo no encontrado");
        return false;
    }

    bool ok = false;
    // Encrypted mods are decrypted in memory and never written to disk.
    if (filepath.rfind(".tb.bin") != std::string::npos) {
        std::string key = TBBinaryCompiler::resolve_release_key_for(filepath);
        std::string code = TBBinaryCompiler::decode_binary(filepath, key);
        if (code.empty()) {
            std::cerr << color::RED << "Error: No se pudo descifrar el Mod binario '" << filepath << "'" << color::RESET << "\n";
            Telemetry::audit("mod_run", filepath, false, "fallo al descifrar");
            return false;
        }
        ok = ModSecuritySandbox::run_locked(L, code, filepath);
    } else {
        ok = ModSecuritySandbox::run_locked(L, Utils::read_file(filepath), filepath);
    }

    Telemetry::audit("mod_run", filepath, ok);
    return ok;
}

std::vector<std::string> LuaModEngine::list_mods(const std::string& mods_dir) {
    std::vector<std::string> mods;
    DIR* dir = opendir(mods_dir.c_str());
    if (!dir) return mods;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string fname = ent->d_name;
        if (fname.rfind(".lua") != std::string::npos ||
            fname.rfind(".tb.bin") != std::string::npos) {
            mods.push_back(mods_dir + "/" + fname);
        }
    }
    closedir(dir);
    return mods;
}

int LuaModEngine::load_all_mods(const std::string& mods_dir) {
    auto mods = list_mods(mods_dir);
    int count = 0;
    for (const auto& mod_path : mods) {
        std::cout << color::CYAN << "[+] Cargando Mod de Lua: " << mod_path << color::RESET << "\n";
        if (run_file(mod_path)) {
            count++;
        }
    }
    return count;
}

int LuaModEngine::preload_mods(const std::string& mods_dir) {
    if (mods_preloaded) return 0;
    mods_preloaded = true;
    int count = 0;
    for (const auto& mod_path : list_mods(mods_dir)) {
        if (run_file(mod_path)) count++;
    }
    return count;
}

bool LuaModEngine::pack_mod(const std::string& lua_file, const std::string& out_bin) {
    if (!Utils::file_exists(lua_file)) {
        std::cerr << color::RED << "Error: No se encontró el Mod '" << lua_file << "'" << color::RESET << "\n";
        return false;
    }
    std::string code = Utils::read_file(lua_file);
    std::string out = out_bin.empty() ? (lua_file + ".tb.bin") : out_bin;
    std::string key = TBBinaryCompiler::resolve_release_key_for(lua_file);
    if (TBBinaryCompiler::compile_to_binary(code, out, TBBModuleType::GENERIC, key)) {
        std::cout << color::GREEN << "✓ Mod cifrado (ChaCha20): " << out << color::RESET << "\n";
        std::cout << color::YELLOW << "  El código del Mod ya no es legible en disco." << color::RESET << "\n";
        Telemetry::audit("mod_pack", lua_file, true, out);
        return true;
    }
    Telemetry::audit("mod_pack", lua_file, false, "fallo al cifrar");
    return false;
}

bool LuaModEngine::create_mod_template(const std::string& mod_name, const std::string& mods_dir) {
    system(("mkdir -p " + mods_dir).c_str());
    std::string filepath = mods_dir + "/" + mod_name + ".lua";

    std::string content = R"TMPL(-- NAMEK LUA MOD TEMPLATE
-- Author: Namek Developer Community

namek.print("¡Mod de Lua '__MOD__' inicializado con éxito!")

-- Acceso a información del motor (solo lectura)
local info = namek.info()
namek.print("Motor: " .. info.engine .. " v" .. info.version)

-- Los mods NO tienen acceso a archivos, shell ni Lua interno.
-- (io, os, require, package, debug, loadfile, load ... están bloqueados)

-- Modificar el comportamiento del motor: configuración en runtime
namek.set_config("mod___MOD___enabled", "true")
namek.print("Config mod: " .. namek.get_config("mod___MOD___enabled"))

-- Registrar hooks que el motor dispara
namek.hook("on_mod_loaded", function(event)
    namek.print("[HOOK] evento " .. event .. " recibido por '__MOD__'")
end)

-- Registrar un comando nuevo del motor: tb __MOD__ <arg>
namek.register_command("__MOD__", function(cmd, args)
    namek.print("Comando personalizado 'tb __MOD__' ejecutado con " .. #args .. " argumento(s)")
end)

-- Operaciones en la Base de Datos NoSQL NamekDB
namek.db_set("lua_mod_status", "active")
local status = namek.db_get("lua_mod_status")
namek.print("Estado en NamekDB: " .. status)

-- Generación de Datos Ficticios
local user_name = namek.fake_name()
local user_email = namek.fake_email()
namek.print("Usuario simulado: " .. user_name .. " (" .. user_email .. ")")
)TMPL";

    std::string token = "__MOD__";
    size_t pos = 0;
    while ((pos = content.find(token, pos)) != std::string::npos) {
        content.replace(pos, token.size(), mod_name);
        pos += mod_name.size();
    }

    return Utils::write_file(filepath, content);
}

} // namespace namek
