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
    vm.open_upvalues = NULL;

    ADD_BUILTIN(clock);
    ADD_BUILTIN(print);
    ADD_BUILTIN(println);
    ADD_BUILTIN(scanln);
    ADD_BUILTIN(getc);

    ADD_BUILTIN(source);
}

void VM_free() {
    free_all_obj();
    table_free(&vm.strings);
    table_free(&vm.globals);
}

void runtime_error(char* message, ...) {
    CallFrame f = *vm.frame;
    eprintf("Runtime error at line %d: ",
            chunk_get_instr_line(&f.func->chunk, f.ip - 1));
    va_list l;
    va_start(l, message);
    vfprintf(stderr, message, l);
    va_end(l);
    eprintf("\n");
    CallFrame* p = *vm.cspp;
    *p = f;
    for (; p > vm.call_stack; p--) {
        eprintf("    from call of %s at line %d\n",
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
        runtime_error("Invalid operand for '" #op "'.");                       \
        return RUNTIME_ERROR;                                                  \
    }                                                                          \
    PUSH(res_val(a.num op b.num));

bool truthy(Value v) {
    return !(v.type == VT_NIL || (v.type == VT_BOOL && !v.b));
}

void close_upvalues(Value* sp) {
    while (vm.open_upvalues && vm.open_upvalues->loc >= sp) {
        vm.open_upvalues->closed = *vm.open_upvalues->loc;
        vm.open_upvalues->loc = &vm.open_upvalues->closed;
        vm.open_upvalues = vm.open_upvalues->next;
    }
}

int run(ObjFunction* toplevel) {
    Value* sp = vm.stack;
    CallFrame* csp = vm.call_stack;

    PUSH(OBJ_VAL(toplevel));

    CallFrame cur;
    cur.fp = vm.stack;
    cur.func = (ObjFunction*) cur.fp[0].obj;
    cur.up = NULL;
    cur.ip = cur.func->chunk.code.d;

    vm.frame = &cur;
    vm.spp = &sp;
    vm.cspp = &csp;

#ifdef DEBUG_TRACE
    eprintf("-------------- Begin Trace --------------\n");
#endif

    while (true) {
#ifdef DEBUG_TRACE
        eprintf("Stack:");
        for (Value* p = cur.fp; p < sp; p++) eprintf(" "), eprint_value(*p);
        eprintf("\n%04lx: ", cur.ip - cur.func->chunk.code.d);
        disassemble_instr(&cur.func->chunk, cur.ip - cur.func->chunk.code.d);
        eprintf("\n");
#endif
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
            case OP_PUSH_UPVALUE: {
                PUSH(*cur.up[FETCH()]->loc);
                break;
            }
            case OP_POP_UPVALUE: {
                POP(*cur.up[FETCH()]->loc);
                break;
            }
            case OP_PUSH_CLOSURE: {
                ObjFunction* func = (ObjFunction*) CONST(FETCH()).obj;
                ObjClosure* clos = create_closure(func);
                clos->nupvalues = func->nupvalues;
                clos->upvalues =
                    malloc(clos->nupvalues * sizeof *clos->upvalues);
                for (int i = 0; i < clos->nupvalues; i++) {
                    if (func->upvalues[i].local) {
                        Value* loc = &cur.fp[func->upvalues[i].id];
                        ObjUpvalue** ptr = &vm.open_upvalues;
                        while (*ptr && (*ptr)->loc > loc) ptr = &(*ptr)->next;
                        ObjUpvalue* upval;
                        if (*ptr && (*ptr)->loc == loc) {
                            upval = *ptr;
                        } else {
                            upval =
                                create_upvalue(&cur.fp[func->upvalues[i].id]);
                            upval->next = *ptr;
                            *ptr = upval;
                        }
                        clos->upvalues[i] = upval;
                    } else {
                        clos->upvalues[i] = cur.up[func->upvalues[i].id];
                    }
                }
                PUSH(OBJ_VAL(clos));
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
                close_upvalues(sp);
                break;
            case OP_POPN:
                sp -= FETCH();
                close_upvalues(sp);
                break;
            case OP_NEG: {
                POP(Value a);
                if (a.type != VT_NUMBER) {
                    runtime_error("Invalid operand for unary '-'.");
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
                    runtime_error("Invalid operand for '+'.");
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
                    runtime_error("Invalid operand for '%%'.");
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
                                cur.up = NULL;
                                cur.ip = func->chunk.code.d;
                                if (func->nargs != nargs) {
                                    runtime_error("Invalid argument count, "
                                                  "expected %d, got %d.",
                                                  func->nargs, nargs);
                                    return RUNTIME_ERROR;
                                }
                                break;
                            }
                            case OT_CLOSURE: {
                                ObjClosure* clos = (ObjClosure*) v.obj;
                                ObjFunction* func = clos->f;
                                if (csp - vm.call_stack == MAX_CALLS) {
                                    runtime_error("Max call depth exceeded.");
                                    return RUNTIME_ERROR;
                                }
                                *csp++ = cur;
                                cur.func = func;
                                cur.fp = sp - nargs - 1;
                                cur.up = clos->upvalues;
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
                        if (v.builtin(nargs, sp - nargs - 1) != OK) {
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
                close_upvalues(sp);
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

int run_file(char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NO_FILE;
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    char* program = malloc(len + 1);
    program[len] = '\0';
    fseek(fp, 0, SEEK_SET);
    (void) !fread(program, 1, len, fp);
    fclose(fp);

    int code = interpret(program);

    free(program);
    return code;
}