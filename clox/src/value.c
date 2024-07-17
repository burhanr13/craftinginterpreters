#include "value.h"

#include <stdio.h>
bool value_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch(a.type) {
        case VT_NUMBER:
            return a.num == b.num;
        case VT_BOOL:
            return a.b == b.b;
        case VT_NIL:
            return true;
        case VT_OBJ:
            return obj_equal(a.obj, b.obj);
        default:
            return false;
    }
}

void print_value(Value v) {
    switch(v.type) {
        case VT_NUMBER:
            printf("%f", v.num);
            break;
        case VT_NIL:
            printf("nil");
            break;
        case VT_BOOL:
            printf("%s", v.b ? "true" : "false");
            break;
        case VT_OBJ:
            print_obj(v.obj);
            break;
    }
}
