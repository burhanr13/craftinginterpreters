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
    Value* fp;
    ObjUpvalue** up;
    u8* ip;
} CallFrame;

typedef struct {
    Obj* objs;
    Table strings;
    Table globals;

    ObjUpvalue* open_upvalues;

    size_t alloc_bytes;

    Value* sp;
    CallFrame* csp;
    CallFrame call_stack[MAX_CALLS];
    Value stack[STACK_SIZE];
} VM;

extern VM vm;

void VM_init();
void VM_free();

int interpret(char* source);

int run_file(char* filename);

#endif