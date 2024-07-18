#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"

enum {
    PREC_0,
    PREC_NONE,
    PREC_ASSN,
    PREC_OR,
    PREC_AND,
    PREC_EQUAL,
    PREC_COMP,
    PREC_SUM,
    PREC_PROD,
    PREC_UNARY,
    PREC_MAX
};

int infix_prec[TOKEN_MAX] = {
    [TOKEN_EQUAL] = PREC_ASSN,
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
};

struct {
    Token cur;
    Token prev;

    bool hadError;
    bool curError;
} parser;

typedef struct {
    struct {
        Token tok;
        u8 id;
    } globalRefs[256];
    int nglobalrefs;
    struct {
        Token name;
        int depth;
    } locals[256];
    int nlocals;
    int depth;
} Compiler;

Compiler* curState;
Chunk* curChunk;

void compiler_init(Compiler* c) {
    c->nglobalrefs = 0;
    c->nlocals = 0;
    c->depth = 0;
}

void compiler_free(Compiler* c) {}

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

#define EMIT(b) chunk_write(curChunk, b, parser.prev.line)
#define EMIT2(b1, b2) (EMIT(b1), EMIT(b2))
#define EMIT_CONST(v) chunk_push_const(curChunk, v, parser.prev.line)

#define IDENTS_EQUAL(a, b) (a.len == b.len && !memcmp(a.start, b.start, a.len))

u8 global_ref_id(Token tok) {
    for (int i = 0; i < curState->nglobalrefs; i++) {
        if (IDENTS_EQUAL(curState->globalRefs[i].tok, tok)) {
            return curState->globalRefs[i].id;
        }
    }
    u8 id = add_constant(curChunk,
                         OBJ_VAL((Obj*) create_string(tok.start, tok.len)));
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

int emit_jmp(u8 opcode) {
    int instr = curChunk->code.size;
    EMIT(opcode);
    EMIT2(0, 0);
    return instr;
}

void patch_jmp(int instr) {
    int off = curChunk->code.size - (instr + 3);
    curChunk->code.d[instr + 1] = off & 0xff;
    curChunk->code.d[instr + 2] = (off >> 8) & 0xff;
}

void emit_jmp_back(u8 opcode, int dest) {
    int off = dest - (curChunk->code.size + 3);
    EMIT(opcode);
    EMIT2(off & 0xff, (off >> 8) & 0xff);
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
            EMIT_CONST(OBJ_VAL((Obj*) create_string(parser.prev.start + 1,
                                                    parser.prev.len - 2)));
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
                parse_precedence(PREC_ASSN);
                EMIT2(pop_op, id);
                EMIT(OP_PUSH);
            } else {
                EMIT2(push_op, id);
            }
            break;
        default:
            parse_error("Unexpected token.");
            return;
    }

    while (infix_prec[parser.cur.type] >= prec) {
        advance();
        switch (parser.prev.type) {
            case TOKEN_OR: {
                int jmpsc = emit_jmp(OP_JMP_TRUE);
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_POP);
                patch_jmp(jmpsc);
                EMIT(OP_PUSH);
                break;
            }
            case TOKEN_AND: {
                int jmpsc = emit_jmp(OP_JMP_FALSE);
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_POP);
                patch_jmp(jmpsc);
                EMIT(OP_PUSH);
                break;
            }
            case TOKEN_EQUAL:
                parse_error("Invalid assignment.");
                return;
            case TOKEN_EQUAL_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_EQ);
                break;
            case TOKEN_NOT_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT2(OP_EQ, OP_NOT);
                break;
            case TOKEN_LESS:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_LT);
                break;
            case TOKEN_LESS_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT2(OP_GT, OP_NOT);
                break;
            case TOKEN_GREATER:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_GT);
                break;
            case TOKEN_GREATER_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT2(OP_LT, OP_NOT);
                break;
            case TOKEN_PLUS:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_ADD);
                break;
            case TOKEN_MINUS:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_SUB);
                break;
            case TOKEN_STAR:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_MUL);
                break;
            case TOKEN_SLASH:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_DIV);
                break;
            case TOKEN_PERCENT:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_MOD);
                break;
            default:
                break;
        }
    }
}

void parse_expr() {
    parse_precedence(PREC_NONE);
}

void enter_scope() {
    curState->depth++;
}

void leave_scope() {
    for (int i = curState->nlocals - 1; i >= 0; i--) {
        if (curState->locals[i].depth < curState->depth) break;
        EMIT(OP_POP);
        curState->nlocals--;
    }
    curState->depth--;
}

void parse_decl_or_stmt();

void parse_stmt() {
    switch (parser.cur.type) {
        case TOKEN_LEFT_BRACE:
            advance();
            enter_scope();
            while (parser.cur.type != TOKEN_EOF &&
                   parser.cur.type != TOKEN_RIGHT_BRACE) {
                parse_decl_or_stmt();
            }
            leave_scope();
            EXPECT(TOKEN_RIGHT_BRACE);
            break;
        case TOKEN_PRINT:
            advance();
            parse_expr();
            EXPECT(TOKEN_SEMICOLON);
            EMIT(OP_PRINT);
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
            int loopdest = curChunk->code.size;
            EXPECT(TOKEN_LEFT_PAREN);
            parse_expr();
            EXPECT(TOKEN_RIGHT_PAREN);
            int breakjmp = emit_jmp(OP_JMP_FALSE);
            parse_stmt();
            emit_jmp_back(OP_JMP, loopdest);
            patch_jmp(breakjmp);
            break;
        }
        case TOKEN_FOR: {
            advance();
            EXPECT(TOKEN_LEFT_PAREN);
            parse_decl_or_stmt();
            int loopdest = curChunk->code.size;
            parse_expr();
            EXPECT(TOKEN_SEMICOLON);
            int jmpbreak = emit_jmp(OP_JMP_FALSE);
            int jmpbody = emit_jmp(OP_JMP);
            int assndest = curChunk->code.size;
            parse_expr();
            EMIT(OP_POP);
            emit_jmp_back(OP_JMP, loopdest);
            EXPECT(TOKEN_RIGHT_PAREN);
            patch_jmp(jmpbody);
            parse_stmt();
            emit_jmp_back(OP_JMP, assndest);
            patch_jmp(jmpbreak);
            break;
        }
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
        case TOKEN_VAR:
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
            if (curState->depth == 0) {
                u8 id = global_ref_id(id_tok);
                EMIT2(OP_DEF_GLOBAL, id);
            } else {
                curState->locals[curState->nlocals].name = id_tok;
                curState->locals[curState->nlocals].depth = curState->depth;
                curState->nlocals++;
            }
            break;
        default:
            parse_stmt();
    }
}

void synchronize() {
    advance();
    while (parser.cur.type != TOKEN_EOF && parser.prev.type != TOKEN_SEMICOLON)
        advance();
    parser.curError = false;
}

bool compile(char* source, Chunk* chunk) {
    init_scanner(source);
    Compiler compiler;
    compiler_init(&compiler);

    parser.hadError = false;
    parser.curError = false;

    curChunk = chunk;
    curState = &compiler;

    advance();

    while (parser.cur.type != TOKEN_EOF) {
        parse_decl_or_stmt();
        if (parser.curError) synchronize();
    }

    EMIT(OP_RET);

    compiler_free(&compiler);

    return !parser.hadError;
}