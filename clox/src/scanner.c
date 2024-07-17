#include "scanner.h"

#include <ctype.h>
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

#define __KWD(kw, type)                                            \
    return strncmp(p, kw, sizeof kw - 1) ? TOKEN_IDENTIFIER : type

TokenType identifierType() {
    char* p = scanner.start;
    switch (*p++) {
        case 'a':
            __KWD("nd", TOKEN_AND);
        case 'c':
            __KWD("lass", TOKEN_CLASS);
        case 'e':
            __KWD("lse", TOKEN_ELSE);
        case 'f':
            switch(*p++){
                case 'a':
                    __KWD("lse", TOKEN_FALSE);
                case 'o':
                    __KWD("r", TOKEN_FOR);
                case 'u':
                    __KWD("n", TOKEN_FUN);
            }
            break;
        case 'i':
            __KWD("f", TOKEN_IF);
        case 'n':
            __KWD("il", TOKEN_NIL);
        case 'o':
            __KWD("r", TOKEN_OR);
        case 'p':
            __KWD("rint", TOKEN_PRINT);
        case 'r':
            __KWD("eturn", TOKEN_RETURN);
        case 's':
            __KWD("uper", TOKEN_SUPER);
        case 't':
            switch(*p++){
                case 'h':
                    __KWD("is", TOKEN_THIS);
                case 'r':
                    __KWD("ue", TOKEN_TRUE);
            }
            break;
        case 'v':
            __KWD("ar", TOKEN_VAR);
        case 'w':
            __KWD("hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}


Token next_token() {
    scanner.start = scanner.cur;

    char c = *scanner.cur++;

    if (isdigit(c)) {
        bool saw_dot = false;
        while (isdigit(*scanner.cur) || (!saw_dot && *scanner.cur == '.')) {
            if (*scanner.cur == '.') saw_dot = true;
            scanner.cur++;
        }
        return make_token(TOKEN_NUMBER);
    }

    if (isalpha(c) || c == '_') {
        while (isalnum(*scanner.cur) || *scanner.cur == '_') {
            scanner.cur++;
        }

        return make_token(identifierType());
    }

    switch (c) {
        case '\0':
            scanner.cur--;
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
            switch (*scanner.cur++) {
                case '/':
                    while (*scanner.cur && *scanner.cur != '\n') {
                        scanner.cur++;
                    }
                    return next_token();
                case '*':
                    while (*scanner.cur) {
                        if (*scanner.cur == '*' && scanner.cur[1] == '/') {
                            scanner.cur += 2;
                            return next_token();
                        } else if (*scanner.cur == '\n') scanner.line++;
                        scanner.cur++;
                    }
                    return error_token("Unterminated block comment.");
                default:
                    scanner.cur--;
                    return make_token(TOKEN_SLASH);
            }
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