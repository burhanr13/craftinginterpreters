#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "object.h"
#include "scanner.h"
#include "value.h"

struct {
    Token cur;
    Token prev;

    bool hadError;
    bool curError;
} parser;

enum {
    PREC_0,
    PREC_NONE,
    PREC_EQUAL,
    PREC_COMP,
    PREC_SUM,
    PREC_PROD,
    PREC_UNARY,
    PREC_MAX
};

int infix_prec[TOKEN_MAX] = {
    [TOKEN_EQUAL_EQUAL] = PREC_EQUAL, [TOKEN_NOT_EQUAL] = PREC_EQUAL,
    [TOKEN_LESS] = PREC_COMP,         [TOKEN_LESS_EQUAL] = PREC_COMP,
    [TOKEN_GREATER] = PREC_COMP,      [TOKEN_GREATER_EQUAL] = PREC_COMP,
    [TOKEN_PLUS] = PREC_SUM,          [TOKEN_MINUS] = PREC_SUM,
    [TOKEN_STAR] = PREC_PROD,         [TOKEN_SLASH] = PREC_PROD,
};

Chunk* curChunk;

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

#define EMIT(b) chunk_write(curChunk, b, parser.prev.line)
#define EMIT_CONST(v) chunk_push_const(curChunk, v, parser.prev.line)

#define EXPECT(t)                                                              \
    if (parser.cur.type != t) parse_error("Expected " #t ".");                 \
    else advance();

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
        default:
            parse_error("Unexpected token.");
            return;
    }

    while (infix_prec[parser.cur.type] >= prec) {
        advance();
        switch (parser.prev.type) {
            case TOKEN_EQUAL_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_EQ);
                break;
            case TOKEN_NOT_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_EQ);
                EMIT(OP_NOT);
                break;
            case TOKEN_LESS:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_LT);
                break;
            case TOKEN_LESS_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_GT);
                EMIT(OP_NOT);
                break;
            case TOKEN_GREATER:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_GT);
                break;
            case TOKEN_GREATER_EQUAL:
                parse_precedence(infix_prec[parser.prev.type] + 1);
                EMIT(OP_LT);
                EMIT(OP_NOT);
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
        case TOKEN_PRINT:
            advance();
            parse_expr();
            EXPECT(TOKEN_SEMICOLON);
            EMIT(OP_PRINT);
            break;
        default:
            parse_expr();
            EXPECT(TOKEN_SEMICOLON);
            break;
    }
}

void parse_decl_or_stmt() {
    parse_stmt();
}

bool compile(char* source, Chunk* chunk) {
    init_scanner(source);

    parser.hadError = false;
    parser.curError = false;

    curChunk = chunk;

    advance();

    while (parser.cur.type != TOKEN_EOF) {
        parse_decl_or_stmt();
    }

    EMIT(OP_RET);

    return !parser.hadError;
}