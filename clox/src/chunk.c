#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void chunk_init(Chunk* c) {
    memset(c, 0, sizeof *c);

    Vec_push(c->lines, -1);
}

void chunk_free(Chunk* c) {
    free(c->code.d);
    free(c->constants.d);
    free(c->lines.d);
}

void chunk_write(Chunk* c, u8 b, int line) {
    Vec_push(c->code, b);
    if (line > c->lines.size - 1) {
        int last_instr = c->lines.size == 0 ? 0 : c->lines.d[c->lines.size - 1];
        int n = line - c->lines.size;
        for (int i = 0; i < n; i++) {
            Vec_push(c->lines, last_instr);
        }
        Vec_push(c->lines, c->code.size - 1);
    }
}

int chunk_get_instr_line(Chunk* c, u8* pc) {
    int off = pc - c->code.d;
    for (int i = 0; i < c->lines.size; i++) {
        if (c->lines.d[i] > off && i > 0) {
            int line = i - 1;
            if (line < 1) line = 1;
            return line;
        }
    }
    return 1;
}

void chunk_push_const(Chunk* c, Value v, int line) {
    chunk_write(c, OP_PUSH_CONST, line);
    chunk_write(c, add_constant(c, v), line);
}

int add_constant(Chunk* c, Value v) {
    Vec_push(c->constants, v);
    return c->constants.size - 1;
}

int disassemble_instr(Chunk* c, int off) {
    switch (c->code.d[off++]) {
        case OP_NOP:
            printf("nop");
            break;
        case OP_DEF_GLOBAL: {
            printf("def globals[");
            int const_ind = c->code.d[off++];
            printf("%s", ((ObjString*) c->constants.d[const_ind].obj)->data);
            printf("]");
            break;
        }
        case OP_PUSH_GLOBAL: {
            printf("push globals[");
            int const_ind = c->code.d[off++];
            printf("%s", ((ObjString*) c->constants.d[const_ind].obj)->data);
            printf("]");
            break;
        }
        case OP_POP_GLOBAL: {
            printf("pop globals[");
            int const_ind = c->code.d[off++];
            printf("%s", ((ObjString*) c->constants.d[const_ind].obj)->data);
            printf("]");
            break;
        }
        case OP_PUSH_LOCAL: {
            printf("push locals[");
            int ind = c->code.d[off++];
            printf("%d", ind);
            printf("]");
            break;
        }
        case OP_POP_LOCAL: {
            printf("pop locals[");
            int ind = c->code.d[off++];
            printf("%d", ind);
            printf("]");
            break;
        }
        case OP_PUSH_CONST: {
            printf("push ");
            int const_ind = c->code.d[off++];
            print_value(c->constants.d[const_ind]);
            break;
        }
        case OP_PUSH_NIL:
            printf("push nil");
            break;
        case OP_PUSH_TRUE:
            printf("push true");
            break;
        case OP_PUSH_FALSE:
            printf("push false");
            break;
        case OP_PUSH:
            printf("push");
            break;
        case OP_POP:
            printf("pop");
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
        case OP_MOD:
            printf("mod");
            break;
        case OP_NOT:
            printf("not");
            break;
        case OP_EQ:
            printf("eq");
            break;
        case OP_GT:
            printf("gt");
            break;
        case OP_LT:
            printf("lt");
            break;
        case OP_PRINT:
            printf("print");
            break;
        case OP_JMP: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            printf("jmp %04x", dst);
            break;
        }
        case OP_JMP_TRUE: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            printf("jmp,true %04x", dst);
            break;
        }
        case OP_JMP_FALSE: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            printf("jmp,false %04x", dst);
            break;
        }
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
    while (off < c->code.size) {
        printf("%04x: ", off);
        off = disassemble_instr(c, off);
        printf("\n");
    }
}