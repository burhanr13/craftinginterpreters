#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <math.h>

#include "chunk.h"
#include "compiler.h"

#define DEBUG_INFO

VM vm;

void VM_init() {
    vm.sp = vm.stack;

    table_init(&vm.strings);
    table_init(&vm.globals);
}

void VM_free() {
    free_all_obj();
    table_free(&vm.strings);
    table_free(&vm.globals);
}

void runtime_error(char* message, ...) {
    printf("Error at line %d: ", chunk_get_instr_line(vm.chunk, vm.pc));
    va_list l;
    va_start(l, message);
    vprintf(message, l);
    va_end(l);
    printf("\n");
}

#define PUSH(a) *vm.sp++ = a
#define POP(a) a = *--vm.sp

#define GET_ID(id) (ObjString*) vm.chunk->constants.d[id].obj

#define BINARY(op, res_val)                                                    \
    POP(Value b);                                                              \
    POP(Value a);                                                              \
    if (a.type != VT_NUMBER || b.type != VT_NUMBER) {                          \
        runtime_error("Operand must be a number.");                            \
        return RUNTIME_ERROR;                                                  \
    }                                                                          \
    PUSH(res_val(a.num op b.num));

bool truthy(Value v) {
    return !(v.type == VT_NIL || (v.type == VT_BOOL && !v.b));
}

int run() {
    while (true) {
        u8 instr = *vm.pc++;
        switch (instr) {
            case OP_DEF_GLOBAL: {
                POP(Value v);
                ObjString* id = GET_ID(*vm.pc++);
                table_set(&vm.globals, id, v);
                break;
            }
            case OP_PUSH_GLOBAL: {
                ObjString* id = GET_ID(*vm.pc++);
                Value v;
                if (!table_get(&vm.globals, id, &v)) {
                    runtime_error("Undefined variable \"%s\".", id->data);
                    return RUNTIME_ERROR;
                }
                PUSH(v);
                break;
            }
            case OP_POP_GLOBAL: {
                ObjString* id = GET_ID(*vm.pc++);
                POP(Value v);
                if (table_set(&vm.globals, id, v)) {
                    table_delete(&vm.globals, id);
                    runtime_error("Undefined variable \"%s\".", id->data);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_PUSH_LOCAL: {
                PUSH(vm.stack[*vm.pc++]);
                break;
            }
            case OP_POP_LOCAL: {
                POP(vm.stack[*vm.pc++]);
                break;
            }
            case OP_PUSH_CONST:
                PUSH(vm.chunk->constants.d[*vm.pc++]);
                break;
            case OP_PUSH_NIL:
                PUSH(NIL_VAL);
                break;
            case OP_PUSH_TRUE:
                PUSH(BOOL_VAL(true));
                break;
            case OP_PUSH_FALSE:
                PUSH(BOOL_VAL(false));
                break;
            case OP_PUSH:
                vm.sp++;
                break;
            case OP_POP:
                --vm.sp;
                break;
            case OP_POPN:
                vm.sp -= *vm.pc++;
                break;
            case OP_NEG: {
                POP(Value a);
                if (a.type != VT_NUMBER) {
                    runtime_error("Operand must be a number.");
                    return RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(-a.num));
                break;
            }
            case OP_NOT: {
                POP(Value a);
                PUSH(BOOL_VAL(!truthy(a)));
                break;
            }
            case OP_ADD: {
                POP(Value b);
                POP(Value a);
                if (a.type == VT_NUMBER && b.type == VT_NUMBER) {
                    PUSH(NUMBER_VAL(a.num + b.num));
                } else if (isObjType(a, OT_STRING) && isObjType(b, OT_STRING)) {
                    PUSH(OBJ_VAL((Obj*) concat_string((ObjString*) a.obj,
                                                      (ObjString*) b.obj)));
                } else {
                    runtime_error("Operand must be a number or string.");
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUB: {
                BINARY(-, NUMBER_VAL);
                break;
            }
            case OP_MUL: {
                BINARY(*, NUMBER_VAL);
                break;
            }
            case OP_DIV: {
                BINARY(/, NUMBER_VAL);
                break;
            }
            case OP_MOD: {
                POP(Value b);
                POP(Value a);
                if (a.type != VT_NUMBER || b.type != VT_NUMBER) {
                    runtime_error("Operand must be a number.");
                    return RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(fmod(a.num, b.num)));
                break;
            }
            case OP_EQ: {
                POP(Value a);
                POP(Value b);
                PUSH(BOOL_VAL(value_equal(a, b)));
                break;
            }
            case OP_GT: {
                BINARY(>, BOOL_VAL);
                break;
            }
            case OP_LT: {
                BINARY(<, BOOL_VAL);
                break;
            }
            case OP_PRINT: {
                POP(Value v);
                print_value(v);
                printf("\n");
                break;
            }
            case OP_JMP: {
                int off = *vm.pc++;
                off |= *vm.pc++ << 8;
                off = off << 16 >> 16;
                vm.pc += off;
                break;
            }
            case OP_JMP_TRUE: {
                int off = *vm.pc++;
                off |= *vm.pc++ << 8;
                off = off << 16 >> 16;
                POP(Value cond);
                if (truthy(cond)) {
                    vm.pc += off;
                }
                break;
            }
            case OP_JMP_FALSE: {
                int off = *vm.pc++;
                off |= *vm.pc++ << 8;
                off = off << 16 >> 16;
                POP(Value cond);
                if (!truthy(cond)) {
                    vm.pc += off;
                }
                break;
            }
            case OP_RET:
                return OK;
        }
    }
}

int interpret(char* source) {
    Chunk chunk;
    chunk_init(&chunk);

    int res = OK;

    if (!compile(source, &chunk)) {
        res = COMPILE_ERROR;
        goto interpret_end;
    }

#ifdef DEBUG_INFO
    disassemble_chunk(&chunk);
#endif

    vm.chunk = &chunk;
    vm.pc = chunk.code.d;

    res = run();

interpret_end:
    chunk_free(&chunk);

#ifdef DEBUG_INFO
    printf("Allocated bytes: %ld\n", alloc_bytes);
#endif

    return res;
}