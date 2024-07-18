#include "table.h"

#include <string.h>

void table_init(Table* t) {
    t->size = 0;
    t->cap = 0;
    t->ents = NULL;
}

void table_free(Table* t) {
    free(t->ents);
}

Entry* find_entry(Table* t, ObjString* key) {
    if (!t->cap) return NULL;
    Entry* tombstone = NULL;
    int idx = key->hash % t->cap;
    for (;; idx = (idx + 1) % t->cap) {
        if (!t->ents[idx].key) {
            if (t->ents[idx].value.b) {
                tombstone = tombstone ? tombstone : &t->ents[idx];
            } else {
                return tombstone ? tombstone : &t->ents[idx];
            }
        } else if (t->ents[idx].key == key) {
            return &t->ents[idx];
        }
    }
}

static void resize(Table* t, size_t newcap) {
    size_t oldcap = t->cap;
    Entry* oldents = t->ents;
    t->ents = calloc(newcap, sizeof(Entry));
    t->cap = newcap;
    t->size = 0;
    for (int i = 0; i < oldcap; i++) {
        if (oldents[i].key) {
            table_set(t, oldents[i].key, oldents[i].value);
        }
    }
    free(oldents);
}

bool table_set(Table* t, ObjString* key, Value val) {
    if (t->size + 1 > t->cap * LOAD_FACTOR) {
        resize(t, t->cap ? 2 * t->cap : 8);
    }
    Entry* e = find_entry(t, key);
    bool newKey = !e->key;
    if (!e->key && !e->value.b) {
        t->size++;
    }
    e->key = key;
    e->value = val;
    return newKey;
}

void table_add_all(Table* dest, Table* src) {
    for (int i = 0; i < src->cap; i++) {
        if (src->ents[i].key) {
            table_set(dest, src->ents[i].key, src->ents[i].value);
        }
    }
}

bool table_get(Table* t, ObjString* key, Value* val) {
    Entry* e = find_entry(t, key);
    if (e && e->key) {
        *val = e->value;
        return true;
    } else {
        return false;
    }
}

bool table_delete(Table* t, ObjString* key) {
    Entry* e = find_entry(t, key);
    if (!e || !e->key) return false;
    e->key = NULL;
    e->value = BOOL_VAL(true);
    return true;
}

ObjString* table_find_string(Table* t, ObjString* key) {
    if (!t->cap) return NULL;
    int idx = key->hash % t->cap;
    for (;; idx = (idx + 1) % t->cap) {
        if (!t->ents[idx].key) {
            if (!t->ents[idx].value.b) return NULL;
        } else if (t->ents[idx].key->hash == key->hash &&
                   !strcmp(t->ents[idx].key->data, key->data)) {
            return t->ents[idx].key;
        }
    }
}