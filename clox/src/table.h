#ifndef TABLE_H
#define TABLE_H

#include <stdlib.h>

#include "types.h"
#include "object.h"
#include "value.h"

#define LOAD_FACTOR 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    size_t size;
    size_t cap;
    Entry* ents;
} Table;

void table_init(Table* t);
void table_free(Table* t);

void table_set(Table* t, ObjString* key, Value val);
bool table_get(Table* t, ObjString* key, Value* val);
bool table_delete(Table* t, ObjString* key);

ObjString* table_find_string(Table* t, ObjString* key);

void table_add_all(Table* dest, Table* src);

#endif
