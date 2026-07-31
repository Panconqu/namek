-- NAMEK MOD: security_sentinel v2.1.0
-- Monitor de seguridad del motor: registra una sesión auditada, cuenta
-- comandos del motor, y expone un comando 'lockdown' para proteger config.
namek.set_config("sentinel.armed", "true")

namek.hook("on_command", function(event, command)
    if command == "lockdown" then
        namek.set_config("engine.mode", "locked")
        namek.print("[SENTINEL] MOTOR EN LOCKDOWN: archivos y shell bloqueados")
    elseif command == "scan_mod" then
        namek.print("[SENTINEL] comando de mod detectado")
    end
end)

namek.register_command("lockdown", function(cmd, args)
    local info = namek.info()
    local nonce = namek.uuid()
    namek.db_set("sentinel.session", nonce)
    namek.db_set("sentinel.status", "locked")
    namek.print("Sentinel: sesión " .. nonce .. " | uptime=" .. info.uptime .. "s")
    namek.print("Sentinel: hash de auditoría = " .. namek.sha256(nonce .. "|" .. tostring(info.uptime)))
end)

namek.register_command("sentinel_status", function(cmd, args)
    local armed = namek.get_config("sentinel.armed", "unknown")
    local mode = namek.get_config("engine.mode", "unknown")
    namek.print("Sentinel estado -> armed: " .. armed .. " | engine.mode: " .. mode)
end)

local info = namek.info()
namek.print("security_sentinel v2.1.0 listo | motor=" .. info.engine
    .. " | sandbox=" .. info.sandbox .. " | file_io=" .. info.file_io)
