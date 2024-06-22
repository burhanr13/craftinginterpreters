#include "compiler.h"

#include <stdio.h>

#include "chunk.h"
#include "scanner.h"

struct {
    Token cur;
    Token prev;

    bool hadError;
    bool curError;
} parser;

enum { PREC_NONE, PREC_SUM, PREC_PROD, PREC_UNARY, PREC_MAX };

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

#define EMIT(b) chunk_write(curChunk, b, parser.prev.line)

void advance() {
    parser.prev = parser.cur;

    while (true) {
        parser.cur = next_token();
        if (parser.cur.type != TOKEN_ERROR) break;
        parse_error(NULL);
    }
}

#define EXPECT(t)                                                              \
    if (parser.cur.type != t) parse_error("Expected " #t ".");                 \
    else advance();

void parse_precedence(int prec) {
    switch (parser.cur.type) {
        case TOKEN_MINUS:
            break;
        case TOKEN_LEFT_PAREN:
            advance();
            parse_precedence(PREC_NONE);

            break;
        case TOKEN_NUMBER:
            break;
        default:
            parse_error("Unexpected token.");
            return;
    }
}

void parse_expr() {
    parse_precedence(PREC_NONE);
}

bool compile(char* source, Chunk* chunk) {
    init_scanner(source);

    parser.hadError = false;
    parser.curError = false;

    curChunk = chunk;

    advance();

    parse_expr();

    if (parser.cur.type != TOKEN_EOF) {
        parse_error("Expected EOF.");
    }

    EMIT(OP_RET);

    return !parser.hadError;
}