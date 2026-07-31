use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use uuid::Uuid;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Document {
    pub id: String,
    pub fields: HashMap<String, serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct NamekDB {
    pub filepath: String,
    pub kv: HashMap<String, String>,
    pub collections: HashMap<String, HashMap<String, Document>>,
}

impl NamekDB {
    pub fn new(filepath: &str) -> Self {
        let mut db = NamekDB {
            filepath: filepath.to_string(),
            kv: HashMap::new(),
            collections: HashMap::new(),
        };
        db.load();
        db
    }

    pub fn load(&mut self) -> bool {
        if Path::new(&self.filepath).exists() {
            if let Ok(content) = fs::read_to_string(&self.filepath) {
                if let Ok(data) = serde_json::from_str::<serde_json::Value>(&content) {
                    if let Some(kv_obj) = data.get("kv").and_then(|v| v.as_object()) {
                        for (k, v) in kv_obj {
                            if let Some(str_v) = v.as_str() {
                                self.kv.insert(k.clone(), str_v.to_string());
                            }
                        }
                    }
                    return true;
                }
            }
        }
        false
    }

    pub fn save(&self) -> bool {
        let data = serde_json::json!({
            "kv": self.kv,
            "collections": self.collections,
        });
        if let Ok(json_str) = serde_json::to_string_pretty(&data) {
            return fs::write(&self.filepath, json_str).is_ok();
        }
        false
    }

    pub fn set(&mut self, key: &str, value: &str) {
        self.kv.insert(key.to_string(), value.to_string());
        self.save();
    }

    pub fn get(&self, key: &str) -> Option<&String> {
        self.kv.get(key)
    }

    pub fn insert_doc(&mut self, collection: &str, fields: HashMap<String, serde_json::Value>) -> String {
        let doc_id = Uuid::new_v4().to_string();
        let doc = Document {
            id: doc_id.clone(),
            fields,
        };
        self.collections
            .entry(collection.to_string())
            .or_insert_with(HashMap::new)
            .insert(doc_id.clone(), doc);
        self.save();
        doc_id
    }
}
