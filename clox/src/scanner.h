#ifndef SCANNER_H
#define SCANNER_H

typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_SQUARE,
    TOKEN_RIGHT_SQUARE,
    TOKEN_LEFT_CURLY,
    TOKEN_RIGHT_CURLY,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    TOKEN_PERCENT,
    TOKEN_QUESTION,
    TOKEN_COLON,
    // One or two character tokens.
    TOKEN_NOT,
    TOKEN_NOT_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_ARROW,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_CHAR,
    // Keywords.
    TOKEN_AND,
    TOKEN_ARRAY,
    TOKEN_BREAK,
    TOKEN_CASE,
    TOKEN_CLASS,
    TOKEN_CONTINUE,
    TOKEN_DO,
    TOKEN_DEFAULT,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_SWITCH,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,

    TOKEN_ERROR,
    TOKEN_EOF,

    TOKEN_MAX
} TokenType;

typedef struct {
    TokenType type;
    char* start;
    int len;
    int line;
} Token;

void init_scanner(char* source);

Token next_token();

#endif