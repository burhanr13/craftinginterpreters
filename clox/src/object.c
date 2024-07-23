#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "table.h"
#include "vm.h"

bool obj_equal(Obj* a, Obj* b) {
    return a == b;
}

void fprint_obj(FILE* file, Obj* obj) {
#define printf(...) fprintf(file, __VA_ARGS__)
    switch (obj->type) {
        case OT_STRING:
            printf("%s", ((ObjString*) obj)->data);
            break;
        case OT_FUNCTION:
            if (((ObjFunction*) obj)->name)
                printf("<fn %s>", ((ObjFunction*) obj)->name->data);
            else printf("<anonymous fn>");
            break;
        case OT_CLOSURE:
            fprint_obj(file, (Obj*) ((ObjClosure*) obj)->f);
            break;
        case OT_UPVALUE:
            break;
    }
#undef printf
}

Obj* alloc_obj(ObjType t, size_t size) {
    Obj* o = malloc(size);
    vm.alloc_bytes += size;
    o->type = t;
    o->next = vm.objs;
    vm.objs = o;
    return o;
}

#define ALLOC_OBJ(type, objtype, adlen)                                        \
    (type*) alloc_obj(objtype, sizeof(type) + adlen)

void free_obj(Obj* o) {
    size_t size = 0;
    switch (o->type) {
        case OT_STRING:
            size = sizeof(ObjString) + ((ObjString*) o)->len + 1;
            break;
        case OT_FUNCTION:
            size = sizeof(ObjFunction);
            free(((ObjFunction*) o)->upvalues);
            chunk_free(&((ObjFunction*) o)->chunk);
            break;
        case OT_CLOSURE:
            size = sizeof(ObjClosure);
            free(((ObjClosure*) o)->upvalues);
            break;
        case OT_UPVALUE:
            size = sizeof(OT_UPVALUE);
            break;
    }
    free(o);
    vm.alloc_bytes -= size;
}

void free_all_obj() {
    while (vm.objs) {
        Obj* tmp = vm.objs;
        vm.objs = vm.objs->next;
        free_obj(tmp);
    }
}

#define ALLOC_STRING(len) ALLOC_OBJ(ObjString, OT_STRING, len + 1)

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

ObjFunction* create_function() {
    ObjFunction* func = ALLOC_OBJ(ObjFunction, OT_FUNCTION, 0);
    func->name = NULL;
    func->nargs = 0;
    func->nupvalues = 0;
    func->upvalues = NULL;
    chunk_init(&func->chunk);
    return func;
}

ObjClosure* create_closure(ObjFunction* func) {
    ObjClosure* clos = ALLOC_OBJ(ObjClosure, OT_CLOSURE, 0);
    clos->f = func;
    clos->nupvalues = 0;
    clos->upvalues = NULL;
    return clos;
}

ObjUpvalue* create_upvalue(Value* loc) {
    ObjUpvalue* upval = ALLOC_OBJ(ObjUpvalue, OT_UPVALUE, 0);
    upval->loc = loc;
    upval->next = NULL;
    return upval;
}

void disassemble_function(ObjFunction* func) {
    eprintf("===== begin %s =====\n",
            func->name ? func->name->data : "<anonymous fn>");
    disassemble_chunk(&func->chunk);
    eprintf("===== end %s =====\n",
            func->name ? func->name->data : "<anonymous fn>");
}