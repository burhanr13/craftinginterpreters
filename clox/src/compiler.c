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
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,

    PREC_MAX
};

int infix_prec[TOKEN_MAX] = {
    [TOKEN_COMMA] = PREC_COMMA,
    [TOKEN_EQUAL] = PREC_ASSN,
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
    [TOKEN_LEFT_PAREN] = PREC_CALL,
    [TOKEN_DOT] = PREC_CALL,
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
    c->depth = curState ? curState->depth : 0;
    c->continueDepth = -1;
    c->breakDepth = -1;
    curState = c;
}

ObjFunction* compiler_end() {
    EMIT(OP_PUSH_NIL);
    EMIT(OP_RET);
#ifdef DEBUG_INFO
    if (!parser.hadError) disassemble_function(curState->f);
#endif
    ObjFunction* f = curState->f;
    curState = curState->parent;
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
           parser.cur.type != TOKEN_LEFT_BRACE &&
           parser.cur.type != TOKEN_RIGHT_BRACE &&
           parser.prev.type != TOKEN_SEMICOLON)
        advance();
    parser.curError = false;
}

void parse_block() {
    while (parser.cur.type != TOKEN_EOF &&
           parser.cur.type != TOKEN_RIGHT_BRACE) {
        parse_decl_or_stmt();
        if (parser.curError) synchronize();
    }
    EXPECT(TOKEN_RIGHT_BRACE);
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

    switch (parser.cur.type) {
        case TOKEN_LEFT_BRACE: {
            advance();
            enter_scope();
            parse_block();
            break;
        }
        case TOKEN_ARROW: {
            advance();
            parse_precedence(PREC_ASSN);
            EMIT(OP_RET);
            if (!expr) {
                EXPECT(TOKEN_SEMICOLON)
            };
            break;
        }
        default:
            parse_error("Expected block or arrow function.");
            return;
    }

    ObjFunction* func = compiler_end();

    EMIT_CONST(OBJ_VAL(func));
}

void parse_precedence(int prec) {
    switch (parser.cur.type) {
        case TOKEN_MINUS:
            advance();
            parse_precedence(PREC_UNARY);
            EMIT(OP_NEG);
            break;
        case TOKEN_NOT:
            advance();
            parse_precedence(PREC_UNARY);
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
        case TOKEN_STRING:
            advance();
            EMIT_CONST(OBJ_VAL(
                create_string(parser.prev.start + 1, parser.prev.len - 2)));
            break;
        case TOKEN_IDENTIFIER:
            advance();
            u8 push_op = OP_PUSH_LOCAL, pop_op = OP_POP_LOCAL;
            int id;
            for (id = curState->nlocals - 1; id >= 0; id--) {
                if (IDENTS_EQUAL(curState->locals[id].name, parser.prev)) break;
            }
            if (id == -1) {
                id = global_ref_id(parser.prev);
                push_op = OP_PUSH_GLOBAL, pop_op = OP_POP_GLOBAL;
            }
            if (parser.cur.type == TOKEN_EQUAL && prec <= PREC_ASSN) {
                advance();
                PARSE_RHS_RA();
                EMIT2(pop_op, id);
                EMIT(OP_PUSH);
            } else {
                EMIT2(push_op, id);
            }
            break;
        case TOKEN_FUN: {
            advance();

            parse_function(NULL, true);
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
            case TOKEN_DOT:
                break;
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
        case TOKEN_LEFT_BRACE:
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

            EXPECT(TOKEN_LEFT_BRACE);

            int casejmp = emit_jmp(OP_JMP);

            while (parser.cur.type != TOKEN_EOF &&
                   parser.cur.type != TOKEN_RIGHT_BRACE) {
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
            EXPECT(TOKEN_RIGHT_BRACE);

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

    ObjFunction* toplevel = compiler_end();

    return parser.hadError ? NULL : toplevel;
}
