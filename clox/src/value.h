#ifndef VALUE_H
#define VALUE_H

#include <stdio.h>

#include "types.h"

typedef enum {
    VT_BOOL,
    VT_NIL,
    VT_NUMBER,
    VT_CHAR,
    VT_OBJ,
    VT_BUILTIN,
} ValueType;

typedef struct _Value Value;
typedef struct _Obj Obj;
typedef struct _ObjString ObjString;

typedef int (BuiltinFn)(int argc, Value* argv);

typedef struct _Value {
    ValueType type;
    union {
        double num;
        bool b;
        char c;
        Obj* obj;
        BuiltinFn* builtin;
    };
} Value;

#define NUMBER_VAL(d) ((Value){.type = VT_NUMBER, {.num = d}})
#define NIL_VAL ((Value){.type = VT_NIL})
#define BOOL_VAL(_b) ((Value){.type = VT_BOOL, {.b = _b}})
#define CHAR_VAL(_c) ((Value){.type = VT_CHAR, {.c = _c}})
#define OBJ_VAL(o) ((Value){.type = VT_OBJ, {.obj = (Obj*) o}})
#define BUILTIN_VAL(_b) ((Value){.type = VT_BUILTIN, {.builtin = _b}})

bool value_equal(Value a, Value b);
void fprint_value(FILE* file, Value v, bool debug);
#define print_value(v) fprint_value(stdout, v, false)
#define eprint_value(v) fprint_value(stderr, v, true)
ObjString* string_value(Value v);

#endif