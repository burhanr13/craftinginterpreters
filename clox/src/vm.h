#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "table.h"
#include "value.h"

#define MAX_LOCALS 256
#define MAX_CALLS 64

#define STACK_SIZE (MAX_CALLS * MAX_LOCALS)

enum { OK, NO_FILE, COMPILE_ERROR, RUNTIME_ERROR };

typedef struct {
    ObjFunction* func;
    ObjClosure* clos;
    Value* fp;
    u8* ip;
} CallFrame;

typedef struct {
    Obj* objs;
    Table strings;
    Table globals;

    ObjUpvalue* open_upvalues;

    bool gc_on;
    size_t gc_threshold;
    size_t alloc_bytes;
    int alloc_objs;

    Value* sp;
    CallFrame* csp;
    Value* stack_base;
    CallFrame call_stack[MAX_CALLS];
} VM;

extern VM vm;

void VM_init();
void VM_free();

static inline void gc_enable() {
    vm.gc_on = true;
#ifdef DEBUG_MEM
    eprintf("=========== gc on [%ld B, %d OBJ] ============\n", vm.alloc_bytes,
            vm.alloc_objs);
#endif
}

static inline void gc_disable() {
    vm.gc_on = false;
#ifdef DEBUG_MEM
    eprintf("=========== gc off [%ld B, %d OBJ] ===========\n", vm.alloc_bytes,
            vm.alloc_objs);
#endif
}

int interpret(char* source);

#endif