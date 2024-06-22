#ifndef CHUNK_H
#define CHUNK_H

#include "types.h"
#include "value.h"

enum {
    OP_LOAD_CONST,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_RET
};

typedef struct {
    Vector(u8) code;

    Vector(Value) constants;

    Vector(int) lines;

} Chunk;

void chunk_init(Chunk* c);
void chunk_clear(Chunk* c);
void chunk_write(Chunk* c, u8 b, int line);

void chunk_load_const(Chunk* c, Value v, int line);
int add_constant(Chunk* c, Value v);

int disassemble_instr(Chunk* c, int off);
void disassemble_chunk(Chunk* c);

#endif