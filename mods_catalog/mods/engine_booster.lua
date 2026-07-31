-- NAMEK MOD: engine_booster
-- Los mods corren en un sandbox duro: pueden MODIFICAR el motor
-- (hooks, comandos nuevos, config) pero NO pueden leer archivos,
-- ejecutar shell ni acceder al código del motor.
namek.set_config("engine.mode", "sandboxed")

namek.hook("on_command", function(event, command)
    namek.print("[HOOK] el motor ejecutó el comando: " .. command)
end)

namek.register_command("scan_mod", function(cmd, args)
    namek.print("tb scan_mod | argumentos: " .. table.concat(args, ", "))
    namek.print("  sha256('namek') = " .. namek.sha256("namek"))
    namek.print("  md5('namek')    = " .. namek.md5("namek"))
end)

namek.register_command("set_verbose", function(cmd, args)
    local v = args[1] or "off"
    namek.set_config("engine.verbose", v)
    namek.print("Motor configurado: verbose = " .. v)
end)

local info = namek.info()
namek.print("engine_booster activo | " .. info.engine .. " v" .. info.version
    .. " | file_io=" .. info.file_io .. " | os=" .. info.os)
