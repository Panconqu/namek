#include "namek_c_api.h"
#include "namek.h"
#include <cstring>
#include <cstdlib>

static char* duplicate_string(const std::string& str) {
    char* res = (char*)malloc(str.length() + 1);
    if (res) {
        std::strcpy(res, str.c_str());
    }
    return res;
}

extern "C" {

NamekDBHandle namek_db_create(const char* filepath) {
    std::string fp = filepath ? filepath : "namek_db.json";
    return new namek::NamekDB(fp);
}

void namek_db_destroy(NamekDBHandle handle) {
    if (handle) {
        delete static_cast<namek::NamekDB*>(handle);
    }
}

void namek_db_set(NamekDBHandle handle, const char* key, const char* value) {
    if (handle && key && value) {
        static_cast<namek::NamekDB*>(handle)->set(key, value);
    }
}

const char* namek_db_get(NamekDBHandle handle, const char* key, const char* default_val) {
    if (!handle || !key) return duplicate_string(default_val ? default_val : "");
    std::string val = static_cast<namek::NamekDB*>(handle)->get(key, default_val ? default_val : "");
    return duplicate_string(val);
}

int namek_db_has(NamekDBHandle handle, const char* key) {
    if (!handle || !key) return 0;
    return static_cast<namek::NamekDB*>(handle)->has(key) ? 1 : 0;
}

int namek_db_del(NamekDBHandle handle, const char* key) {
    if (!handle || !key) return 0;
    return static_cast<namek::NamekDB*>(handle)->del(key) ? 1 : 0;
}

const char* namek_db_insert(NamekDBHandle handle, const char* collection_name, const char* json_fields) {
    if (!handle || !collection_name || !json_fields) return duplicate_string("");
    namek::Document doc = namek::Document::from_json(json_fields);
    std::string id = static_cast<namek::NamekDB*>(handle)->insert(collection_name, doc.fields);
    return duplicate_string(id);
}

const char* namek_db_find_all(NamekDBHandle handle, const char* collection_name) {
    if (!handle || !collection_name) return duplicate_string("[]");
    auto docs = static_cast<namek::NamekDB*>(handle)->find_all(collection_name);
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < docs.size(); ++i) {
        if (i > 0) ss << ",";
        ss << docs[i].to_json();
    }
    ss << "]";
    return duplicate_string(ss.str());
}

int namek_db_save(NamekDBHandle handle) {
    if (!handle) return 0;
    return static_cast<namek::NamekDB*>(handle)->save() ? 1 : 0;
}

int namek_db_load(NamekDBHandle handle) {
    if (!handle) return 0;
    return static_cast<namek::NamekDB*>(handle)->load() ? 1 : 0;
}

const char* namek_generate_uuid() {
    return duplicate_string(namek::Utils::generate_uuid());
}

void namek_free_string(char* ptr) {
    if (ptr) free(ptr);
}

}
