#include "namek.h"

namespace namek {

// ==========================================
// DOCUMENT IMPL
// ==========================================
std::string Document::to_json() const {
    std::ostringstream ss;
    ss << "{\"_id\":\"" << Utils::escape_json(id) << "\"";
    for (const auto& [k, v] : fields) {
        ss << ",\"" << Utils::escape_json(k) << "\":\"" << Utils::escape_json(v) << "\"";
    }
    ss << "}";
    return ss.str();
}

Document Document::from_json(const std::string& json_str) {
    Document doc;
    // Simple robust JSON key-value string extractor for flat documents
    size_t pos = 0;
    while (pos < json_str.length()) {
        size_t key_start = json_str.find('"', pos);
        if (key_start == std::string::npos) break;
        size_t key_end = json_str.find('"', key_start + 1);
        if (key_end == std::string::npos) break;

        std::string key = json_str.substr(key_start + 1, key_end - key_start - 1);
        size_t colon = json_str.find(':', key_end);
        if (colon == std::string::npos) break;

        size_t val_start = json_str.find('"', colon + 1);
        if (val_start == std::string::npos) break;
        size_t val_end = json_str.find('"', val_start + 1);
        if (val_end == std::string::npos) break;

        std::string val = json_str.substr(val_start + 1, val_end - val_start - 1);
        if (key == "_id") {
            doc.id = val;
        } else {
            doc.fields[key] = val;
        }
        pos = val_end + 1;
    }
    return doc;
}

// ==========================================
// NAMEK DB IMPL
// ==========================================
NamekDB::NamekDB(const std::string& filepath) : db_filepath(filepath) {
    load();
}

NamekDB::~NamekDB() {
    save();
}

bool NamekDB::load() {
    if (!Utils::file_exists(db_filepath)) return false;
    std::string content = Utils::read_file(db_filepath);
    return import_json_string(content);
}

bool NamekDB::save() {
    std::string json_data = export_json_string();
    return Utils::write_file(db_filepath, json_data);
}

void NamekDB::set(const std::string& key, const std::string& value) {
    key_value_store[key] = value;
}

std::string NamekDB::get(const std::string& key, const std::string& default_val) {
    auto it = key_value_store.find(key);
    if (it != key_value_store.end()) return it->second;
    return default_val;
}

bool NamekDB::has(const std::string& key) {
    return key_value_store.find(key) != key_value_store.end();
}

bool NamekDB::del(const std::string& key) {
    return key_value_store.erase(key) > 0;
}

std::vector<std::string> NamekDB::keys() {
    std::vector<std::string> res;
    for (const auto& [k, v] : key_value_store) {
        res.push_back(k);
    }
    return res;
}

std::string NamekDB::insert(const std::string& collection_name, const std::unordered_map<std::string, std::string>& fields) {
    Document doc;
    doc.id = Utils::generate_uuid();
    doc.fields = fields;
    collections[collection_name][doc.id] = doc;
    return doc.id;
}

bool NamekDB::update(const std::string& collection_name, const std::string& doc_id, const std::unordered_map<std::string, std::string>& fields) {
    auto col_it = collections.find(collection_name);
    if (col_it == collections.end()) return false;
    auto doc_it = col_it->second.find(doc_id);
    if (doc_it == col_it->second.end()) return false;

    for (const auto& [k, v] : fields) {
        doc_it->second.fields[k] = v;
    }
    return true;
}

bool NamekDB::remove(const std::string& collection_name, const std::string& doc_id) {
    auto col_it = collections.find(collection_name);
    if (col_it == collections.end()) return false;
    return col_it->second.erase(doc_id) > 0;
}

Document NamekDB::get_doc(const std::string& collection_name, const std::string& doc_id) {
    auto col_it = collections.find(collection_name);
    if (col_it != collections.end()) {
        auto doc_it = col_it->second.find(doc_id);
        if (doc_it != col_it->second.end()) {
            return doc_it->second;
        }
    }
    return Document{};
}

std::vector<Document> NamekDB::find(const std::string& collection_name, const std::string& field_name, const std::string& expected_value) {
    std::vector<Document> results;
    auto col_it = collections.find(collection_name);
    if (col_it != collections.end()) {
        for (const auto& [id, doc] : col_it->second) {
            auto field_it = doc.fields.find(field_name);
            if (field_it != doc.fields.end() && field_it->second == expected_value) {
                results.push_back(doc);
            }
        }
    }
    return results;
}

std::vector<Document> NamekDB::find_all(const std::string& collection_name) {
    std::vector<Document> results;
    auto col_it = collections.find(collection_name);
    if (col_it != collections.end()) {
        for (const auto& [id, doc] : col_it->second) {
            results.push_back(doc);
        }
    }
    return results;
}

size_t NamekDB::count(const std::string& collection_name) {
    auto col_it = collections.find(collection_name);
    return (col_it != collections.end()) ? col_it->second.size() : 0;
}

std::vector<std::string> NamekDB::list_collections() {
    std::vector<std::string> cols;
    for (const auto& [col_name, docs] : collections) {
        cols.push_back(col_name);
    }
    return cols;
}

std::string NamekDB::export_json_string() {
    std::ostringstream ss;
    ss << "{\n  \"kv\": {\n";
    bool first_kv = true;
    for (const auto& [k, v] : key_value_store) {
        if (!first_kv) ss << ",\n";
        ss << "    \"" << Utils::escape_json(k) << "\": \"" << Utils::escape_json(v) << "\"";
        first_kv = false;
    }
    ss << "\n  },\n  \"collections\": {\n";
    bool first_col = true;
    for (const auto& [col_name, docs] : collections) {
        if (!first_col) ss << ",\n";
        ss << "    \"" << Utils::escape_json(col_name) << "\": [\n";
        bool first_doc = true;
        for (const auto& [id, doc] : docs) {
            if (!first_doc) ss << ",\n";
            ss << "      " << doc.to_json();
            first_doc = false;
        }
        ss << "\n    ]";
        first_col = false;
    }
    ss << "\n  }\n}";
    return ss.str();
}

bool NamekDB::import_json_string(const std::string& json_data) {
    if (json_data.empty()) return false;
    // Basic parser for KV and Collections
    size_t kv_pos = json_data.find("\"kv\":");
    if (kv_pos != std::string::npos) {
        size_t kv_start = json_data.find('{', kv_pos);
        size_t kv_end = json_data.find('}', kv_start);
        if (kv_start != std::string::npos && kv_end != std::string::npos) {
            std::string kv_block = json_data.substr(kv_start, kv_end - kv_start + 1);
            size_t pos = 0;
            while (pos < kv_block.length()) {
                size_t k_start = kv_block.find('"', pos);
                if (k_start == std::string::npos) break;
                size_t k_end = kv_block.find('"', k_start + 1);
                if (k_end == std::string::npos) break;

                std::string k = kv_block.substr(k_start + 1, k_end - k_start - 1);
                size_t colon = kv_block.find(':', k_end);
                if (colon == std::string::npos) break;

                size_t v_start = kv_block.find('"', colon + 1);
                if (v_start == std::string::npos) break;
                size_t v_end = kv_block.find('"', v_start + 1);
                if (v_end == std::string::npos) break;

                std::string v = kv_block.substr(v_start + 1, v_end - v_start - 1);
                key_value_store[k] = v;
                pos = v_end + 1;
            }
        }
    }
    return true;
}

} // namespace namek
