#ifndef VM_H
#define VM_H

#include "chunk.h"

#define OK 0

#define STACK_SIZE 256

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