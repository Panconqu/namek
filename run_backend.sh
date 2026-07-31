#!/usr/bin/env bash
# Arranca el backend de cuentas/auditoría (Rust) usando las credenciales del .env
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f .env ]; then
    echo "ERROR: falta el archivo .env (pide uno con cp .env.example .env)" >&2
    exit 1
fi
if grep -q "<db_password>" .env; then
    echo "ERROR: rellena MONGODB_URI en .env con tu contraseña real (no la compartas en chats)." >&2
    exit 1
fi

export PATH="$HOME/.cargo/bin:$PATH"
export $(grep -v '^#' .env | xargs)
exec cargo run --release --manifest-path backend/Cargo.toml
