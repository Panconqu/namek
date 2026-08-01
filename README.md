# Namek Framework & Developer Toolbox

**Namek** es un ecosistema multi-lenguaje de alto rendimiento diseñado para construir herramientas CLI avanzadas, APIs y servicios de datos. Cuenta con **Motor de Mods en Lua 5.3**, **Sintaxis DSL `tb` / `toolbox`**, **Motor de Ofuscación de 5 Capas (MV)** con **bytecode armado**, y **Empaquetado de Distribución `.tb.bin` v2** (ChaCha20 + SHA-256).

---

## 🌙 Sistema de Mods en Lua 5.3 (`tb mod`)

Namek integra un motor incrustado de **Lua 5.3** que permite cargar, escribir y ejecutar plugins/mods dinámicos con acceso a toda la API nativa de Namek.

### Comandos de Gestión de Mods:

```bash
# Crear un nuevo Mod a partir de una plantilla
tb mod create mi_primer_mod

# Listar todos los Mods instalados en mods/
tb mod list

# Buscar e instalar Mods remotos desde el catálogo GitHub
tb mod search
tb mod install security_sentinel --yes

# Cargar y ejecutar un Mod específico
tb mod run mods/mi_primer_mod.lua

# Cargar y ejecutar TODOS los Mods en mods/ automáticamente
tb mod load

# Cifrar un Mod para que su código no sea legible en disco (.tb.bin ChaCha20)
tb mod pack mods/mi_primer_mod.lua
tb mod run mods/mi_primer_mod.lua.tb.bin
```

### Ejemplo de un Mod de Lua (`mods/sample_mod.lua`):

```lua
-- Namek Lua Mod API Example
namek.print("=== MOD DE LUA INICIALIZADO ===")

-- Generar UUIDs y codificación Base64 desde Lua
local session_id = namek.uuid()
local b64_token = namek.base64_encode("SecretData")
namek.print("Session ID: " .. session_id)
namek.print("Base64 Token: " .. b64_token)

-- Manipular la Base de Datos NoSQL NamekDB desde Lua
namek.db_set("lua_mod_status", "active")
local status = namek.db_get("lua_mod_status")
namek.print("Estado NamekDB: " .. status)

-- Generación de Datos Ficticios
local user_name = namek.fake_name()
local user_email = namek.fake_email()
namek.print("Operador: " .. user_name .. " (" .. user_email .. ")")
```

### 🔐 Modelo de Seguridad: Open-Close (modificar el motor sin extraerlo)

Los mods corren en un **sandbox duro**: pueden **modificar el motor** (abierto) pero
**no pueden extraer su código ni leer archivos** (cerrado).

| Capacidad | Estado |
|---|---|
| Modificar el motor: hooks, comandos nuevos, config runtime | ✅ Permitido vía API `namek.*` |
| Acceder a información del motor (`namek.info()`) | ✅ Solo lectura |
| Leer/escribir archivos (`io`), shell (`os`), `require`/`package` | 🚫 Bloqueado |
| `load`/`loadstring`/`dofile`/`loadfile` (ejecutar código arbitrario) | 🚫 Bloqueado |
| `debug` (inspección de memoria/Lua interno) | 🚫 Bloqueado |
| Escapar del entorno a `_G` / tablas internas del motor | 🚫 Bloqueado (whitelist `_ENV`) |
| Consumo de CPU (loops infinitos) | ⚠️ Cortado por cuota de instrucciones |

El entorno se construye con **whitelist** (no blacklist): el `_ENV` de cada mod es una
tabla vacía cuyo `__index` apunta a un conjunto cerrado de globals seguras. Todo lo demás
(`io`, `os`, `package`, `require`, `debug`, `load*`, `rawget/set`, `getfenv/setfenv`,
`collectgarbage`) es inalcanzable por diseño, no solo borrado.

### 🛠️ API para modificar el motor desde un Mod

```lua
-- Registrar un hook: se dispara cuando el motor ejecuta cualquier comando
namek.hook("on_command", function(event, command)
    namek.print("[HOOK] el motor ejecutó: " .. command)
end)

-- Registrar un comando NUEVO del motor: tb scan_mod <args...>
namek.register_command("scan_mod", function(cmd, args)
    namek.print("Argumentos: " .. table.concat(args, ", "))
end)

-- Modificar la configuración del motor en runtime
namek.set_config("engine.verbose", "true")
namek.print(namek.get_config("engine.verbose"))

-- Información de solo lectura del motor
local info = namek.info()
print(info.engine .. " v" .. info.version)   -- NamekToolbox v1.0.0

-- Utilidades de criptografía (info, sin tocar el motor)
namek.sha256("texto") / namek.md5("texto") / namek.base64_encode / namek.base64_decode
```

Los mods se autocargan una sola vez por sesión: si un mod registra comandos, esos
comandos quedan disponibles en el motor (`tb <comando_del_mod>`).

### 📦 Mods cifrados

`tb mod pack` envuelve el código Lua en el formato binario v2 (ChaCha20 + SHA-256).
El `.tb.bin` resultante no es legible en disco, se descifra **en memoria** al cargarlo
y se ejecuta dentro del mismo sandbox. Útil para distribuir mods sin exponer su código
fuente a inspección local.

---

## 👤 Sistema de cuentas y auditoría del motor (`backend/`)

El motor puede registrar qué **acciones del motor** ejecuta cada usuario (qué comando,
qué método intentó tocar y si funcionó o no) — **sin tocar el entorno del usuario**.
Para eso hay un backend propio en **Rust** (axum + driver oficial de MongoDB) que es el
**único** que habla con MongoDB Atlas; el cliente C++ nunca lleva la URI de la DB.

### Backend (`namek_backend`)

- `POST /register` y `POST /login` — password hasheada con **Argon2**; a cada sesión se
  emite un **token aleatorio de 256 bits** (guardado como SHA-256 en la DB).
- `POST /events` — log de eventos del motor (evento, método, éxito/fallo, detalle).
- `GET /admin/users`, `GET /admin/events`, `POST /admin/ban`, `POST /admin/unban` —
  solo con credenciales admin (Basic auth `ADMIN_USER`/`ADMIN_PASS`).
- Al **banear** un usuario se **revocan todas sus sesiones** (sus tokens se borran).

### Arranque

```bash
cp .env.example .env   # rellena DATABASE_URL (tu string de PostgreSQL) + SERVER_SECRET + admin
./run_backend.sh       # compila y levanta el backend en http://localhost:8787
```

> ⚠️ La URI de PostgreSQL y las credenciales admin viven SOLO en `.env` (gitignored).
> Nunca se commitearon ni deben pegarse en chats.

### Cliente C++ (CLI)

```bash
namek register pepe --pass *****   # primera vez: pide registro/login en el REPL
namek login pepe --pass *****      # genera token de sesión en ~/.namek_session (0600)
namek me                           # estado de tu cuenta (¿baneado?)
namek logout                       # revoca el token

namek admin users                  # panel: lista de usuarios + nº de eventos
namek admin events --limit 50 --user pepe
namek admin ban pepe               # banea y revoca sesiones
namek admin unban pepe
```

Los eventos del motor se auditan de forma **best-effort** (si el backend está caído, el
motor sigue funcionando sin bloquear): `command`, `mod_run`, `mod_pack`, `mod_install`
y `sandbox_block` (cuando un mod intenta tocar una ruta prohibida).

---

## 📦 Empaquetado Release Binario (`tb pack`)

```bash
tb pack dist                 # empaquetar en dist/ con clave aleatoria
tb pack dist --key <hex64>   # usar una clave de release fija
tb pack dist --name "Bundle" --layers 5 --skip-runtimes
```

Genera la estructura de distribución con los **3 binarios `.tb.bin` v2 cifrados** y los ejecutables MV de 5 capas:

```
dist/
├── bin/ (namek_runtime, tb)
├── modules/ (core.tb.bin, toolbox.tb.bin, mv_engine.tb.bin)
├── mv_runtimes/ (python_mv_runner.py, node_mv_runner.js)
└── release_manifest.json  (clave de release + orden de carga)
```

Cada `.tb.bin` v2 usa:
- **Cifrado ChaCha20** con clave derivada `SHA-256(salt + release_key)` (salt y nonce aleatorios por archivo).
- **Magic por tipo de módulo** (`NKCR` core, `NKTB` toolbox, `NKMV` mv_engine).
- **Integridad SHA-256** sobre el ciphertext (detección de manipulación).

## 🚀 Boot del Release (`tb boot`)

El runtime **carga, valida y ejecuta los 3 módulos binarios en orden** (core → toolbox → mv_engine):

```bash
tb boot                     # boot explícito (busca modules/)
cd dist && ./bin/tb         # auto-boot al ejecutar desde el bundle
cd dist && ./bin/namek_runtime
```

También puedes ejecutar un solo binario: `tb run dist/modules/core.tb.bin`.
La clave se resuelve desde `release_manifest.json`, la variable de entorno `NAMEK_KEY`, o falla con un error claro si no existe.

---

## 🔒 Motor de Ofuscación Nivel 5 & MV Personalizada

```bash
# Ofuscar a Nivel MV (Virtual Machine) con 5 capas de bytecode armado
tb obfuscate script.py --level mv --layers 5 --output script_mv.py

# Desofuscar todas las capas en memoria
tb deobfuscate script_mv.py --output script_decompiled.py

# Niveles alternativos: high (string XOR), compile (binario nativo C++)
tb obfuscate script.py --level high --output script_high.py
tb obfuscate script.py --level compile --output script_bin
```

La MV v2 genera bytecode **armado** con claves aleatorias por ejecución (byte key + armor key), empaquetado en hex compacto, con soporte de **N capas anidadas** y guard anti-debug para Python.

---

## 🧰 Toolbox DSL ampliado

```bash
# Criptografía
tb crypto sha256 "texto"        tb crypto md5 "texto"
tb crypto base64 "texto"        tb crypto hex "texto"
tb crypto xor "texto" "clave"   tb crypto key

# Datos sintéticos
tb data fake                    tb data fake-ip
tb data jwt '{"sub":"u1"}'      tb data format '{"a":1,"b":[1,2]}'

# Red
tb net ip
tb net scan 127.0.0.1 8080
tb net benchmark https://example.com 50
```

---

## 🛠️ Compilación

```bash
cmake -S . -B build && cmake --build build
# Salida: bin/namek (CLI) y libnamek.so (C API / FFI)
```

Requisitos: CMake ≥ 3.10, compilador C++17, `lua5.3` (dev headers).
