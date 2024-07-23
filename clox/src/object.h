#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>

#include "chunk.h"
#include "types.h"
#include "value.h"

typedef enum {
    OT_STRING,
    OT_FUNCTION,
    OT_CLOSURE,
    OT_UPVALUE,
} ObjType;

typedef struct _Obj {
    ObjType type;
    struct _Obj* next;
} Obj;

typedef struct _ObjString {
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
    struct {
        u8 id;
        bool local;
    }* upvalues;
    int nupvalues;
} ObjFunction;

typedef struct _ObjUpvalue {
    Obj hdr;
    Value* loc;
    Value closed;
    struct _ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj hdr;
    ObjFunction* f;
    ObjUpvalue** upvalues;
    int nupvalues;
} ObjClosure;

static inline bool isObjType(Value v, ObjType t) {
    return v.type == VT_OBJ && v.obj->type == t;
}
bool obj_equal(Obj* a, Obj* b);
void fprint_obj(FILE* file, Obj* obj);
#define print_obj(obj) fprint_obj(stdout, obj)
#define eprint_obj(obj) fprint_obj(stderr, obj)

Obj* alloc_obj(ObjType t, size_t size);
void free_obj(Obj* o);

void free_all_obj();

ObjString* create_string(char* str, int len);
#define CREATE_STRING_LITERAL(str) create_string(str, sizeof str - 1)
ObjString* concat_string(ObjString* a, ObjString* b);

ObjFunction* create_function();
ObjClosure* create_closure(ObjFunction* func);
ObjUpvalue* create_upvalue(Value* loc);

void disassemble_function(ObjFunction* func);

#endif