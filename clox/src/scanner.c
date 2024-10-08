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

#define __KWD(kw, type)                                                        \
    return p + sizeof kw - 1 == scanner.cur && !strncmp(p, kw, sizeof kw - 1)  \
               ? type                                                          \
               : TOKEN_IDENTIFIER

TokenType identifierType() {
    char* p = scanner.start;
    switch (*p++) {
        case 'a':
            switch (*p++) {
                case 'n':
                    __KWD("d", TOKEN_AND);
                case 'r':
                    __KWD("ray", TOKEN_ARRAY);
            }
            break;
        case 'b':
            __KWD("reak", TOKEN_BREAK);
        case 'c':
            switch (*p++) {
                case 'a':
                    __KWD("se", TOKEN_CASE);
                case 'l':
                    __KWD("ass", TOKEN_CLASS);
                case 'o':
                    __KWD("ntinue", TOKEN_CONTINUE);
            }
            break;
        case 'd':
            switch (*p++) {
                case 'e':
                    __KWD("fault", TOKEN_DEFAULT);
                case 'o':
                    __KWD("", TOKEN_DO);
            }
            break;
        case 'e':
            __KWD("lse", TOKEN_ELSE);
        case 'f':
            switch (*p++) {
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
        case 'r':
            __KWD("eturn", TOKEN_RETURN);
        case 's':
            switch (*p++) {
                case 'u':
                    __KWD("per", TOKEN_SUPER);
                case 'w':
                    __KWD("itch", TOKEN_SWITCH);
            }
            break;
        case 't':
            switch (*p++) {
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
        case '[':
            return make_token(TOKEN_LEFT_SQUARE);
        case ']':
            return make_token(TOKEN_RIGHT_SQUARE);
        case '{':
            return make_token(TOKEN_LEFT_CURLY);
        case '}':
            return make_token(TOKEN_RIGHT_CURLY);
        case ';':
            return make_token(TOKEN_SEMICOLON);
        case ',':
            return make_token(TOKEN_COMMA);
        case '.':
            return make_token(TOKEN_DOT);
        case '%':
            return make_token(TOKEN_PERCENT);
        case '?':
            return make_token(TOKEN_QUESTION);
        case ':':
            return make_token(TOKEN_COLON);
        case '+':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_PLUS_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_PLUS);
        case '*':
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_STAR_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_STAR);
        case '-':
            if (*scanner.cur++ == '>') {
                return make_token(TOKEN_ARROW);
            }
            scanner.cur--;
            if (*scanner.cur++ == '=') {
                return make_token(TOKEN_MINUS_EQUAL);
            }
            scanner.cur--;
            return make_token(TOKEN_MINUS);
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
                case '=':
                    return make_token(TOKEN_SLASH_EQUAL);
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
        case '#':
            scanner.cur++;
            while (*scanner.cur && *scanner.cur != '\n') {
                scanner.cur++;
            }
            return next_token();
        case '"':
            while (*scanner.cur != '"') {
                if (*scanner.cur == '\n' || *scanner.cur == '\0')
                    return error_token("Unterminated string.");
                if (*scanner.cur == '\\') {
                    scanner.cur++;
                    if (*scanner.cur == '\0')
                        return error_token("Unterminated string.");
                }
                scanner.cur++;
            }
            scanner.cur++;
            return make_token(TOKEN_STRING);
        case '\'':
            if (*scanner.cur == '\\') scanner.cur++;
            if (*scanner.cur == '\0') return error_token("Unterminated char");
            scanner.cur++;
            if (*scanner.cur != '\'') return error_token("Unterminated char");
            scanner.cur++;
            return make_token(TOKEN_CHAR);
        default:
            return error_token("Lexer error.");
    }
}