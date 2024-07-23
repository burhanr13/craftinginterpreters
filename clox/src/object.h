#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>

#include "chunk.h"
#include "types.h"

typedef enum {
    OT_STRING,
    OT_FUNCTION,
} ObjType;

typedef struct _Obj {
    ObjType type;
    struct _Obj* next;
} Obj;

typedef struct {
    Obj hdr;
    int len;
    u32 hash;
    char data[];
} ObjString;

typedef struct {
    Obj hdr;
    ObjString* name;
    int nargs;
    Chunk chunk;
} ObjFunction;

bool obj_equal(Obj* a, Obj* b);
void fprint_obj(FILE* file, Obj* obj);
#define print_obj(obj) fprint_obj(stdout,obj)
#define eprint_obj(obj) fprint_obj(stderr, obj)

Obj* alloc_obj(ObjType t, size_t size);
void free_obj(Obj* o);

void free_all_obj();

ObjString* create_string(char* str, int len);
#define CREATE_STRING_LITERAL(str) create_string(str, sizeof str - 1)
ObjString* concat_string(ObjString* a, ObjString* b);

ObjFunction* create_function();

void disassemble_function(ObjFunction* func);

#endif