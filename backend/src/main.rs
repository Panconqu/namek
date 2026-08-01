#![allow(non_snake_case)]
use argon2::password_hash::rand_core::OsRng;
use argon2::password_hash::SaltString;
use argon2::{Argon2, PasswordHash, PasswordHasher, PasswordVerifier};
use axum::extract::{Query, State};
use axum::http::{HeaderMap, StatusCode};
use axum::response::IntoResponse;
use axum::routing::{get, post};
use axum::{Json, Router};
use base64::{engine::general_purpose, Engine as _};
use chrono::Utc;
use rand::Rng;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use sqlx::postgres::PgPoolOptions;
use sqlx::FromRow;
use tower_http::cors::{Any, CorsLayer};

// ---------------------------------------------------------------------------
// Config / estado
// ---------------------------------------------------------------------------

#[derive(Clone)]
struct AppState {
    pool: sqlx::PgPool,
    admin_user: String,
    admin_pass: String,
}

#[derive(Debug, FromRow)]
struct User {
    username: String,
    pass_hash: String,
    banned: bool,
    created_at: chrono::DateTime<Utc>,
}

#[derive(Debug, FromRow, Serialize, Clone)]
struct EventDoc {
    username: String,
    event: String,
    method: String,
    success: bool,
    detail: String,
    ts: chrono::DateTime<Utc>,
}

// ---------------------------------------------------------------------------
// Bodies de petición / respuesta
// ---------------------------------------------------------------------------

#[derive(Deserialize)]
struct Credentials {
    username: String,
    password: String,
}

#[derive(Deserialize)]
struct NewEvent {
    event: String,
    method: String,
    success: bool,
    #[serde(default)]
    detail: String,
}

#[derive(Serialize)]
struct TokenResponse {
    token: String,
    username: String,
}

#[derive(Deserialize)]
struct BanRequest {
    username: String,
}

#[derive(Serialize)]
struct MeResponse {
    username: String,
    banned: bool,
}

#[derive(Deserialize)]
struct AdminEventsQuery {
    #[serde(default)]
    username: Option<String>,
    #[serde(default = "default_limit")]
    limit: i64,
}

fn default_limit() -> i64 {
    100
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

fn sha256_hex(s: &str) -> String {
    let mut h = Sha256::new();
    h.update(s.as_bytes());
    hex::encode(h.finalize())
}

fn gen_token() -> String {
    let mut bytes = [0u8; 32];
    rand::thread_rng().fill(&mut bytes);
    hex::encode(bytes)
}

async fn bearer_username(state: &AppState, headers: &HeaderMap) -> Result<String, StatusCode> {
    let auth = headers
        .get("authorization")
        .ok_or(StatusCode::UNAUTHORIZED)?
        .to_str()
        .map_err(|_| StatusCode::UNAUTHORIZED)?;
    let token = auth
        .strip_prefix("Bearer ")
        .ok_or(StatusCode::UNAUTHORIZED)?;
    let hash = sha256_hex(token);
    sqlx::query_scalar::<_, String>("SELECT username FROM tokens WHERE token_hash = $1")
        .bind(&hash)
        .fetch_optional(&state.pool)
        .await
        .ok()
        .flatten()
        .ok_or(StatusCode::UNAUTHORIZED)
}

fn admin_ok(state: &AppState, headers: &HeaderMap) -> Result<(), StatusCode> {
    let auth = headers
        .get("authorization")
        .ok_or(StatusCode::UNAUTHORIZED)?
        .to_str()
        .map_err(|_| StatusCode::UNAUTHORIZED)?;
    let (scheme, raw) = auth
        .split_once(' ')
        .ok_or(StatusCode::UNAUTHORIZED)?;
    if scheme != "Basic" {
        return Err(StatusCode::UNAUTHORIZED);
    }
    let decoded = general_purpose::STANDARD
        .decode(raw)
        .map_err(|_| StatusCode::UNAUTHORIZED)?;
    let creds = String::from_utf8(decoded).map_err(|_| StatusCode::UNAUTHORIZED)?;
    let (u, p) = creds
        .split_once(':')
        .ok_or(StatusCode::UNAUTHORIZED)?;
    if u == state.admin_user && p == state.admin_pass {
        Ok(())
    } else {
        Err(StatusCode::UNAUTHORIZED)
    }
}

fn internal<E>(_e: E) -> StatusCode {
    StatusCode::INTERNAL_SERVER_ERROR
}

// ---------------------------------------------------------------------------
// Handlers de autenticación
// ---------------------------------------------------------------------------

async fn register(State(st): State<AppState>, Json(c): Json<Credentials>) -> impl IntoResponse {
    let u = c.username.trim();
    if u.len() < 3 || c.password.len() < 6 {
        return (
            StatusCode::BAD_REQUEST,
            Json(serde_json::json!({"error": "El nombre debe tener >=3 caracteres y la password >=6."})),
        )
            .into_response();
    }
    let existing: i64 = sqlx::query_scalar("SELECT COUNT(*) FROM users WHERE username = $1")
        .bind(u)
        .fetch_one(&st.pool)
        .await
        .unwrap_or(1);
    if existing > 0 {
        return (
            StatusCode::CONFLICT,
            Json(serde_json::json!({"error": "El usuario ya existe."})),
        )
            .into_response();
    }

    let salt = SaltString::generate(&mut OsRng);
    let hash = Argon2::default()
        .hash_password(c.password.as_bytes(), &salt)
        .map_err(internal)
        .unwrap()
        .to_string();

    if sqlx::query("INSERT INTO users (username, pass_hash) VALUES ($1, $2)")
        .bind(u)
        .bind(&hash)
        .execute(&st.pool)
        .await
        .is_err()
    {
        return (
            StatusCode::CONFLICT,
            Json(serde_json::json!({"error": "El usuario ya existe."})),
        )
            .into_response();
    }

    let token = issue_token(&st, u).await;
    (
        StatusCode::CREATED,
        Json(TokenResponse {
            token,
            username: u.to_string(),
        }),
    )
        .into_response()
}

async fn issue_token(st: &AppState, username: &str) -> String {
    let token = gen_token();
    let _ = sqlx::query("INSERT INTO tokens (token_hash, username) VALUES ($1, $2)")
        .bind(sha256_hex(&token))
        .bind(username)
        .execute(&st.pool)
        .await;
    token
}

async fn login(State(st): State<AppState>, Json(c): Json<Credentials>) -> impl IntoResponse {
    let u = c.username.trim();
    let user: Option<User> = sqlx::query_as(
        "SELECT username, pass_hash, banned, created_at FROM users WHERE username = $1",
    )
    .bind(u)
    .fetch_optional(&st.pool)
    .await
    .map_err(internal)
    .unwrap();
    let Some(user) = user else {
        return (
            StatusCode::UNAUTHORIZED,
            Json(serde_json::json!({"error": "Credenciales inválidas."})),
        )
            .into_response();
    };

    let parsed = match PasswordHash::new(&user.pass_hash) {
        Ok(p) => p,
        Err(_) => {
            return (
                StatusCode::INTERNAL_SERVER_ERROR,
                Json(serde_json::json!({"error": "Error interno."})),
            )
                .into_response();
        }
    };
    if Argon2::default()
        .verify_password(c.password.as_bytes(), &parsed)
        .is_err()
    {
        return (
            StatusCode::UNAUTHORIZED,
            Json(serde_json::json!({"error": "Credenciales inválidas."})),
        )
            .into_response();
    }
    if user.banned {
        return (
            StatusCode::FORBIDDEN,
            Json(serde_json::json!({"error": "Esta cuenta está suspendida."})),
        )
            .into_response();
    }

    let token = issue_token(&st, u).await;
    (StatusCode::OK, Json(TokenResponse { token, username: u.to_string() })).into_response()
}

async fn logout(State(st): State<AppState>, headers: HeaderMap) -> impl IntoResponse {
    if bearer_username(&st, &headers).await.is_err() {
        return StatusCode::UNAUTHORIZED.into_response();
    }
    let auth = headers
        .get("authorization")
        .and_then(|v| v.to_str().ok())
        .unwrap_or("");
    let token = auth.strip_prefix("Bearer ").unwrap_or("");
    let _ = sqlx::query("DELETE FROM tokens WHERE token_hash = $1")
        .bind(sha256_hex(token))
        .execute(&st.pool)
        .await;
    StatusCode::OK.into_response()
}

async fn me(State(st): State<AppState>, headers: HeaderMap) -> impl IntoResponse {
    let username = match bearer_username(&st, &headers).await {
        Ok(u) => u,
        Err(e) => return e.into_response(),
    };
    let banned: bool = sqlx::query_scalar("SELECT banned FROM users WHERE username = $1")
        .bind(&username)
        .fetch_optional(&st.pool)
        .await
        .ok()
        .flatten()
        .unwrap_or(false);
    (StatusCode::OK, Json(MeResponse { username, banned })).into_response()
}

// ---------------------------------------------------------------------------
// Log de eventos del motor
// ---------------------------------------------------------------------------

async fn add_event(
    State(st): State<AppState>,
    headers: HeaderMap,
    Json(body): Json<NewEvent>,
) -> impl IntoResponse {
    let username = match bearer_username(&st, &headers).await {
        Ok(u) => u,
        Err(e) => return e.into_response(),
    };
    if sqlx::query(
        "INSERT INTO events (username, event, method, success, detail) VALUES ($1, $2, $3, $4, $5)",
    )
    .bind(&username)
    .bind(&body.event)
    .bind(&body.method)
    .bind(body.success)
    .bind(&body.detail)
    .execute(&st.pool)
    .await
    .is_err()
    {
        return StatusCode::INTERNAL_SERVER_ERROR.into_response();
    }
    StatusCode::CREATED.into_response()
}

// ---------------------------------------------------------------------------
// Panel admin (solo ADMIN_USER / ADMIN_PASS)
// ---------------------------------------------------------------------------

#[derive(Debug, FromRow, Serialize)]
struct AdminUserView {
    username: String,
    banned: bool,
    created_at: chrono::DateTime<Utc>,
    events: i64,
}

async fn admin_users(State(st): State<AppState>, headers: HeaderMap) -> impl IntoResponse {
    if let Err(e) = admin_ok(&st, &headers) {
        return e.into_response();
    }
    let rows: Vec<AdminUserView> = sqlx::query_as(
        "SELECT u.username AS username, u.banned AS banned, u.created_at AS created_at, \
         COALESCE(COUNT(e.id), 0)::BIGINT AS events \
         FROM users u LEFT JOIN events e ON e.username = u.username \
         GROUP BY u.username, u.banned, u.created_at \
         ORDER BY events DESC, u.username",
    )
    .fetch_all(&st.pool)
    .await
    .map_err(internal)
    .unwrap();
    (StatusCode::OK, Json(rows)).into_response()
}

async fn admin_events(
    State(st): State<AppState>,
    headers: HeaderMap,
    Query(q): Query<AdminEventsQuery>,
) -> impl IntoResponse {
    if let Err(e) = admin_ok(&st, &headers) {
        return e.into_response();
    }
    let limit = q.limit.max(1).min(1000);
    let rows: Vec<EventDoc> = match q.username {
        Some(u) => {
            sqlx::query_as(
                "SELECT username, event, method, success, detail, ts \
                 FROM events WHERE username = $1 ORDER BY ts DESC LIMIT $2",
            )
            .bind(u)
            .bind(limit)
            .fetch_all(&st.pool)
            .await
        }
        None => {
            sqlx::query_as(
                "SELECT username, event, method, success, detail, ts \
                 FROM events ORDER BY ts DESC LIMIT $1",
            )
            .bind(limit)
            .fetch_all(&st.pool)
            .await
        }
    }
    .map_err(internal)
    .unwrap();
    (StatusCode::OK, Json(rows)).into_response()
}

async fn admin_ban(
    State(st): State<AppState>,
    headers: HeaderMap,
    Json(body): Json<BanRequest>,
) -> impl IntoResponse {
    if let Err(e) = admin_ok(&st, &headers) {
        return e.into_response();
    }
    let res = sqlx::query("UPDATE users SET banned = $1 WHERE username = $2")
        .bind(true)
        .bind(&body.username)
        .execute(&st.pool)
        .await
        .map_err(internal)
        .unwrap();
    if res.rows_affected() == 0 {
        return (
            StatusCode::NOT_FOUND,
            Json(serde_json::json!({"error": "Usuario no encontrado."})),
        )
            .into_response();
    }
    let _ = sqlx::query("DELETE FROM tokens WHERE username = $1")
        .bind(&body.username)
        .execute(&st.pool)
        .await;
    (StatusCode::OK, Json(serde_json::json!({"ok": true}))).into_response()
}

async fn admin_unban(
    State(st): State<AppState>,
    headers: HeaderMap,
    Json(body): Json<BanRequest>,
) -> impl IntoResponse {
    if let Err(e) = admin_ok(&st, &headers) {
        return e.into_response();
    }
    let res = sqlx::query("UPDATE users SET banned = $1 WHERE username = $2")
        .bind(false)
        .bind(&body.username)
        .execute(&st.pool)
        .await
        .map_err(internal)
        .unwrap();
    if res.rows_affected() == 0 {
        return (
            StatusCode::NOT_FOUND,
            Json(serde_json::json!({"error": "Usuario no encontrado."})),
        )
            .into_response();
    }
    (StatusCode::OK, Json(serde_json::json!({"ok": true}))).into_response()
}

async fn health() -> impl IntoResponse {
    (
        StatusCode::OK,
        Json(serde_json::json!({"status": "ok", "service": "namek_backend"})),
    )
}

async fn root() -> impl IntoResponse {
    (
        StatusCode::OK,
        Json(serde_json::json!({
            "service": "namek_backend",
            "version": "0.1.0",
            "descripcion": "Sistema de cuentas y auditoría del motor Namek. API universal (CORS abierto).",
            "endpoints": {
                "POST /register": "{\"username\":\"x\",\"password\":\"y\"} -> {token, username}",
                "POST /login": "{\"username\":\"x\",\"password\":\"y\"} -> {token, username}",
                "POST /logout": "Authorization: Bearer <token>",
                "GET /me": "Authorization: Bearer <token> -> {username, banned}",
                "POST /events": "Authorization: Bearer <token>  body {\"event\":\"command\",\"method\":\"scan\",\"success\":true,\"detail\":\"\"}",
                "GET /admin/users": "Authorization: Basic base64(admin_user:admin_pass)",
                "GET /admin/events": "?username=&limit=  (admin)",
                "POST /admin/ban": "{\"username\":\"x\"} (admin)",
                "POST /admin/unban": "{\"username\":\"x\"} (admin)",
                "GET /openapi.json": "Especificación OpenAPI de esta API"
            },
            "docs": "/openapi.json"
        })),
    )
}

fn openapi_json() -> serde_json::Value {
    serde_json::json!({
        "openapi": "3.0.3",
        "info": {
            "title": "Namek Backend API",
            "version": "0.1.0",
            "description": "Sistema de cuentas y auditoría del motor Namek. Para llamarlo desde cualquier cliente/IA."
        },
        "servers": [],
        "paths": {
            "/register": {
                "post": {
                    "summary": "Crear cuenta",
                    "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["username","password"], "properties": {"username": {"type": "string", "minLength": 3}, "password": {"type": "string", "minLength": 6}}}}}},
                    "responses": {"201": {"description": "Cuenta creada, devuelve {token, username}"}, "409": {"description": "El usuario ya existe"}, "400": {"description": "Validación"}}
                }
            },
            "/login": {
                "post": {
                    "summary": "Iniciar sesión",
                    "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["username","password"], "properties": {"username": {"type": "string"}, "password": {"type": "string"}}}}}},
                    "responses": {"200": {"description": "Devuelve {token, username}"}, "401": {"description": "Credenciales inválidas"}, "403": {"description": "Cuenta suspendida"}}
                }
            },
            "/logout": {
                "post": {
                    "summary": "Revocar token",
                    "security": [{"bearerAuth": []}],
                    "responses": {"200": {"description": "OK"}}
                }
            },
            "/me": {
                "get": {
                    "summary": "Estado de mi cuenta",
                    "security": [{"bearerAuth": []}],
                    "responses": {"200": {"description": "{username, banned}"}}
                }
            },
            "/events": {
                "post": {
                    "summary": "Registrar acción del motor",
                    "security": [{"bearerAuth": []}],
                    "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["event","method","success"], "properties": {"event": {"type": "string"}, "method": {"type": "string"}, "success": {"type": "boolean"}, "detail": {"type": "string"}}}}}},
                    "responses": {"201": {"description": "Evento registrado"}}
                }
            },
            "/admin/users": {
                "get": {
                    "summary": "Lista de usuarios",
                    "security": [{"basicAuth": []}],
                    "responses": {"200": {"description": "Array de {username, banned, created_at, events}"}}
                }
            },
            "/admin/events": {
                "get": {
                    "summary": "Log de eventos del motor",
                    "security": [{"basicAuth": []}],
                    "parameters": [
                        {"name": "username", "in": "query", "schema": {"type": "string"}},
                        {"name": "limit", "in": "query", "schema": {"type": "integer", "default": 100}}
                    ],
                    "responses": {"200": {"description": "Array de eventos"}}
                }
            },
            "/admin/ban": {
                "post": {
                    "summary": "Banear usuario y revocar sus sesiones",
                    "security": [{"basicAuth": []}],
                    "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["username"], "properties": {"username": {"type": "string"}}}}}},
                    "responses": {"200": {"description": "ok"}}
                }
            },
            "/admin/unban": {
                "post": {
                    "summary": "Desbanear usuario",
                    "security": [{"basicAuth": []}],
                    "requestBody": {"content": {"application/json": {"schema": {"type": "object", "required": ["username"], "properties": {"username": {"type": "string"}}}}}},
                    "responses": {"200": {"description": "ok"}}
                }
            }
        },
        "components": {
            "securitySchemes": {
                "bearerAuth": {"type": "http", "scheme": "bearer"},
                "basicAuth": {"type": "http", "scheme": "basic"}
            }
        }
    })
}

async fn openapi() -> impl IntoResponse {
    (StatusCode::OK, Json(openapi_json()))
}

// ---------------------------------------------------------------------------
// Esquema
// ---------------------------------------------------------------------------

async fn ensure_schema(pool: &sqlx::PgPool) {
    for ddl in [
        "CREATE TABLE IF NOT EXISTS users (
            id BIGSERIAL PRIMARY KEY,
            username TEXT UNIQUE NOT NULL,
            pass_hash TEXT NOT NULL,
            banned BOOLEAN NOT NULL DEFAULT FALSE,
            created_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )",
        "CREATE TABLE IF NOT EXISTS tokens (
            token_hash TEXT PRIMARY KEY,
            username TEXT NOT NULL,
            created_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )",
        "CREATE TABLE IF NOT EXISTS events (
            id BIGSERIAL PRIMARY KEY,
            username TEXT NOT NULL,
            event TEXT NOT NULL,
            method TEXT NOT NULL,
            success BOOLEAN NOT NULL,
            detail TEXT NOT NULL DEFAULT '',
            ts TIMESTAMPTZ NOT NULL DEFAULT now()
        )",
        "CREATE INDEX IF NOT EXISTS idx_events_username ON events(username)",
        "CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts)",
    ] {
        sqlx::query(ddl)
            .execute(pool)
            .await
            .expect("fallo al crear el esquema de PostgreSQL");
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

#[tokio::main]
async fn main() {
    let _ = dotenvy::dotenv();
    let _ = dotenvy::from_path("../.env");

    let url = std::env::var("DATABASE_URL").unwrap_or_default();
    let admin_user = std::env::var("ADMIN_USER").unwrap_or_else(|_| "admin".to_string());
    let admin_pass = std::env::var("ADMIN_PASS").unwrap_or_else(|_| "admin123".to_string());
    let port = std::env::var("PORT")
        .or_else(|_| std::env::var("BACKEND_PORT"))
        .unwrap_or_else(|_| "8787".to_string());

    if url.is_empty() || url.contains("<db_password>") || url.contains("<YOUR_") {
        eprintln!(
            "[namek_backend] ERROR: DATABASE_URL está vacío o con placeholder. Pon tu string real de PostgreSQL en .env (no la compartas en chats)."
        );
        std::process::exit(1);
    }

    // Seguridad: jamás arrancar con credenciales admin débiles o por defecto.
    let weak: [&str; 5] = ["admin", "admin123", "password", "123456", "cambia-esta-contrasena"];
    if admin_pass.len() < 12 || weak.contains(&admin_pass.as_str()) || weak.contains(&admin_user.to_lowercase().as_str()) {
        eprintln!(
            "[namek_backend] ERROR: credenciales admin débiles o por defecto. Establece ADMIN_USER/ADMIN_PASS fuertes en .env (>=12 caracteres, no 'admin'/'admin123'/'password')."
        );
        std::process::exit(1);
    }

    let pool = PgPoolOptions::new()
        .max_connections(5)
        .connect(&url)
        .await
        .unwrap_or_else(|e| {
            eprintln!("[namek_backend] ERROR: no se pudo conectar a PostgreSQL: {e}");
            std::process::exit(1);
        });
    ensure_schema(&pool).await;

    let state = AppState {
        pool,
        admin_user,
        admin_pass,
    };

    let app = Router::new()
        .route("/", get(root))
        .route("/health", get(health))
        .route("/openapi.json", get(openapi))
        .route("/register", post(register))
        .route("/login", post(login))
        .route("/logout", post(logout))
        .route("/me", get(me))
        .route("/events", post(add_event))
        .route("/admin/users", get(admin_users))
        .route("/admin/events", get(admin_events))
        .route("/admin/ban", post(admin_ban))
        .route("/admin/unban", post(admin_unban))
        .with_state(state)
        .layer(
            CorsLayer::new()
                .allow_origin(Any)
                .allow_methods(Any)
                .allow_headers(Any),
        );

    let addr = format!("0.0.0.0:{}", port);
    let listener = tokio::net::TcpListener::bind(&addr).await.expect("bind");
    println!("[namek_backend] escuchando en http://{} (PostgreSQL: Neon)", addr);
    axum::serve(listener, app).await.expect("serve");
}
