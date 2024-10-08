#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define MAX_GLOBAL_REFS 256

enum {
    PREC_0,
    PREC_NONE,
    PREC_COMMA,
    PREC_FUN,
    PREC_ASSN,
    PREC_COND,
    PREC_OR,
    PREC_AND,
    PREC_EQUAL,
    PREC_COMP,
    PREC_SUM,
    PREC_PROD,
    PREC_PREFIX,
    PREC_POSTFIX,
    PREC_PRIMARY,

    PREC_MAX
};

int infix_prec[TOKEN_MAX] = {
    [TOKEN_COMMA] = PREC_COMMA,
    [TOKEN_EQUAL] = PREC_ASSN,
    [TOKEN_PLUS_EQUAL] = PREC_ASSN,
    [TOKEN_MINUS_EQUAL] = PREC_ASSN,
    [TOKEN_STAR_EQUAL] = PREC_ASSN,
    [TOKEN_SLASH_EQUAL] = PREC_ASSN,
    [TOKEN_QUESTION] = PREC_COND,
    [TOKEN_COLON] = PREC_COMMA,
    [TOKEN_OR] = PREC_OR,
    [TOKEN_AND] = PREC_AND,
    [TOKEN_EQUAL_EQUAL] = PREC_EQUAL,
    [TOKEN_NOT_EQUAL] = PREC_EQUAL,
    [TOKEN_LESS] = PREC_COMP,
    [TOKEN_LESS_EQUAL] = PREC_COMP,
    [TOKEN_GREATER] = PREC_COMP,
    [TOKEN_GREATER_EQUAL] = PREC_COMP,
    [TOKEN_PLUS] = PREC_SUM,
    [TOKEN_MINUS] = PREC_SUM,
    [TOKEN_STAR] = PREC_PROD,
    [TOKEN_SLASH] = PREC_PROD,
    [TOKEN_PERCENT] = PREC_PROD,
    [TOKEN_LEFT_PAREN] = PREC_POSTFIX,
    [TOKEN_LEFT_SQUARE] = PREC_POSTFIX,
    [TOKEN_DOT] = PREC_POSTFIX,
};

struct {
    Token cur;
    Token prev;

    bool hadError;
    bool curError;
} parser;

typedef struct _Compiler {
    ObjFunction* f;
    struct _Compiler* parent;

    struct {
        Token tok;
        u8 id;
    } globalRefs[MAX_GLOBAL_REFS];
    int nglobalrefs;
    struct {
        Token name;
        int depth;
    } locals[MAX_LOCALS];
    int nlocals;
    struct {
        u8 id;
        bool local;
    } upvalues[MAX_LOCALS];
    int nupvalues;

    int depth;

    int continueDest;
    int continueDepth;
    Vector(int) breakSrcs;
    int breakDepth;
} Compiler;

Compiler* curState;

#define EMIT(b) chunk_write(&curState->f->chunk, b, parser.prev.line)
#define EMIT2(b1, b2) (EMIT(b1), EMIT(b2))
#define EMIT_CONST(v) chunk_push_const(&curState->f->chunk, v, parser.prev.line)

void compiler_init(Compiler* c) {
    c->f = create_function();
    c->parent = curState;
    c->nglobalrefs = 0;
    c->locals[0].name.start = "";
    c->locals[0].name.len = 0;
    c->locals[0].depth = -1;
    c->nlocals = 1;
    c->nupvalues = 0;
    c->depth = curState ? curState->depth : 0;
    c->continueDepth = -1;
    c->breakDepth = -1;
    curState = c;
}

ObjFunction* compiler_end(bool ret_nil) {
    if (ret_nil) {
        EMIT(OP_PUSH_NIL);
        EMIT(OP_RET);
    }
    ObjFunction* f = curState->f;
    f->nupvalues = curState->nupvalues;
    if (f->nupvalues) {
        f->upvalues = malloc(f->nupvalues * sizeof *f->upvalues);
        memcpy(f->upvalues, curState->upvalues,
               f->nupvalues * sizeof *f->upvalues);
    }
    curState = curState->parent;
#ifdef DEBUG_DISASM
    if (!parser.hadError) disassemble_function(f);
    if (f->nupvalues) {
        eprintf("Closes over:");
        for (int i = 0; i < f->nupvalues; i++) {
            eprintf(" %s$%d", f->upvalues[i].local ? "local" : "upvalue",
                    f->upvalues[i].id);
        }
        eprintf("\n");
    }
#endif
    return f;
}

void parse_error(char* message) {
    parser.hadError = true;
    if (parser.curError) return;
    parser.curError = true;

    eprintf("Error line %d: ", parser.cur.line);
    if (parser.cur.type == TOKEN_ERROR) {
        eprintf("%s\n", parser.cur.start);
        return;
    }
    if (parser.cur.type == TOKEN_EOF) {
        eprintf("at EOF: ");
    } else {
        eprintf("at '%.*s': ", parser.cur.len, parser.cur.start);
    }
    eprintf("%s\n", message);
}

void advance() {
    parser.prev = parser.cur;

    while (true) {
        parser.cur = next_token();
        if (parser.cur.type != TOKEN_ERROR) break;
        parse_error(NULL);
    }
}

void parse_expr();

#define IDENTS_EQUAL(a, b) (a.len == b.len && !memcmp(a.start, b.start, a.len))

u8 global_ref_id(Token tok) {
    for (int i = 0; i < curState->nglobalrefs; i++) {
        if (IDENTS_EQUAL(curState->globalRefs[i].tok, tok)) {
            return curState->globalRefs[i].id;
        }
    }
    u8 id = add_constant(&curState->f->chunk,
                         OBJ_VAL(create_string(tok.start, tok.len)));
    curState->globalRefs[curState->nglobalrefs].tok = tok;
    curState->globalRefs[curState->nglobalrefs].id = id;
    curState->nglobalrefs++;
    return id;
}

#define EXPECT(t)                                                              \
    if (parser.cur.type != t) {                                                \
        parse_error("Expected " #t ".");                                       \
        return;                                                                \
    } else advance();

#define PARSE_RHS_LA() parse_precedence(infix_prec[parser.prev.type] + 1)
#define PARSE_RHS_RA() parse_precedence(infix_prec[parser.prev.type])

#define CUR_POS (curState->f->chunk.code.size)

int emit_jmp(u8 opcode) {
    int instr = CUR_POS;
    EMIT(opcode);
    EMIT2(0, 0);
    return instr;
}

void patch_jmp(int instr) {
    int off = CUR_POS - (instr + 3);
    curState->f->chunk.code.d[instr + 1] = off & 0xff;
    curState->f->chunk.code.d[instr + 2] = (off >> 8) & 0xff;
}

void emit_jmp_back(u8 opcode, int dest) {
    int off = dest - (CUR_POS + 3);
    EMIT(opcode);
    EMIT2(off & 0xff, (off >> 8) & 0xff);
}

void parse_decl_or_stmt();

void enter_scope() {
    curState->depth++;
}

int pop_to_depth(int d) {
    int npop = curState->nlocals;
    for (int i = curState->nlocals - 1; i >= 0; i--) {
        if (curState->locals[i].depth <= d) {
            npop = curState->nlocals - i - 1;
            break;
        }
    }
    if (npop == 1) EMIT(OP_POP);
    else if (npop > 1) EMIT2(OP_POPN, npop);
    return npop;
}

void leave_scope() {
    curState->depth--;
    curState->nlocals -= pop_to_depth(curState->depth);
}

void synchronize() {
    advance();
    while (parser.cur.type != TOKEN_EOF &&
           parser.cur.type != TOKEN_LEFT_CURLY &&
           parser.cur.type != TOKEN_RIGHT_CURLY &&
           parser.prev.type != TOKEN_SEMICOLON)
        advance();
    parser.curError = false;
}

void parse_block() {
    while (parser.cur.type != TOKEN_EOF &&
           parser.cur.type != TOKEN_RIGHT_CURLY) {
        parse_decl_or_stmt();
        if (parser.curError) synchronize();
    }
    EXPECT(TOKEN_RIGHT_CURLY);
}

void define_var(Token id_tok) {
    if (curState->depth == 0) {
        u8 id = global_ref_id(id_tok);
        EMIT2(OP_DEF_GLOBAL, id);
    } else {
        curState->locals[curState->nlocals].name = id_tok;
        curState->locals[curState->nlocals].depth = curState->depth;
        curState->nlocals++;
    }
}

void parse_precedence(int prec);

void parse_function(ObjString* name, bool expr) {
    Compiler compiler;
    compiler_init(&compiler);

    enter_scope();
    EXPECT(TOKEN_LEFT_PAREN);
    while (parser.cur.type != TOKEN_EOF &&
           parser.cur.type != TOKEN_RIGHT_PAREN) {
        EXPECT(TOKEN_IDENTIFIER);
        define_var(parser.prev);
        if (parser.cur.type != TOKEN_RIGHT_PAREN) {
            EXPECT(TOKEN_COMMA);
        }
    }
    EXPECT(TOKEN_RIGHT_PAREN);

    curState->f->name = name;
    curState->f->nargs = curState->nlocals - 1;

    bool nil_ret;

    switch (parser.cur.type) {
        case TOKEN_LEFT_CURLY: {
            advance();
            enter_scope();
            parse_block();
            nil_ret = true;
            break;
        }
        case TOKEN_ARROW: {
            advance();
            parse_precedence(PREC_ASSN);
            EMIT(OP_RET);
            if (!expr) {
                EXPECT(TOKEN_SEMICOLON)
            }
            nil_ret = false;
            break;
        }
        default:
            parse_error("Expected block or arrow function.");
            return;
    }

    ObjFunction* func = compiler_end(nil_ret);

    if (func->nupvalues) {
        u8 id = add_constant(&curState->f->chunk, OBJ_VAL(func));
        EMIT2(OP_PUSH_CLOSURE, id);
    } else {
        EMIT_CONST(OBJ_VAL(func));
    }
}

int resolve_local(Compiler* compiler, Token id_tok) {
    for (int id = compiler->nlocals - 1; id >= 0; id--) {
        if (IDENTS_EQUAL(compiler->locals[id].name, id_tok)) return id;
    }
    return -1;
}

int resolve_upvalue(Compiler* compiler, Token id_tok) {
    if (!compiler->parent) return -1;
    int id = resolve_local(compiler->parent, id_tok);
    bool local = true;
    if (id == -1) {
        id = resolve_upvalue(compiler->parent, id_tok);
        local = false;
        if (id == -1) return -1;
    }
    for (int i = compiler->nupvalues - 1; i >= 0; i--) {
        if (compiler->upvalues[i].id == id &&
            compiler->upvalues[i].local == local)
            return i;
    }
    compiler->upvalues[compiler->nupvalues].id = id;
    compiler->upvalues[compiler->nupvalues].local = local;
    return compiler->nupvalues++;
}

char escape_char(char c) {
    switch (c) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        default:
            return c;
    }
}

void parse_precedence(int prec) {
    switch (parser.cur.type) {
        case TOKEN_MINUS:
            advance();
            parse_precedence(PREC_PREFIX);
            EMIT(OP_NEG);
            break;
        case TOKEN_NOT:
            advance();
            parse_precedence(PREC_PREFIX);
            EMIT(OP_NOT);
            break;
        case TOKEN_LEFT_PAREN:
            advance();
            parse_precedence(PREC_NONE);
            EXPECT(TOKEN_RIGHT_PAREN);
            break;
        case TOKEN_NUMBER:
            advance();
            EMIT_CONST(NUMBER_VAL(strtod(parser.prev.start, NULL)));
            break;
        case TOKEN_TRUE:
            advance();
            EMIT(OP_PUSH_TRUE);
            break;
        case TOKEN_FALSE:
            advance();
            EMIT(OP_PUSH_FALSE);
            break;
        case TOKEN_NIL:
            advance();
            EMIT(OP_PUSH_NIL);
            break;
        case TOKEN_STRING: {
            advance();
            char* buf = malloc(parser.prev.len);
            char* p = buf;
            for (int i = 1; i < parser.prev.len - 1; i++) {
                if (parser.prev.start[i] == '\\') {
                    i++;
                    *p++ = escape_char(parser.prev.start[i]);
                } else {
                    *p++ = parser.prev.start[i];
                }
            }
            EMIT_CONST(OBJ_VAL(create_string(buf, p - buf)));
            free(buf);
            break;
        }
        case TOKEN_CHAR: {
            advance();
            char c;
            if (parser.prev.start[1] == '\\') {
                c = escape_char(parser.prev.start[2]);
            } else {
                c = parser.prev.start[1];
            }
            EMIT_CONST(CHAR_VAL(c));
            break;
        }
        case TOKEN_IDENTIFIER:
            advance();

            u8 push_op = OP_PUSH_LOCAL, pop_op = OP_POP_LOCAL;

            int id = resolve_local(curState, parser.prev);

            if (id == -1) {
                id = resolve_upvalue(curState, parser.prev);
                push_op = OP_PUSH_UPVALUE, pop_op = OP_POP_UPVALUE;
                if (id == -1) {
                    id = global_ref_id(parser.prev);
                    push_op = OP_PUSH_GLOBAL, pop_op = OP_POP_GLOBAL;
                }
            }

            if (prec <= PREC_ASSN) {
                switch (parser.cur.type) {
                    case TOKEN_EQUAL:
                        advance();
                        PARSE_RHS_RA();
                        EMIT2(pop_op, id);
                        EMIT(OP_PUSH);
                        break;
                    case TOKEN_PLUS_EQUAL:
                        advance();
                        EMIT2(push_op, id);
                        PARSE_RHS_RA();
                        EMIT(OP_ADD);
                        EMIT2(pop_op, id);
                        EMIT(OP_PUSH);
                        break;
                    case TOKEN_MINUS_EQUAL:
                        advance();
                        EMIT2(push_op, id);
                        PARSE_RHS_RA();
                        EMIT(OP_SUB);
                        EMIT2(pop_op, id);
                        EMIT(OP_PUSH);
                        break;
                    case TOKEN_STAR_EQUAL:
                        advance();
                        EMIT2(push_op, id);
                        PARSE_RHS_RA();
                        EMIT(OP_MUL);
                        EMIT2(pop_op, id);
                        EMIT(OP_PUSH);
                        break;
                    case TOKEN_SLASH_EQUAL:
                        advance();
                        EMIT2(push_op, id);
                        PARSE_RHS_RA();
                        EMIT(OP_DIV);
                        EMIT2(pop_op, id);
                        EMIT(OP_PUSH);
                        break;
                    default:
                        EMIT2(push_op, id);
                }
            } else {
                EMIT2(push_op, id);
            }
            break;
        case TOKEN_FUN: {
            advance();

            parse_function(NULL, true);
            break;
        }
        case TOKEN_ARRAY: {
            advance();
            EXPECT(TOKEN_LEFT_SQUARE);
            parse_expr();
            EXPECT(TOKEN_RIGHT_SQUARE);
            EMIT(OP_PUSH_ARRAY);
            break;
        }
        case TOKEN_LEFT_SQUARE: {
            advance();
            int len = 0;
            while (parser.cur.type != TOKEN_EOF &&
                   parser.cur.type != TOKEN_RIGHT_SQUARE) {
                parse_precedence(PREC_ASSN);
                len++;
                if (parser.cur.type != TOKEN_RIGHT_SQUARE) {
                    EXPECT(TOKEN_COMMA);
                }
            }
            EXPECT(TOKEN_RIGHT_SQUARE);
            EMIT2(OP_PUSH_ARRAY_INIT, len);
            break;
        }
        default:
            parse_error("Unexpected token.");
            return;
    }

    while (infix_prec[parser.cur.type] >= prec) {
        switch (parser.cur.type) {
            case TOKEN_COMMA:
                advance();
                EMIT(OP_POP);
                PARSE_RHS_RA();
                break;
            case TOKEN_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_PLUS_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_MINUS_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_STAR_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_SLASH_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_QUESTION: {
                advance();
                int ifjmp = emit_jmp(OP_JMP_FALSE);
                PARSE_RHS_RA();
                EXPECT(TOKEN_COLON);
                int elsejmp = emit_jmp(OP_JMP);
                patch_jmp(ifjmp);
                PARSE_RHS_RA();
                patch_jmp(elsejmp);
                break;
            }
            case TOKEN_COLON:
                return;
            case TOKEN_OR: {
                advance();
                int jmpsc = emit_jmp(OP_JMP_TRUE);
                PARSE_RHS_RA();
                EMIT(OP_POP);
                patch_jmp(jmpsc);
                EMIT(OP_PUSH);
                break;
            }
            case TOKEN_AND: {
                advance();
                int jmpsc = emit_jmp(OP_JMP_FALSE);
                PARSE_RHS_RA();
                EMIT(OP_POP);
                patch_jmp(jmpsc);
                EMIT(OP_PUSH);
                break;
            }
            case TOKEN_EQUAL_EQUAL:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_TEQ);
                break;
            case TOKEN_NOT_EQUAL:
                advance();
                PARSE_RHS_LA();
                EMIT2(OP_TEQ, OP_NOT);
                break;
            case TOKEN_LESS:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_TLT);
                break;
            case TOKEN_LESS_EQUAL:
                advance();
                PARSE_RHS_LA();
                EMIT2(OP_TGT, OP_NOT);
                break;
            case TOKEN_GREATER:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_TGT);
                break;
            case TOKEN_GREATER_EQUAL:
                advance();
                PARSE_RHS_LA();
                EMIT2(OP_TLT, OP_NOT);
                break;
            case TOKEN_PLUS:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_ADD);
                break;
            case TOKEN_MINUS:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_SUB);
                break;
            case TOKEN_STAR:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_MUL);
                break;
            case TOKEN_SLASH:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_DIV);
                break;
            case TOKEN_PERCENT:
                advance();
                PARSE_RHS_LA();
                EMIT(OP_MOD);
                break;
            case TOKEN_LEFT_PAREN: {
                advance();
                int nargs = 0;
                while (parser.cur.type != TOKEN_EOF &&
                       parser.cur.type != TOKEN_RIGHT_PAREN) {
                    parse_precedence(PREC_ASSN);
                    nargs++;
                    if (parser.cur.type != TOKEN_RIGHT_PAREN) {
                        EXPECT(TOKEN_COMMA);
                    }
                }
                EXPECT(TOKEN_RIGHT_PAREN);
                EMIT2(OP_CALL, nargs);
                break;
            }
            case TOKEN_LEFT_SQUARE: {
                advance();

                parse_expr();
                EXPECT(TOKEN_RIGHT_SQUARE);

                if (prec <= PREC_ASSN && parser.cur.type == TOKEN_EQUAL) {
                    advance();
                    PARSE_RHS_RA();
                    EMIT(OP_SETITEM);
                } else {
                    EMIT(OP_GETITEM);
                }
                break;
            }
            case TOKEN_DOT: {
                advance();
                EXPECT(TOKEN_IDENTIFIER);
                u8 id = global_ref_id(parser.prev);
                if (prec <= PREC_ASSN && parser.cur.type == TOKEN_EQUAL) {
                    advance();
                    PARSE_RHS_RA();
                    EMIT2(OP_SETATTR, id);
                } else {
                    EMIT2(OP_GETATTR, id);
                }
                break;
            }
            default:
                break;
        }
    }
}

void parse_expr() {
    parse_precedence(PREC_NONE);
}

void parse_stmt() {
    switch (parser.cur.type) {
        case TOKEN_LEFT_CURLY:
            advance();
            enter_scope();
            parse_block();
            leave_scope();
            break;
        case TOKEN_IF: {
            advance();
            EXPECT(TOKEN_LEFT_PAREN);
            parse_expr();
            EXPECT(TOKEN_RIGHT_PAREN);
            int ifjmp = emit_jmp(OP_JMP_FALSE);
            parse_stmt();
            if (parser.cur.type == TOKEN_ELSE) {
                advance();
                int elsejmp = emit_jmp(OP_JMP);
                patch_jmp(ifjmp);
                parse_stmt();
                patch_jmp(elsejmp);
            } else {
                patch_jmp(ifjmp);
            }
            break;
        }
        case TOKEN_WHILE: {
            advance();

            int loopdest = CUR_POS;

            int oldContinueDest = curState->continueDest;
            int oldContinueDepth = curState->continueDepth;
            curState->continueDest = loopdest;
            curState->continueDepth = curState->depth;

            EXPECT(TOKEN_LEFT_PAREN);
            parse_expr();
            EXPECT(TOKEN_RIGHT_PAREN);

            int breakjmp = emit_jmp(OP_JMP_FALSE);

            Vector(int) oldBreakSrcs;
            Vec_copy(oldBreakSrcs, curState->breakSrcs);
            int oldBreakDepth = curState->breakDepth;
            Vec_init(curState->breakSrcs);
            curState->breakDepth = curState->depth;

            parse_stmt();

            emit_jmp_back(OP_JMP, loopdest);
            patch_jmp(breakjmp);
            for (int i = 0; i < curState->breakSrcs.size; i++) {
                patch_jmp(curState->breakSrcs.d[i]);
            }

            Vec_free(curState->breakSrcs);
            Vec_copy(curState->breakSrcs, oldBreakSrcs);
            curState->breakDepth = oldBreakDepth;
            curState->continueDest = oldContinueDest;
            curState->continueDepth = oldContinueDepth;
            break;
        }
        case TOKEN_FOR: {
            advance();

            enter_scope();

            EXPECT(TOKEN_LEFT_PAREN);
            parse_decl_or_stmt();

            int loopdest = CUR_POS;

            if (parser.cur.type == TOKEN_SEMICOLON) {
                advance();
                EMIT(OP_PUSH_TRUE);
            } else {
                parse_expr();
                EXPECT(TOKEN_SEMICOLON);
            }

            int jmpbreak = emit_jmp(OP_JMP_FALSE);
            int jmpbody = emit_jmp(OP_JMP);

            int assndest = CUR_POS;
            int oldContinueDest = curState->continueDest;
            int oldContinueDepth = curState->continueDepth;
            curState->continueDest = assndest;
            curState->continueDepth = curState->depth;
            if (parser.cur.type == TOKEN_RIGHT_PAREN) {
                advance();
            } else {
                parse_expr();
                EMIT(OP_POP);
                EXPECT(TOKEN_RIGHT_PAREN);
            }
            emit_jmp_back(OP_JMP, loopdest);
            patch_jmp(jmpbody);

            Vector(int) oldBreakSrcs;
            Vec_copy(oldBreakSrcs, curState->breakSrcs);
            int oldBreakDepth = curState->breakDepth;
            Vec_init(curState->breakSrcs);
            curState->breakDepth = curState->depth;

            parse_stmt();
            emit_jmp_back(OP_JMP, assndest);

            patch_jmp(jmpbreak);
            for (int i = 0; i < curState->breakSrcs.size; i++) {
                patch_jmp(curState->breakSrcs.d[i]);
            }

            Vec_free(curState->breakSrcs);
            Vec_copy(curState->breakSrcs, oldBreakSrcs);
            curState->breakDepth = oldBreakDepth;

            curState->continueDest = oldContinueDest;
            curState->continueDepth = oldContinueDepth;

            leave_scope();
            break;
        }
        case TOKEN_DO: {
            advance();

            int loopdest = CUR_POS;

            Vector(int) oldBreakSrcs;
            Vec_copy(oldBreakSrcs, curState->breakSrcs);
            int oldBreakDepth = curState->breakDepth;
            Vec_init(curState->breakSrcs);
            curState->breakDepth = curState->depth;

            int oldContinueDest = curState->continueDest;
            int oldContinueDepth = curState->continueDepth;
            curState->continueDest = loopdest;
            curState->continueDepth = curState->depth;

            parse_stmt();

            EXPECT(TOKEN_WHILE);
            EXPECT(TOKEN_LEFT_PAREN);
            parse_expr();
            EXPECT(TOKEN_RIGHT_PAREN);
            EXPECT(TOKEN_SEMICOLON);
            emit_jmp_back(OP_JMP_TRUE, loopdest);

            for (int i = 0; i < curState->breakSrcs.size; i++) {
                patch_jmp(curState->breakSrcs.d[i]);
            }

            Vec_free(curState->breakSrcs);
            Vec_copy(curState->breakSrcs, oldBreakSrcs);
            curState->breakDepth = oldBreakDepth;
            curState->continueDest = oldContinueDest;
            curState->continueDepth = oldContinueDepth;

            break;
        }
        case TOKEN_SWITCH: {
            advance();

            enter_scope();

            EXPECT(TOKEN_LEFT_PAREN);
            parse_expr();
            EXPECT(TOKEN_RIGHT_PAREN);
            curState->locals[curState->nlocals].name.start = "";
            curState->locals[curState->nlocals].name.len = 0;
            curState->locals[curState->nlocals].depth = curState->depth;
            int comp_local = curState->nlocals++;

            Vector(int) oldBreakSrcs;
            Vec_copy(oldBreakSrcs, curState->breakSrcs);
            int oldBreakDepth = curState->breakDepth;
            Vec_init(curState->breakSrcs);
            curState->breakDepth = curState->depth;

            EXPECT(TOKEN_LEFT_CURLY);

            int casejmp = emit_jmp(OP_JMP);

            while (parser.cur.type != TOKEN_EOF &&
                   parser.cur.type != TOKEN_RIGHT_CURLY) {
                switch (parser.cur.type) {
                    case TOKEN_CASE: {
                        advance();
                        int skipjmp = emit_jmp(OP_JMP);
                        patch_jmp(casejmp);
                        EMIT2(OP_PUSH_LOCAL, comp_local);
                        parse_expr();
                        EMIT(OP_TEQ);
                        EXPECT(TOKEN_COLON);
                        casejmp = emit_jmp(OP_JMP_FALSE);
                        patch_jmp(skipjmp);
                        break;
                    }
                    case TOKEN_DEFAULT: {
                        advance();
                        EXPECT(TOKEN_COLON);
                        patch_jmp(casejmp);
                        casejmp = -1;
                        break;
                    }
                    default:
                        parse_stmt();
                }
                if (parser.curError) synchronize();
            }
            EXPECT(TOKEN_RIGHT_CURLY);

            if (casejmp != -1) patch_jmp(casejmp);

            for (int i = 0; i < curState->breakSrcs.size; i++) {
                patch_jmp(curState->breakSrcs.d[i]);
            }

            Vec_free(curState->breakSrcs);
            Vec_copy(curState->breakSrcs, oldBreakSrcs);
            curState->breakDepth = oldBreakDepth;

            leave_scope();

            break;
        }
        case TOKEN_BREAK:
            if (curState->breakDepth == -1) {
                parse_error("Cannot use break outside loop or switch.");
                return;
            }
            advance();
            EXPECT(TOKEN_SEMICOLON);
            pop_to_depth(curState->breakDepth);
            Vec_push(curState->breakSrcs, emit_jmp(OP_JMP));
            break;
        case TOKEN_CONTINUE:
            if (curState->continueDepth == -1) {
                parse_error("Cannot use continue outside loop.");
                return;
            }
            advance();
            EXPECT(TOKEN_SEMICOLON);
            pop_to_depth(curState->continueDepth);
            emit_jmp_back(OP_JMP, curState->continueDest);
            break;
        case TOKEN_RETURN:
            advance();
            switch (parser.cur.type) {
                case TOKEN_SEMICOLON:
                    advance();
                    EMIT(OP_PUSH_NIL);
                    break;
                default:
                    parse_expr();
                    EXPECT(TOKEN_SEMICOLON);
            }
            EMIT(OP_RET);
            break;
        case TOKEN_SEMICOLON:
            advance();
            break;
        default:
            parse_expr();
            EXPECT(TOKEN_SEMICOLON);
            EMIT(OP_POP);
            break;
    }
}

void parse_decl_or_stmt() {
    switch (parser.cur.type) {
        case TOKEN_VAR: {
            advance();
            EXPECT(TOKEN_IDENTIFIER);
            Token id_tok = parser.prev;
            switch (parser.cur.type) {
                case TOKEN_EQUAL:
                    advance();
                    parse_expr();
                    EXPECT(TOKEN_SEMICOLON);
                    break;
                case TOKEN_SEMICOLON:
                    advance();
                    EMIT(OP_PUSH_NIL);
                    break;
                default:
                    parse_error("Expected initializer or semicolon.");
            }
            define_var(id_tok);
            break;
        }
        case TOKEN_FUN: {
            advance();
            EXPECT(TOKEN_IDENTIFIER);
            Token id_tok = parser.prev;

            parse_function(create_string(id_tok.start, id_tok.len), false);
            define_var(id_tok);

            break;
        }
        case TOKEN_CLASS: {
            advance();
            EXPECT(TOKEN_IDENTIFIER);
            Token id_tok = parser.prev;
            ObjClass* cls =
                create_class(create_string(id_tok.start, id_tok.len));
            EXPECT(TOKEN_LEFT_CURLY);
            EXPECT(TOKEN_RIGHT_CURLY);
            EMIT_CONST(OBJ_VAL(cls));
            define_var(id_tok);

            break;
        }
        default:
            parse_stmt();
    }
}

ObjFunction* compile(char* source) {

    init_scanner(source);
    curState = NULL;
    Compiler compiler;
    compiler_init(&compiler);
    curState->f->name = CREATE_STRING_LITERAL("script");

    parser.hadError = false;
    parser.curError = false;

    advance();

    while (parser.cur.type != TOKEN_EOF) {
        parse_decl_or_stmt();
        if (parser.curError) synchronize();
    }

    ObjFunction* toplevel = compiler_end(true);

    return parser.hadError ? NULL : toplevel;
}
