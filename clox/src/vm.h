#ifndef VM_H
#define VM_H

#include "chunk.h"

#define STACK_SIZE 256

enum { OK, COMPILE_ERROR };

typedef struct {
    Chunk* chunk;
    u8* pc;

    Value* sp;
    Value stack[STACK_SIZE];
} VM;

void VM_init();
void VM_free();

int interpret(char* source);

#endif