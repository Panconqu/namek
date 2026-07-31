#!/usr/bin/env bash
# publish.sh — añade un Mod al catálogo y lo publica en GitHub.
#
# Uso:
#   ./publish.sh <archivo.lua> [version] [descripcion] [autor]
#
# Ejemplo:
#   ./publish.sh mods_catalog/mods/mi_mod.lua 1.0.0 "Mi primer mod" "Nombre"
#
# Requiere: gh autenticado y push access al repo.
set -euo pipefail

MOD_FILE="${1:?Uso: ./publish.sh <archivo.lua> [version] [descripcion] [autor]}"
VERSION="${2:-1.0.0}"
DESC="${3:-Sin descripción}"
AUTHOR="${4:-$(git config user.name || echo 'Namek Community')}"

if [ ! -f "$MOD_FILE" ]; then
    echo "Error: archivo '$MOD_FILE' no existe" >&2
    exit 1
fi

NAME="$(basename "$MOD_FILE" .lua)"
DIR="$(cd "$(dirname "$0")" && pwd)"
REGISTRY="$DIR/mods_catalog/registry.json"
mkdir -p "$DIR/mods_catalog/mods"
cp "$MOD_FILE" "$DIR/mods_catalog/mods/$NAME.lua"

SHA="$(sha256sum "$DIR/mods_catalog/mods/$NAME.lua" | cut -d' ' -f1)"
URL="https://raw.githubusercontent.com/Panconqu/namek/main/mods_catalog/mods/$NAME.lua"

python3 - "$NAME" "$VERSION" "$DESC" "$AUTHOR" "$URL" "$SHA" <<'EOF'
import json, sys
name, version, desc, author, url, sha = sys.argv[1:7]
path = "mods_catalog/registry.json"
with open(path) as f:
    reg = json.load(f)
entry = {
    "name": name,
    "version": version,
    "description": desc,
    "author": author,
    "download_url": url,
    "sha256": sha,
}
for i, e in enumerate(reg):
    if e["name"] == name:
        reg[i] = entry
        break
else:
    reg.append(entry)
with open(path, "w") as f:
    json.dump(reg, f, indent=2, ensure_ascii=False)
    f.write("\n")
print(f"registry.json actualizado: {name} v{version} (sha256 {sha[:16]}...)")
EOF

cd "$DIR"
git add mods_catalog/registry.json "mods_catalog/mods/$NAME.lua"
git commit -m "mods_catalog: publicar $NAME v$VERSION"
git push origin main

echo "✓ Mod '$NAME' publicado: $URL"
