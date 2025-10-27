#include "../console.h"

VarEntry var_table[MAX_HASH_VARS];
size_t var_count = 0;

uint64_t hash_str(const char* s) {
    uint64_t hash = 1469598103934665603ULL;
    while (*s) {
        hash ^= (unsigned char)*s++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Finds or inserts a variable by name.
// Returns index in var_table or -1 if full.
int find_or_insert_var(const char* name) {
    uint64_t hash = hash_str(name);
    size_t idx = hash % MAX_HASH_VARS;
    size_t start = idx;

    do {
        VarEntry* e = &var_table[idx];
        if (!e->name) {
            // empty slot → insert
            size_t len = strlen(name) + 1;
            e->name = malloc(len);
            memcpy(e->name, name, len);
            e->value = NULL;
            var_count++;
            return (int)idx;
        }
        else if (!strcmp(e->name, name)) {
            // found existing
            return (int)idx;
        }

        idx = (idx + 1) % MAX_HASH_VARS;
    } while (idx != start);

    return -1; // table full
}

int find_var(const char* name) {
    uint64_t hash = hash_str(name);
    size_t idx = hash % MAX_HASH_VARS;
    size_t start = idx;
    do {
        VarEntry* e = &var_table[idx];
        if (!e->name)
            return -1; // not found
        if (!strcmp(e->name, name))
            return (int)idx;
        idx = (idx + 1) % MAX_HASH_VARS;
    } while (idx != start);

    return -1;
}