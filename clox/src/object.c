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

void fprint_obj(FILE* file, Obj* obj, bool debug) {
#define printf(...) fprintf(file, __VA_ARGS__)
    switch (obj->type) {
        case OT_STRING:
            if (debug) printf("\"%s\"", ((ObjString*) obj)->data);
            else printf("%s", ((ObjString*) obj)->data);
            break;
        case OT_FUNCTION:
            if (((ObjFunction*) obj)->name)
                printf("<fn %s>", ((ObjFunction*) obj)->name->data);
            else printf("<anonymous fn>");
            break;
        case OT_CLOSURE:
            if (debug) printf("<closure ");
            fprint_obj(file, (Obj*) ((ObjClosure*) obj)->f, debug);
            if (debug) printf(">");
            break;
        case OT_UPVALUE:
            printf("<upvalue>");
            break;
        case OT_CLASS:
            printf("<class %s>", ((ObjClass*) obj)->name->data);
            break;
        case OT_INSTANCE:
            printf("<instance %s>", ((ObjInstance*) obj)->cls->name->data);
            break;
        case OT_ARRAY: {
            ObjArray* arr = (ObjArray*) obj;
            printf("[");
            for (int i = 0; i < arr->len; i++) {
                fprint_value(file, arr->data[i], debug);
                if (i < arr->len - 1) printf(",");
            }
            printf("]");
        }
    }
#undef printf
}

void eprint_objtype(ObjType t) {
    switch (t) {
        case OT_STRING:
            eprintf("String");
            break;
        case OT_FUNCTION:
            eprintf("Function");
            break;
        case OT_CLOSURE:
            eprintf("Closure");
            break;
        case OT_UPVALUE:
            eprintf("Upvalue");
            break;
        case OT_CLASS:
            eprintf("Class");
            break;
        case OT_INSTANCE:
            eprintf("Instance");
            break;
        case OT_ARRAY:
            eprintf("Array");
            break;
    }
}

Obj* alloc_obj(ObjType t, size_t size) {
#ifdef DEBUG_MEM
    eprintf("ALLOC %ld B, ", size);
    eprint_objtype(t);
    eprintf("\n");
#endif
    Obj* o = malloc(size);
    vm.alloc_bytes += size;
    vm.alloc_objs++;
    o->type = t;

#ifdef DEBUG_GC_STRESS
    collect_garbage();
#else
    if (vm.alloc_bytes >= vm.gc_threshold) {
        collect_garbage();
        vm.gc_threshold = vm.alloc_bytes * 2;
    }
#endif

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
            table_delete(&vm.strings, (ObjString*) o);
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
            size = sizeof(ObjUpvalue);
            break;
        case OT_CLASS:
            size = sizeof(ObjClass);
            table_free(&((ObjClass*) o)->methods);
            break;
        case OT_INSTANCE:
            size = sizeof(ObjInstance);
            table_free(&((ObjInstance*) o)->attrs);
            break;
        case OT_ARRAY:
            size = sizeof(ObjArray) + ((ObjArray*) o)->len * sizeof(Value);
            break;
    }
#ifdef DEBUG_MEM
    eprintf("FREE %ld B, ", size);
    eprint_objtype(o->type);
    eprintf("\n");
#endif
    free(o);
    vm.alloc_bytes -= size;
    vm.alloc_objs--;
}

#define MARK(o) ((o)->next = (Obj*) ((intptr_t) (o)->next | 1))
#define UNMARK(o) ((o)->next = (Obj*) ((intptr_t) (o)->next & ~1))
#define MARKED(o) ((intptr_t) (o)->next & 1)

#define MARK_VALUE(v)                                                          \
    if ((v).type == VT_OBJ) MARK_OBJ((v).obj)

#define MARK_OBJ(o) (mark_obj((Obj*) o))

void mark_obj(Obj* o) {
    if (MARKED(o)) return;
    MARK(o);
    switch (o->type) {
        case OT_STRING:
            break;
        case OT_FUNCTION: {
            ObjFunction* f = (ObjFunction*) o;
            if (f->name) MARK_OBJ(f->name);
            for (int i = 0; i < f->chunk.constants.size; i++) {
                MARK_VALUE(f->chunk.constants.d[i]);
            }
            break;
        }
        case OT_CLOSURE: {
            ObjClosure* c = (ObjClosure*) o;
            MARK_OBJ(c->f);
            for (int i = 0; i < c->nupvalues; i++) {
                MARK_OBJ(c->upvalues[i]);
            }
            break;
        }
        case OT_UPVALUE: {
            ObjUpvalue* u = (ObjUpvalue*) o;
            if (u->loc == &u->closed) MARK_VALUE(u->closed);
            break;
        }
        case OT_CLASS: {
            ObjClass* c = (ObjClass*) o;
            MARK_OBJ(c->name);
            mark_table(&c->methods);
            break;
        }
        case OT_INSTANCE: {
            ObjInstance* i = (ObjInstance*) o;
            MARK_OBJ(i->cls);
            mark_table(&i->attrs);
            break;
        }
        case OT_ARRAY: {
            ObjArray* arr = (ObjArray*) o;
            for (int i = 0; i < arr->len; i++) {
                MARK_VALUE(arr->data[i]);
            }
        }
    }
}

void mark_table(Table* tbl) {
    for (int i = 0; i < tbl->cap; i++) {
        if (tbl->ents[i].key) {
            MARK_OBJ(tbl->ents[i].key);
            MARK_VALUE(tbl->ents[i].value);
        }
    }
}

void collect_garbage() {
    if (!vm.gc_on) return;
#ifdef DEBUG_MEM
    eprintf("------ begin gc [%ld B, %d OBJ] --------\n", vm.alloc_bytes,
            vm.alloc_objs);
#endif

    for (Value* p = vm.stack_base; p < vm.sp; p++) {
        MARK_VALUE(*p);
    }
    for (CallFrame* p = vm.call_stack; p <= vm.csp; p++) {
        MARK_OBJ(p->func);
        if (p->clos) MARK_OBJ(p->clos);
    }
    mark_table(&vm.globals);
    for (ObjUpvalue* p = vm.open_upvalues; p; p = p->next) {
        MARK_OBJ(p);
    }

    Obj** p = &vm.objs;
    while (*p) {
        if (MARKED(*p)) {
            UNMARK(*p);
            p = &(*p)->next;
        } else {
            Obj* tmp = *p;
            *p = (*p)->next;
            free_obj(tmp);
        }
    }

#ifdef DEBUG_MEM
    eprintf("------ end gc [%ld B, %d OBJ] --------\n", vm.alloc_bytes,
            vm.alloc_objs);
#endif
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

ObjClass* create_class(ObjString* name) {
    ObjClass* cls = ALLOC_OBJ(ObjClass, OT_CLASS, 0);
    cls->name = name;
    table_init(&cls->methods);
    return cls;
}

ObjInstance* create_instance(ObjClass* cls) {
    ObjInstance* inst = ALLOC_OBJ(ObjInstance, OT_INSTANCE, 0);
    inst->cls = cls;
    table_init(&inst->attrs);
    return inst;
}

ObjArray* create_array(size_t len) {
    ObjArray* arr = ALLOC_OBJ(ObjArray, OT_ARRAY, len * sizeof(Value));
    arr->len = len;
    for (int i = 0; i < len; i++) {
        arr->data[i] = NIL_VAL;
    }
    return arr;
}

ObjArray* create_array_full(size_t len, Value* vals) {
    ObjArray* arr = ALLOC_OBJ(ObjArray, OT_ARRAY, len * sizeof(Value));
    arr->len = len;
    memcpy(arr->data, vals, len * sizeof(Value));
    return arr;
}

void disassemble_function(ObjFunction* func) {
    eprintf("===== begin %s =====\n",
            func->name ? func->name->data : "<anonymous fn>");
    disassemble_chunk(&func->chunk);
    eprintf("===== end %s =====\n",
            func->name ? func->name->data : "<anonymous fn>");
}