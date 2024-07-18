#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "vm.h"

size_t alloc_bytes = 0;

bool obj_equal(Obj* a, Obj* b) {
    return a == b;
}

void print_obj(Obj* obj) {
    switch (obj->type) {
        case OT_STRING:
            printf("%s", ((ObjString*) obj)->data);
            break;
    }
}

Obj* alloc_obj(ObjType t, size_t size) {
    Obj* o = malloc(size);
    alloc_bytes += size;
    o->type = t;
    o->size = size;
    o->next = vm.objs;
    vm.objs = o;
    return o;
}

void free_obj(Obj* o) {
    switch (o->type) {
        default:
            break;
    }
    alloc_bytes -= o->size;
    free(o);
}

void free_all_obj() {
    while (vm.objs) {
        Obj* tmp = vm.objs;
        vm.objs = vm.objs->next;
        free_obj(tmp);
    }
}

#define ALLOC_STRING(len)                                                      \
    ((ObjString*) alloc_obj(OT_STRING, sizeof(ObjString) + len + 1))

u32 hash(char* str) {
    u32 hash = 2166136261u;
    for (char* p = str; *p; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

#define HASH_STR(str) str->hash = hash(str->data)

ObjString* create_string(char* str, int len) {
    ObjString* o = ALLOC_STRING(len);
    o->len = len;
    memcpy(o->data, str, len);
    o->data[len] = '\0';
    HASH_STR(o);

    ObjString* intern = table_find_string(&vm.strings, o);
    if (intern) {
        vm.objs = vm.objs->next;
        free_obj((Obj*) o);
        return intern;
    } else {
        table_set(&vm.strings, o, NIL_VAL);
        return o;
    }
}

ObjString* concat_string(ObjString* a, ObjString* b) {
    ObjString* c = ALLOC_STRING(a->len + b->len);
    c->len = a->len + b->len;
    strcpy(c->data, a->data);
    strcat(c->data, b->data);
    HASH_STR(c);
    ObjString* intern = table_find_string(&vm.strings, c);
    if (intern) {
        vm.objs = vm.objs->next;
        free_obj((Obj*) c);
        return intern;
    } else {
        table_set(&vm.strings, c, NIL_VAL);
        return c;
    }
}