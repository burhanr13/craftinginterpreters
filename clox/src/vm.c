#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"

VM vm;

void VM_init() {
    vm.sp = vm.stack;

    table_init(&vm.strings);
}

void VM_free() {
    free_all_obj();
    table_free(&vm.strings);
}

void runtime_error(char* message) {
    printf("Error at line %d: %s\n", chunk_get_instr_line(vm.chunk, vm.pc),
           message);
}

#define PUSH(a) *vm.sp++ = a
#define POP(a) a = *--vm.sp

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
            case OP_POP: {
                POP(Value v);
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

    disassemble_chunk(&chunk);

    vm.chunk = &chunk;
    vm.pc = chunk.code.d;

    res = run();

interpret_end:
    chunk_free(&chunk);

    return res;
}