#!/usr/bin/env python3
import sys
import os

# Include local SDK path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../sdk/python")))

from namek import NamekDB, CLIBuilder, UUID, Hash, HTTP, TextUtils

def main():
    print("\033[36m\033[1m=== NAMEK PYTHON TOOLBOX DEMO ===\033[0m\n")

    # 1. NamekDB NoSQL Engine Demo
    db = NamekDB("demo_db.json")
    db.set("app_name", "Super CLI Tool")
    db.set("version", "2.5.0")

    doc_id = db.insert("users", {
        "name": "Alice Johnson",
        "role": "Lead Architect",
        "email": "alice@example.com"
    })
    print(f"✓ Documento insertado en colección 'users' con ID: \033[33m{doc_id}\033[0m")

    users = db.find("users")
    print(f"✓ Usuarios encontrados en NamekDB: {len(users)}")
    for u in users:
        print(f"  - {u['name']} ({u['role']})")

    # 2. Developer Tools Demo
    print("\n\033[32m\033[1m=== UTILITIES DEMO ===\033[0m")
    uuid_val = UUID.v4()
    sha = Hash.sha256("Namek Framework")
    slug = TextUtils.slugify("Namek CLI Toolbox for Developers")
    
    print(f"UUID v4: \033[36m{uuid_val}\033[0m")
    print(f"SHA256 ('Namek Framework'): \033[35m{sha}\033[0m")
    print(f"Slugify: \033[33m{slug}\033[0m")

if __name__ == "__main__":
    main()
