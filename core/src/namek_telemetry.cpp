#include "namek_telemetry.h"
#include "namek.h"
#include "namek_toolbox_suite.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>

namespace namek {

namespace {
// Minimal HTTP call via curl (popen). Writes the response body and parses
// a trailing status code sentinel. Returns false on transport failure.
bool http_call(const std::string& method, const std::string& url,
               const std::string& body, const std::string& auth,
               std::string& out_body, long& out_status) {
    std::string cmd = "curl -s --max-time 15 -X " + method;
    if (!auth.empty()) cmd += " -H \"Authorization: " + auth + "\"";
    cmd += " -H \"Content-Type: application/json\"";
    std::string tmp_file;
    if (!body.empty()) {
        tmp_file = "/tmp/namek_tb_body_" + std::to_string(getpid());
        { std::ofstream f(tmp_file); f << body; }
        cmd += " -d @" + tmp_file;
    }
    cmd += " -w \"\\n@@STATUS:%{http_code}\" \"" + url + "\"";

    out_body.clear();
    out_status = 0;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        if (!tmp_file.empty()) std::remove(tmp_file.c_str());
        return false;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) out_body += buf;
    int rc = pclose(fp);
    if (!tmp_file.empty()) std::remove(tmp_file.c_str());

    size_t pos = out_body.rfind("@@STATUS:");
    if (pos != std::string::npos) {
        out_status = std::atol(out_body.c_str() + pos + 9);
        out_body.erase(pos);
    }
    return rc == 0 && out_status != 0;
}

std::string trim_copy(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Cheap JSON field lookup for string values.
std::string json_field(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    size_t colon = body.find(':', p + needle.size());
    if (colon == std::string::npos) return "";
    size_t vs = body.find('"', colon + 1);
    if (vs == std::string::npos) return "";
    size_t ve = vs + 1;
    std::string val;
    while (ve < body.size()) {
        char c = body[ve];
        if (c == '\\' && ve + 1 < body.size()) { val += body[ve + 1]; ve += 2; continue; }
        if (c == '"') break;
        val += c;
        ve++;
    }
    return val;
}
} // namespace

std::string Telemetry::backend_url() {
    const char* env = std::getenv("NAMEK_BACKEND_URL");
    if (env && *env) return env;
    return "http://localhost:8787";
}

std::string Telemetry::token_file() {
    const char* home = std::getenv("HOME");
    std::string dir = home ? std::string(home) : ".";
    return dir + "/.namek_session";
}

bool Telemetry::load_token(std::string& token, std::string& username) {
    std::ifstream f(token_file());
    if (!f.is_open()) return false;
    std::string line;
    token.clear();
    username.clear();
    while (std::getline(f, line)) {
        line = trim_copy(line);
        if (line.rfind("token=", 0) == 0) token = line.substr(6);
        else if (line.rfind("username=", 0) == 0) username = line.substr(9);
    }
    return !token.empty();
}

bool Telemetry::save_token(const std::string& token, const std::string& username) {
    std::ofstream f(token_file(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << "token=" << token << "\n";
    f << "username=" << username << "\n";
    f.close();
    ::chmod(token_file().c_str(), 0600);
    return true;
}

void Telemetry::clear_token() {
    std::remove(token_file().c_str());
}

std::string Telemetry::json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

TelemetryResult Telemetry::register_user(const std::string& username, const std::string& password) {
    std::string body = "{\"username\":\"" + json_escape(username) + "\",\"password\":\"" +
                       json_escape(password) + "\"}";
    TelemetryResult r;
    if (!http_call("POST", backend_url() + "/register", body, "", r.body, r.status)) {
        r.error = "No se pudo contactar el backend en " + backend_url() + " (¿está corriendo?)";
        return r;
    }
    r.ok = (r.status == 201 || r.status == 200);
    if (r.ok) {
        std::string tok = json_field(r.body, "token");
        if (!tok.empty()) save_token(tok, json_field(r.body, "username"));
    } else {
        r.error = json_field(r.body, "error");
    }
    return r;
}

TelemetryResult Telemetry::login_user(const std::string& username, const std::string& password) {
    std::string body = "{\"username\":\"" + json_escape(username) + "\",\"password\":\"" +
                       json_escape(password) + "\"}";
    TelemetryResult r;
    if (!http_call("POST", backend_url() + "/login", body, "", r.body, r.status)) {
        r.error = "No se pudo contactar el backend en " + backend_url() + " (¿está corriendo?)";
        return r;
    }
    r.ok = (r.status == 200);
    if (r.ok) {
        std::string tok = json_field(r.body, "token");
        if (!tok.empty()) save_token(tok, json_field(r.body, "username"));
    } else {
        r.error = json_field(r.body, "error");
    }
    return r;
}

bool Telemetry::logout() {
    std::string token, username;
    if (!load_token(token, username)) return false;
    TelemetryResult r;
    http_call("POST", backend_url() + "/logout", "", "Bearer " + token, r.body, r.status);
    clear_token();
    return true;
}

TelemetryResult Telemetry::me() {
    TelemetryResult r;
    std::string token, username;
    if (!load_token(token, username)) {
        r.error = "No hay sesión activa.";
        return r;
    }
    if (!http_call("GET", backend_url() + "/me", "", "Bearer " + token, r.body, r.status)) {
        r.error = "No se pudo contactar el backend.";
        return r;
    }
    r.ok = (r.status == 200);
    if (r.status == 401) {
        clear_token();
        r.error = "Sesión inválida o revocada (vuelve a iniciar sesión).";
    }
    if (!r.ok && r.error.empty()) r.error = json_field(r.body, "error");
    return r;
}

void Telemetry::audit(const std::string& event, const std::string& method, bool success,
                      const std::string& detail) {
    std::string token, username;
    if (!load_token(token, username)) return; // sin sesión: no auditar
    std::string body = "{\"event\":\"" + json_escape(event) + "\",\"method\":\"" +
                       json_escape(method) + "\",\"success\":" + (success ? "true" : "false") +
                       ",\"detail\":\"" + json_escape(detail) + "\"}";
    std::string out;
    long status = 0;
    http_call("POST", backend_url() + "/events", body, "Bearer " + token, out, status);
    if (status == 401) clear_token(); // sesión revocada (p.ej. ban)
}

TelemetryResult Telemetry::admin_users(const std::string& admin_user, const std::string& admin_pass) {
    std::string creds = CryptoTools::base64_encode(admin_user + ":" + admin_pass);
    TelemetryResult r;
    if (!http_call("GET", backend_url() + "/admin/users", "", "Basic " + creds, r.body, r.status)) {
        r.error = "No se pudo contactar el backend.";
        return r;
    }
    r.ok = (r.status == 200);
    if (!r.ok) r.error = "Credenciales admin inválidas.";
    return r;
}

TelemetryResult Telemetry::admin_events(const std::string& admin_user, const std::string& admin_pass,
                                        const std::string& filter_user, int limit) {
    std::string creds = CryptoTools::base64_encode(admin_user + ":" + admin_pass);
    std::string url = backend_url() + "/admin/events?limit=" + std::to_string(limit);
    if (!filter_user.empty()) url += "&username=" + filter_user;
    TelemetryResult r;
    if (!http_call("GET", url, "", "Basic " + creds, r.body, r.status)) {
        r.error = "No se pudo contactar el backend.";
        return r;
    }
    r.ok = (r.status == 200);
    if (!r.ok) r.error = "Credenciales admin inválidas.";
    return r;
}

TelemetryResult Telemetry::admin_ban(const std::string& admin_user, const std::string& admin_pass,
                                     const std::string& target, bool ban) {
    std::string creds = CryptoTools::base64_encode(admin_user + ":" + admin_pass);
    std::string body = "{\"username\":\"" + json_escape(target) + "\"}";
    std::string url = backend_url() + (ban ? "/admin/ban" : "/admin/unban");
    TelemetryResult r;
    if (!http_call("POST", url, body, "Basic " + creds, r.body, r.status)) {
        r.error = "No se pudo contactar el backend.";
        return r;
    }
    r.ok = (r.status == 200);
    if (!r.ok) r.error = json_field(r.body, "error");
    return r;
}

} // namespace namek
