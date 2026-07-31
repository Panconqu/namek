#ifndef NAMEK_C_API_H
#define NAMEK_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

// Pointer handle to NamekDB instance
typedef void* NamekDBHandle;

NamekDBHandle namek_db_create(const char* filepath);
void namek_db_destroy(NamekDBHandle handle);

void namek_db_set(NamekDBHandle handle, const char* key, const char* value);
const char* namek_db_get(NamekDBHandle handle, const char* key, const char* default_val);
int namek_db_has(NamekDBHandle handle, const char* key);
int namek_db_del(NamekDBHandle handle, const char* key);

const char* namek_db_insert(NamekDBHandle handle, const char* collection_name, const char* json_fields);
const char* namek_db_find_all(NamekDBHandle handle, const char* collection_name);

int namek_db_save(NamekDBHandle handle);
int namek_db_load(NamekDBHandle handle);

const char* namek_generate_uuid();
void namek_free_string(char* ptr);

#ifdef __cplusplus
}
#endif

#endif // NAMEK_C_API_H
