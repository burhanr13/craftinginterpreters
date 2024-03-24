#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>

void chunk_init(Chunk* c) {
    c->code = NULL;
    c->size = 0;
    c->cap = 0;
    c->constants = NULL;
    c->n_constants = 0;
    c->cap_constants = 0;
}

void chunk_clear(Chunk* c) {
    free(c->code);
    free(c->constants);
    chunk_init(c);
}

void chunk_write(Chunk* c, u8 b, int line) {
    if (c->size == c->cap) {
        c->cap = c->cap ? 2 * c->cap : 8;
        c->code = realloc(c->code, c->cap);
    }
    c->code[c->size++] = b;
    c->last_line = line;
}

void chunk_load_const(Chunk* c, Value v, int line) {
    chunk_write(c, OP_LOAD_CONST, line);
    chunk_write(c, add_constant(c, v), line);
}

int add_constant(Chunk* c, Value v) {
    if (c->n_constants == c->cap_constants) {
        c->cap_constants = c->cap_constants ? 2 * c->cap_constants : 8;
        c->constants = realloc(c->constants, c->cap_constants * sizeof(Value));
    }
    c->constants[c->n_constants++] = v;
    return c->n_constants - 1;
}

int disassemble_instr(Chunk* c, int off) {
    switch (c->code[off++]) {
        case OP_LOAD_CONST:
            printf("load ");
            int const_ind = c->code[off++];
            print_value(c->constants[const_ind]);
            break;
        case OP_NEG:
            printf("neg");
            break;
        case OP_ADD:
            printf("add");
            break;
        case OP_SUB:
            printf("sub");
            break;
        case OP_MUL:
            printf("mul");
            break;
        case OP_DIV:
            printf("div");
            break;
        case OP_RET:
            printf("ret");
            break;
        default:
            printf("unknown");
            break;
    }
    return off;
}

void disassemble_chunk(Chunk* c) {
    printf("==== Chunk ====\n");
    int off = 0;
    while (off < c->size) {
        printf("%03x: ", off);
        off = disassemble_instr(c, off);
        printf("\n");
    }
}