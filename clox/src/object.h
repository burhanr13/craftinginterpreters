#ifndef OBJECT_H
#define OBJECT_H

#include <stdio.h>

#include "chunk.h"
#include "table.h"
#include "types.h"
#include "value.h"

typedef enum {
    OT_STRING,
    OT_FUNCTION,
    OT_CLOSURE,
    OT_UPVALUE,
    OT_CLASS,
    OT_INSTANCE,
    OT_ARRAY,
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

typedef struct {
    Obj hdr;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct {
    Obj hdr;
    ObjClass* cls;
    Table attrs;
} ObjInstance;

typedef struct {
    Obj hdr;
    size_t len;
    Value data[];
} ObjArray;

static inline bool
isObjType(Value v, ObjType t) {
    return v.type == VT_OBJ && v.obj->type == t;
}
bool obj_equal(Obj* a, Obj* b);
void fprint_obj(FILE* file, Obj* obj, bool debug);
#define print_obj(obj) fprint_obj(stdout, obj, false)
#define eprint_obj(obj) fprint_obj(stderr, obj, true)

Obj* alloc_obj(ObjType t, size_t size);
void free_obj(Obj* o);

void mark_obj(Obj* o);
void mark_table(Table* t);

void collect_garbage();
void free_all_obj();

ObjString* create_string(char* str, int len);
#define CREATE_STRING_LITERAL(str) create_string(str, sizeof str - 1)
ObjString* concat_string(ObjString* a, ObjString* b);

ObjFunction* create_function();
ObjClosure* create_closure(ObjFunction* func);
ObjUpvalue* create_upvalue(Value* loc);

ObjClass* create_class(ObjString* name);
ObjInstance* create_instance(ObjClass* cls);

ObjArray* create_array(size_t len);
ObjArray* create_array_full(size_t len, Value* vals);

void disassemble_function(ObjFunction* func);

#endif