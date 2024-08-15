#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"

void chunk_init(Chunk* c) {
    Vec_init(c->code);
    Vec_init(c->constants);
    Vec_init(c->lines);

    c->linesStart = -1;
}

void chunk_free(Chunk* c) {
    Vec_free(c->code);
    Vec_free(c->constants);
    Vec_free(c->lines);
}

void chunk_write(Chunk* c, u8 b, int line) {
    Vec_push(c->code, b);
    if (c->linesStart == -1) {
        c->linesStart = line;
        Vec_push(c->lines, c->code.size - 1);
    } else {
        line -= c->linesStart;
        if (line > c->lines.size - 1) {
            int last_instr =
                c->lines.size == 0 ? 0 : c->lines.d[c->lines.size - 1];
            int n = line - c->lines.size;
            for (int i = 0; i < n; i++) {
                Vec_push(c->lines, last_instr);
            }
            Vec_push(c->lines, c->code.size - 1);
        }
    }
}

int chunk_get_instr_line(Chunk* c, u8* pc) {
    if (c->lines.size == 0) return c->linesStart;
    int off = pc - c->code.d;
    for (int i = 0; i < c->lines.size; i++) {
        if (c->lines.d[i] > off) {
            return i + c->linesStart;
        }
    }
    return c->lines.size - 1 + c->linesStart;
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
            eprintf("nop");
            break;
        case OP_DEF_GLOBAL: {
            eprintf("def ");
            int const_ind = c->code.d[off++];
            eprintf("%s (@%d)",
                    ((ObjString*) c->constants.d[const_ind].obj)->data,
                    const_ind);
            break;
        }
        case OP_PUSH_GLOBAL: {
            eprintf("push ");
            int const_ind = c->code.d[off++];
            eprintf("%s (@%d)",
                    ((ObjString*) c->constants.d[const_ind].obj)->data,
                    const_ind);
            break;
        }
        case OP_POP_GLOBAL: {
            eprintf("pop ");
            int const_ind = c->code.d[off++];
            eprintf("%s (@%d)",
                    ((ObjString*) c->constants.d[const_ind].obj)->data,
                    const_ind);
            break;
        }
        case OP_PUSH_LOCAL: {
            eprintf("push local$");
            int ind = c->code.d[off++];
            eprintf("%d", ind);
            break;
        }
        case OP_POP_LOCAL: {
            eprintf("pop local$");
            int ind = c->code.d[off++];
            eprintf("%d", ind);
            break;
        }
        case OP_PUSH_UPVALUE: {
            eprintf("push upvalue$");
            int ind = c->code.d[off++];
            eprintf("%d", ind);
            break;
        }
        case OP_POP_UPVALUE: {
            eprintf("pop upvalue$");
            int ind = c->code.d[off++];
            eprintf("%d", ind);
            break;
        }
        case OP_PUSH_CLOSURE: {
            eprintf("push <");
            int const_ind = c->code.d[off++];
            eprint_value(c->constants.d[const_ind]);
            eprintf("> (@%d)", const_ind);
            break;
        }
        case OP_PUSH_ARRAY: {
            eprintf("push array");
            break;
        }
        case OP_PUSH_ARRAY_INIT: {
            eprintf("push array inited[");
            int ind = c->code.d[off++];
            eprintf("%d]", ind);
            break;
        }
        case OP_PUSH_CONST: {
            eprintf("push ");
            int const_ind = c->code.d[off++];
            eprint_value(c->constants.d[const_ind]);
            eprintf(" (@%d)", const_ind);
            break;
        }
        case OP_PUSH_NIL:
            eprintf("push nil");
            break;
        case OP_PUSH_TRUE:
            eprintf("push true");
            break;
        case OP_PUSH_FALSE:
            eprintf("push false");
            break;
        case OP_PUSH:
            eprintf("push");
            break;
        case OP_POP:
            eprintf("pop");
            break;
        case OP_POPN:
            eprintf("pop %dx", c->code.d[off++]);
            break;
        case OP_GETATTR: {
            eprintf("getattr ");
            int const_ind = c->code.d[off++];
            eprintf("%s (@%d)",
                    ((ObjString*) c->constants.d[const_ind].obj)->data,
                    const_ind);
            break;
        }
        case OP_SETATTR: {
            eprintf("setattr ");
            int const_ind = c->code.d[off++];
            eprintf("%s (@%d)",
                    ((ObjString*) c->constants.d[const_ind].obj)->data,
                    const_ind);
            break;
        }
        case OP_GETITEM:
            eprintf("getitem");
            break;
        case OP_SETITEM:
            eprintf("setitem");
            break;
        case OP_NEG:
            eprintf("neg");
            break;
        case OP_ADD:
            eprintf("add");
            break;
        case OP_SUB:
            eprintf("sub");
            break;
        case OP_MUL:
            eprintf("mul");
            break;
        case OP_DIV:
            eprintf("div");
            break;
        case OP_MOD:
            eprintf("mod");
            break;
        case OP_NOT:
            eprintf("not");
            break;
        case OP_TEQ:
            eprintf("teq");
            break;
        case OP_TGT:
            eprintf("tgt");
            break;
        case OP_TLT:
            eprintf("tlt");
            break;
        case OP_JMP: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            eprintf("jmp %04x", dst);
            break;
        }
        case OP_JMP_TRUE: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            eprintf("jmp,true %04x", dst);
            break;
        }
        case OP_JMP_FALSE: {
            int dst = c->code.d[off++];
            dst |= c->code.d[off++] << 8;
            dst = dst << 16 >> 16;
            dst += off;
            eprintf("jmp,false %04x", dst);
            break;
        }
        case OP_CALL:
            eprintf("call (%d)", c->code.d[off++]);
            break;
        case OP_RET:
            eprintf("ret");
            break;
        default:
            eprintf("unknown");
            break;
    }
    return off;
}

void disassemble_chunk(Chunk* c) {
    int off = 0;
    while (off < c->code.size) {
        eprintf("%04x: ", off);
        off = disassemble_instr(c, off);
        eprintf("\n");
    }
}