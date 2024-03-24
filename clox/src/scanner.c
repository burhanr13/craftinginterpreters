#include "scanner.h"

#include <string.h>

#include "types.h"

struct {
    char* start;
    char* cur;
    int line;
} scanner;

void init_scanner(char* source) {
    scanner.start = scanner.cur = source;
    scanner.line = 1;
}

Token make_token(TokenType type) {
    Token t;
    t.start = scanner.start;
    t.len = scanner.cur - scanner.start;
    t.type = type;
    t.line = scanner.line;
    return t;
}

Token error_token(char* message) {
    Token t;
    t.start = message;
    t.type = TOKEN_ERROR;
    t.line = scanner.line;
    return t;
}

Token next_token() {
    scanner.start = scanner.cur;

    char c = *scanner.cur++;

    if ('0' <= c && c <= '9') {
        bool saw_dot = false;
        while (('0' <= *scanner.cur && *scanner.cur <= '9') ||
               (!saw_dot && *scanner.cur == '.')) {
            if (*scanner.cur == '.') saw_dot = true;
            scanner.cur++;
        }
        return make_token(TOKEN_NUMBER);
    }

    if ('a' <= c && c <= 'z' || 'A' <= c && c <= 'Z' || c == '_') {
        while ('0' <= *scanner.cur && *scanner.cur <= '9' ||
               'a' <= *scanner.cur && *scanner.cur <= 'z' ||
               'A' <= *scanner.cur && *scanner.cur <= 'Z' ||
               *scanner.cur == '_') {
            scanner.cur++;
        }
        return make_token(TOKEN_IDENTIFIER);
    }

    switch (c) {
        case '\0':
            return make_token(TOKEN_EOF);
        case '\n':
            scanner.line++;
            return next_token();
        case ' ':
        case '\t':
            return next_token();
        case '(':
            return make_token(TOKEN_LEFT_PAREN);
        case ')':
            return make_token(TOKEN_RIGHT_PAREN);
        case '{':
            return make_token(TOKEN_LEFT_BRACE);
        case '}':
            return make_token(TOKEN_RIGHT_BRACE);
        case ';':
            return make_token(TOKEN_SEMICOLON);
        case ',':
            return make_token(TOKEN_COMMA);
        case '.':
            return make_token(TOKEN_DOT);
        case '-':
            return make_token(TOKEN_MINUS);
        case '+':
            return make_token(TOKEN_PLUS);
        case '*':
            return make_token(TOKEN_STAR);
        case '!':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_NOT_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_NOT);
        case '=':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_EQUAL_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_EQUAL);
        case '<':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_LESS_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_LESS);
        case '>':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_GREATER_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_GREATER);
        case '/':
            if (*scanner.cur++ == '/') {
                while (*scanner.cur != '\n' && *scanner.cur != '\0') {
                    scanner.cur++;
                }
                return next_token();
            }
            scanner.cur--;
            return make_token(TOKEN_SLASH);
        case '"':
            while (*scanner.cur != '"') {
                if (*scanner.cur == '\n' || *scanner.cur == '\0')
                    return error_token("Unterminated string.");
                scanner.cur++;
            }
            scanner.cur++;
            return make_token(TOKEN_STRING);
        default:
            return error_token("Lexer error.");
    }
}