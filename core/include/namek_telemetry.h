#ifndef NAMEK_TELEMETRY_H
#define NAMEK_TELEMETRY_H

#include <string>
#include <vector>

namespace namek {

struct TelemetryResult {
    bool ok = false;
    long status = 0;
    std::string error;
    std::string body;
};

// ==========================================
// Cliente de telemetría / auditoría contra el
// backend Rust (namek_backend). Nunca habla
// directo con MongoDB; todo pasa por el backend.
// ==========================================
class Telemetry {
public:
    // URL del backend: env NAMEK_BACKEND_URL o http://localhost:8787
    static std::string backend_url();
    static std::string token_file();

    // Sesión local (~/.namek_session)
    static bool load_token(std::string& token, std::string& username);
    static bool save_token(const std::string& token, const std::string& username);
    static void clear_token();

    // Auth
    static TelemetryResult register_user(const std::string& username, const std::string& password);
    static TelemetryResult login_user(const std::string& username, const std::string& password);
    static bool logout();
    static TelemetryResult me();

    // Auditoría del motor (best-effort, no bloquea nunca al usuario)
    static void audit(const std::string& event, const std::string& method, bool success,
                      const std::string& detail = "");

    // Panel admin (Basic auth con ADMIN_USER / ADMIN_PASS)
    static TelemetryResult admin_users(const std::string& admin_user, const std::string& admin_pass);
    static TelemetryResult admin_events(const std::string& admin_user, const std::string& admin_pass,
                                        const std::string& filter_user = "", int limit = 50);
    static TelemetryResult admin_ban(const std::string& admin_user, const std::string& admin_pass,
                                     const std::string& target, bool ban);

    // Utilidades JSON expuestas para los handlers CLI
    static std::string json_escape(const std::string& s);
};

} // namespace namek

#endif // NAMEK_TELEMETRY_H
