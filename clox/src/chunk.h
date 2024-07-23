#ifndef CHUNK_H
#define CHUNK_H

#include "types.h"

enum {
    OP_NOP,
    OP_DEF_GLOBAL,
    OP_PUSH_GLOBAL,
    OP_POP_GLOBAL,
    OP_PUSH_LOCAL,
    OP_POP_LOCAL,
    OP_PUSH_CONST,
    OP_PUSH_NIL,
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH,
    OP_POP,
    OP_POPN,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NOT,
    OP_TEQ,
    OP_TGT,
    OP_TLT,
    OP_JMP,
    OP_JMP_TRUE,
    OP_JMP_FALSE,
    OP_CALL,
    OP_RET,
};

typedef struct _Value Value;

typedef struct _Chunk {
    Vector(u8) code;

    Vector(Value) constants;

    Vector(int) lines;

} Chunk;

void chunk_init(Chunk* c);
void chunk_free(Chunk* c);
void chunk_write(Chunk* c, u8 b, int line);

int chunk_get_instr_line(Chunk* c, u8* pc);

void chunk_push_const(Chunk* c, Value v, int line);
int add_constant(Chunk* c, Value v);

int disassemble_instr(Chunk* c, int off);
void disassemble_chunk(Chunk* c);

#endif