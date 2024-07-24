#include "value.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "object.h"

bool value_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VT_NUMBER:
            return a.num == b.num;
        case VT_BOOL:
            return a.b == b.b;
        case VT_NIL:
            return true;
        case VT_CHAR:
            return a.c == b.c;
            break;
        case VT_OBJ:
            return obj_equal(a.obj, b.obj);
        case VT_BUILTIN:
            return a.builtin == b.builtin;
    }
    return false;
}

void fprint_value(FILE* file, Value v, bool debug) {
#define printf(...) fprintf(file, __VA_ARGS__)
    switch (v.type) {
        case VT_NUMBER:
            if (v.num == (int) v.num) printf("%d", (int) v.num);
            else printf("%f", v.num);
            break;
        case VT_NIL:
            printf("nil");
            break;
        case VT_BOOL:
            printf("%s", v.b ? "true" : "false");
            break;
        case VT_CHAR:
            if (debug) printf("'%c'", v.c);
            else printf("%c", v.c);
            break;
        case VT_OBJ:
            fprint_obj(file, v.obj, debug);
            break;
        case VT_BUILTIN:
            printf("<builtin fn>");
            break;
    }
#undef printf
}

ObjString* string_value(Value v) {
    switch (v.type) {
        case VT_NUMBER: {
            char buf[20];
            if (v.num == (int) v.num) snprintf(buf, 20, "%d", (int) v.num);
            else snprintf(buf, 20, "%f", v.num);
            return create_string(buf, strlen(buf));
            break;
        }
        case VT_NIL:
            return CREATE_STRING_LITERAL("nil");
            break;
        case VT_BOOL:
            return v.b ? CREATE_STRING_LITERAL("true")
                       : CREATE_STRING_LITERAL("false");
            break;
        case VT_CHAR:
            return create_string(&v.c, 1);
        case VT_OBJ:
            return CREATE_STRING_LITERAL("<obj>");
            break;
        default:
            return CREATE_STRING_LITERAL("<value>");
    }
}