#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_SIZE 256

enum { OK, COMPILE_ERROR, RUNTIME_ERROR };

typedef struct {
    Chunk* chunk;
    u8* pc;

    Obj* objs;

    Table strings;

    Table globals;

    Value* sp;
    Value stack[STACK_SIZE];
} VM;

extern VM vm;

void VM_init();
void VM_free();

int interpret(char* source);

#endif