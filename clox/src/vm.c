#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"

VM vm;

void VM_init() {
    vm.sp = vm.stack;
}

void VM_free() {}

int run() {
    while (true) {
        u8 instr = *vm.pc++;
        switch (instr) {
            case OP_LOAD_CONST:
                *vm.sp++ = vm.chunk->constants[*vm.pc++];
                break;
            case OP_NEG: {
                Value a = *--vm.sp;
                *vm.sp++ = -a;
                break;
            }
            case OP_ADD: {
                Value b = *--vm.sp;
                Value a = *--vm.sp;
                *vm.sp++ = a + b;
                break;
            }
            case OP_SUB: {
                Value b = *--vm.sp;
                Value a = *--vm.sp;
                *vm.sp++ = a - b;
                break;
            }
            case OP_MUL: {
                Value b = *--vm.sp;
                Value a = *--vm.sp;
                *vm.sp++ = a * b;
                break;
            }
            case OP_DIV: {
                Value b = *--vm.sp;
                Value a = *--vm.sp;
                *vm.sp++ = a / b;
                break;
            }
            case OP_RET: {
                Value v = *--vm.sp;
                print_value(v);
                printf("\n");
                return OK;
            }
        }
    }
}

int interpret(char* source) {
    compile(source);
    return OK;
}