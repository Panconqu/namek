# Roadmap de seguridad Namek

Plan de endurecimiento (auditoría + fases de producto). Estado y prioridad por ítem.

## Fase 0 — Corregir la auditoría actual

### 0.1 — Eliminar los 2 RCE por `system()` con path sin sanitizar
- `namek_obfuscator.cpp:374` (`compile_native`) y `namek_toolbox_suite.cpp:318` (`build_monster_cli`):
  reemplazar `system("g++ ...")` por `fork()` + `execvp()` con array de `argv` (sin shell).
  Validar `--output` con whitelist `[A-Za-z0-9_./-]`.

### 0.2 — Matar la inyección vía curl en marketplace y telemetría
- `namek_mod_marketplace.cpp:20,193-199` y `namek_telemetry.cpp:19-28`:
  quitar `system(curl)`/`popen`. Cliente HTTP propio en C++ (socket) o `libcurl` si está
  disponible, sin shell. Credenciales por stdin/env, nunca en argv.
- Firmar `registry.json` (Ed25519) y verificar firma antes de confiar en `download_url`/`sha256`.

### 0.3 — Cerrar el "cifrado" falso
- `tb mod pack` falla si la clave está vacía (`namek_lua.cpp:412` + `namek_binary_format.cpp:156-181`).
- La `release_key` no se embebe en `release_manifest.json` en texto plano; se derivará del
  binario / `NAMEK_KEY` exigida.

### 0.4 — Backend seguro
- `backend/main.rs:492-493`: arranque falla si `ADMIN_PASS` falta o es débil (sin default admin).
- Redactar secretos en events (nunca loguear valores de `set api key`/passwords) — `cli/main.cpp:184-192`.
- Rate-limit en `/login` y `/events`; `detail` con límite de tamaño (`backend/main.rs:319-340`).
- Temp bodies con `0600` (`namek_telemetry.cpp:23-27`).

### 0.5 — Sandbox
- `namek.db_get`/`db_set` apuntan a una DB aislada del mod, no a la del motor (`namek_lua.cpp:33-53`).
- Quitar auto-ejecución de mods en comandos desconocidos (`namek_syntax.cpp:396-401`): solo `tb mod load`.

### 0.6 — Crypto
- Solo `getrandom`, sin fallback `mt19937` para secretos (`namek_crypto.cpp:254-264`).
- Validar `payload_size` contra tamaño real del archivo y un máximo (`namek_binary_format.cpp:133`).

## Fase 1 — Framework / DSL (syntax 2.0) — Python, JS, C++
- `tb init` real (`cli/main.cpp:254-273` hoy es fake): genera estructura real (`src/`, `tests/`,
  `.tbconfig`, `README`) con plantillas Python/JS/C++.
- `tb connect api <nombre> --url <url> [--auth]` → guarda la conexión en `.tbconfig` y genera un
  cliente listo en el idioma elegido.
- `tb env set/get/list` — gestión de variables + carga de `.env`.
- `tb task add/run` — task runner (`tb task add build "cmd"`).
- `tb gen <tipo>` — boilerplate de mods/componentes/CLIs.
- `tb help <verbo>` + tab-completion en el REPL.

## Fase 2 — MV v3 (máquina virtual real)
- Bytecode con opcodes propios (no hex-XOR legible): `namek_obfuscator.cpp:79-133` se reescribe.
- Claves derivadas en runtime desde un seed (SHA-256), nunca impresas (`#NK-KEY` desaparece).
- Capas polimórficas: algoritmo distinto por capa (XOR → substitution → custom).
- Anti-tamper: checksum del payload antes de ejecutar; anti-debug en JS (ya hay).
