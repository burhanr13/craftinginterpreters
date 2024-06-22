#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void chunk_init(Chunk* c) {
    memset(c, 0, sizeof *c);

    Vec_push(c->lines, -1);
}

void chunk_clear(Chunk* c) {
    free(c->code.d);
    free(c->constants.d);
    chunk_init(c);
}

void chunk_write(Chunk* c, u8 b, int line) {
    Vec_push(c->code, b);
    if (line > c->lines.size - 1) {
        int last_instr = c->lines.d[c->lines.size - 1];
        int n = line - c->lines.size;
        for (int i = 0; i < n; i++) {
            Vec_push(c->lines, last_instr);
        }
        Vec_push(c->lines, c->code.size - 1);
    }
}

void chunk_get_instr_line(Chunk* c, void* pc) {
    bsearch()
}

void chunk_load_const(Chunk* c, Value v, int line) {
    chunk_write(c, OP_LOAD_CONST, line);
    chunk_write(c, add_constant(c, v), line);
}

int add_constant(Chunk* c, Value v) {
    Vec_push(c->constants, v);
    return c->constants.size - 1;
}

int disassemble_instr(Chunk* c, int off) {
    switch (c->code[off++]) {
        case OP_LOAD_CONST:
            printf("load ");
            int const_ind = c->code[off++];
            print_value(c->constants.d[const_ind]);
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