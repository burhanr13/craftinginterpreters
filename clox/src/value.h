#ifndef VALUE_H
#define VALUE_H

#include "object.h"
#include "types.h"

typedef enum {
    VT_BOOL,
    VT_NIL,
    VT_NUMBER,
    VT_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        double num;
        bool b;
        Obj* obj;
    };
} Value;

#define NUMBER_VAL(d) ((Value){.type = VT_NUMBER, {.num = d}})
#define NIL_VAL ((Value){.type = VT_NIL})
#define BOOL_VAL(_b) ((Value){.type = VT_BOOL, {.b = _b}})
#define OBJ_VAL(o) ((Value){.type = VT_OBJ, {.obj = o}})

static inline bool isObjType(Value v, ObjType t) {
    return v.type == VT_OBJ && v.obj->type == t;
}

bool value_equal(Value a, Value b);
void print_value(Value v);

#endif