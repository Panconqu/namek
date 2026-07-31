use namek_core::NamekDB;
use colored::*;
use std::collections::HashMap;

fn main() {
    println!("{}", "=== NAMEK RUST CORE DEMO ===".cyan().bold());

    let mut db = NamekDB::new("demo_rust_db.json");
    db.set("rust_engine", "active");
    db.set("version", "1.0.0");

    println!("✓ Key 'rust_engine': {:?}", db.get("rust_engine"));

    let mut doc_fields = HashMap::new();
    doc_fields.insert("language".to_string(), serde_json::json!("Rust"));
    doc_fields.insert("performance".to_string(), serde_json::json!("Blazing Fast"));

    let doc_id = db.insert_doc("benchmarks", doc_fields);
    println!("{} Document inserted with ID: {}", "✓".green(), doc_id.yellow());
}
