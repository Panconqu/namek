import os
import json
import uuid
import ctypes
from typing import Dict, Any, List, Optional

class NamekDB:
    """
    Motor de base de datos NoSQL ultrarrápido NamekDB para Python.
    Soporta almacenamiento Llave-Valor y Colecciones de Documentos JSON.
    """
    def __init__(self, filepath: str = "namek_db.json"):
        self.filepath = filepath
        self.kv_store: Dict[str, str] = {}
        self.collections: Dict[str, Dict[str, Dict[str, Any]]] = {}
        self.load()

    def load(self) -> bool:
        if not os.path.exists(self.filepath):
            return False
        try:
            with open(self.filepath, "r", encoding="utf-8") as f:
                data = json.load(f)
                self.kv_store = data.get("kv", {})
                self.collections = data.get("collections", {})
            return True
        except Exception:
            return False

    def save(self) -> bool:
        try:
            data = {
                "kv": self.kv_store,
                "collections": self.collections
            }
            with open(self.filepath, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            return True
        except Exception:
            return False

    # --- KEY-VALUE API ---
    def set(self, key: str, value: Any) -> None:
        self.kv_store[key] = str(value)
        self.save()

    def get(self, key: str, default: Optional[str] = None) -> Optional[str]:
        return self.kv_store.get(key, default)

    def delete(self, key: str) -> bool:
        if key in self.kv_store:
            del self.kv_store[key]
            self.save()
            return True
        return False

    def keys(self) -> List[str]:
        return list(self.kv_store.keys())

    # --- NOSQL DOCUMENT STORE API ---
    def insert(self, collection: str, document: Dict[str, Any]) -> str:
        if collection not in self.collections:
            self.collections[collection] = {}
        doc_id = str(uuid.uuid4())
        doc_data = {"_id": doc_id, **document}
        self.collections[collection][doc_id] = doc_data
        self.save()
        return doc_id

    def find_one(self, collection: str, doc_id: str) -> Optional[Dict[str, Any]]:
        return self.collections.get(collection, {}).get(doc_id)

    def find(self, collection: str, query: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
        col = self.collections.get(collection, {})
        if not query:
            return list(col.values())
        
        results = []
        for doc in col.values():
            match = True
            for k, v in query.items():
                if doc.get(k) != v:
                    match = False
                    break
            if match:
                results.append(doc)
        return results

    def remove(self, collection: str, doc_id: str) -> bool:
        if collection in self.collections and doc_id in self.collections[collection]:
            del self.collections[collection][doc_id]
            self.save()
            return True
        return False

    def count(self, collection: str) -> int:
        return len(self.collections.get(collection, {}))
