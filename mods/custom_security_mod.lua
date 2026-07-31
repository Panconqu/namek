-- NAMEK LUA MOD TEMPLATE
-- Author: Namek Developer Community

namek.print("¡Mod de Lua 'custom_security_mod' inicializado con éxito!")

-- Ejemplo de uso de la API Namek desde Lua
local mod_uuid = namek.uuid()
namek.print("UUID generado desde Mod: " .. mod_uuid)

-- Operaciones en la Base de Datos NoSQL NamekDB
namek.db_set("lua_mod_status", "active")
local status = namek.db_get("lua_mod_status")
namek.print("Estado en NamekDB: " .. status)

-- Generación de Datos Ficticios
local user_name = namek.fake_name()
local user_email = namek.fake_email()
namek.print("Usuario simulado: " .. user_name .. " (" .. user_email .. ")")
