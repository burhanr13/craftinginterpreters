#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <math.h>

#include "builtins.h"
#include "chunk.h"
#include "compiler.h"

VM vm;

void VM_init() {
    vm.alloc_bytes = 0;

    table_init(&vm.strings);
    table_init(&vm.globals);

    ADD_BUILTIN(clock);
    ADD_BUILTIN(print);
    ADD_BUILTIN(println);
    ADD_BUILTIN(scanln);
}

void VM_free() {
    free_all_obj();
    table_free(&vm.strings);
    table_free(&vm.globals);
}

void runtime_error(char* message, ...) {
    CallFrame f = *vm.cur_frame;
    eprintf("Runtime error at line %d: ",
            chunk_get_instr_line(&f.func->chunk, f.ip - 1));
    va_list l;
    va_start(l, message);
    vfprintf(stderr, message, l);
    va_end(l);
    eprintf("\n");
    CallFrame* p = *vm.cur_csp;
    *p = f;
    for (; p > vm.call_stack; p--) {
        eprintf("from call of %s at line %d\n",
                p[0].func->name ? p[0].func->name->data : "<anonymous fn>",
                chunk_get_instr_line(&p[-1].func->chunk, p[-1].ip - 1));
    }
}

#define FETCH() *cur.ip++
#define CONST(n) (cur.func->chunk.constants.d[n])

#define PUSH(a) *sp++ = a
#define POP(a) a = *--sp

#define GET_ID(id) (ObjString*) CONST(id).obj

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

int run(ObjFunction* toplevel) {

    Value* sp = vm.stack;
    CallFrame* csp = vm.call_stack;

    PUSH(OBJ_VAL(toplevel));

    CallFrame cur;
    cur.fp = vm.stack;
    cur.func = (ObjFunction*) cur.fp[0].obj;
    cur.ip = cur.func->chunk.code.d;

    vm.cur_frame = &cur;
    vm.cur_sp = &sp;
    vm.cur_csp = &csp;

    while (true) {
        switch (FETCH()) {
            case OP_DEF_GLOBAL: {
                POP(Value v);
                ObjString* id = GET_ID(FETCH());
                table_set(&vm.globals, id, v);
                break;
            }
            case OP_PUSH_GLOBAL: {
                ObjString* id = GET_ID(FETCH());
                Value v;
                if (!table_get(&vm.globals, id, &v)) {
                    runtime_error("Undefined variable \"%s\".", id->data);
                    return RUNTIME_ERROR;
                }
                PUSH(v);
                break;
            }
            case OP_POP_GLOBAL: {
                ObjString* id = GET_ID(FETCH());
                POP(Value v);
                if (table_set(&vm.globals, id, v)) {
                    table_delete(&vm.globals, id);
                    runtime_error("Undefined variable \"%s\".", id->data);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_PUSH_LOCAL: {
                PUSH(cur.fp[FETCH()]);
                break;
            }
            case OP_POP_LOCAL: {
                POP(cur.fp[FETCH()]);
                break;
            }
            case OP_PUSH_CONST:
                PUSH(CONST(FETCH()));
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
                sp++;
                break;
            case OP_POP:
                --sp;
                break;
            case OP_POPN:
                sp -= FETCH();
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
                    PUSH(OBJ_VAL(
                        concat_string((ObjString*) a.obj, (ObjString*) b.obj)));
                } else if (isObjType(a, OT_STRING)) {
                    PUSH(OBJ_VAL(
                        concat_string((ObjString*) a.obj, string_value(b))));
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
            case OP_TEQ: {
                POP(Value a);
                POP(Value b);
                PUSH(BOOL_VAL(value_equal(a, b)));
                break;
            }
            case OP_TGT: {
                BINARY(>, BOOL_VAL);
                break;
            }
            case OP_TLT: {
                BINARY(<, BOOL_VAL);
                break;
            }
            case OP_JMP: {
                int off = FETCH();
                off |= FETCH() << 8;
                off = off << 16 >> 16;
                cur.ip += off;
                break;
            }
            case OP_JMP_TRUE: {
                int off = FETCH();
                off |= FETCH() << 8;
                off = off << 16 >> 16;
                POP(Value cond);
                if (truthy(cond)) {
                    cur.ip += off;
                }
                break;
            }
            case OP_JMP_FALSE: {
                int off = FETCH();
                off |= FETCH() << 8;
                off = off << 16 >> 16;
                POP(Value cond);
                if (!truthy(cond)) {
                    cur.ip += off;
                }
                break;
            }
            case OP_CALL: {
                int nargs = FETCH();
                Value v = sp[-(nargs + 1)];
                switch (v.type) {
                    case VT_OBJ:
                        switch (v.obj->type) {
                            case OT_FUNCTION: {
                                ObjFunction* func = (ObjFunction*) v.obj;
                                if (csp - vm.call_stack == MAX_CALLS) {
                                    runtime_error("Max call depth exceeded.");
                                    return RUNTIME_ERROR;
                                }
                                *csp++ = cur;
                                cur.func = func;
                                cur.fp = sp - nargs - 1;
                                cur.ip = func->chunk.code.d;
                                if (func->nargs != nargs) {
                                    runtime_error("Invalid argument count, "
                                                  "expected %d, got %d.",
                                                  func->nargs, nargs);
                                    return RUNTIME_ERROR;
                                }
                                break;
                            }
                            default:
                                runtime_error("Value not callable.");
                                return RUNTIME_ERROR;
                        }
                        break;
                    case VT_BUILTIN:
                        if (v.builtin(nargs, sp - nargs - 1) == RUNTIME_ERROR) {
                            runtime_error("Error from builtin function.");
                            return RUNTIME_ERROR;
                        }
                        sp -= nargs;
                        break;
                    default:
                        runtime_error("Value not callable.");
                        return RUNTIME_ERROR;
                }
                break;
            }
            case OP_RET: {
                if (csp == vm.call_stack) return OK;
                POP(Value v);
                sp = cur.fp;
                cur = *--csp;
                PUSH(v);
                break;
            }
        }
    }
}

int interpret(char* source) {

    ObjFunction* toplevel = compile(source);

    if (!toplevel) {
        return COMPILE_ERROR;
    }

    return run(toplevel);
}