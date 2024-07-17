#ifndef OBJECT_H
#define OBJECT_H

#include "types.h"

typedef enum {
    OT_STRING,
} ObjType;

typedef struct _Obj {
    ObjType type;
    size_t size;
    struct _Obj* next;
} Obj;

typedef struct {
    Obj hdr;
    int len;
    u32 hash;
    char data[];
} ObjString;

extern size_t alloc_bytes;

bool obj_equal(Obj* a, Obj* b);
void print_obj(Obj* obj);

Obj* alloc_obj(ObjType t, size_t size);
void free_obj(Obj* o);

void free_all_obj();

ObjString* create_string(char* str, int len);
ObjString* concat_string(ObjString* a, ObjString* b);

#endif