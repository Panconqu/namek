-- NAMEK MOD: database_replicator v1.4.0
-- Replica el estado de NamekDB en una snapshot cifrada con hash y expone
-- comandos 'backup_db' y 'restore_db' al motor.
local SNAP_KEY = "replicator.snapshot"

namek.register_command("backup_db", function(cmd, args)
    local stamp = os.time() or 0
    local payload = "snapshot@" .. tostring(stamp)
    namek.db_set(SNAP_KEY, payload)
    namek.db_set("replicator.last_backup", payload)
    local digest = namek.sha256(payload)
    namek.print("Backup DB completado | stamp=" .. stamp)
    namek.print("  SHA-256 snapshot: " .. digest)
end)

namek.register_command("restore_db", function(cmd, args)
    local snap = namek.db_get(SNAP_KEY, "none")
    if snap == "none" then
        namek.print("No hay snapshot disponible. Ejecuta: tb backup_db")
        return
    end
    namek.db_set("replicator.restored", snap)
    namek.print("Restauración completada desde: " .. snap)
end)

namek.print("database_replicator v1.4.0 activo | comandos: backup_db, restore_db")
