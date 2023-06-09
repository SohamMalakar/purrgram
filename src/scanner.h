#ifndef clox_scanner_h
#define clox_scanner_h

typedef enum
{
    // Single-character tokens.
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    TOKEN_PERCENT,
    // One or two character tokens.
    TOKEN_SLASH_SLASH,
    TOKEN_STAR_STAR,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    // Keywords.
    TOKEN_AND,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_DEF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_IF,
    TOKEN_NONE,
    TOKEN_NOT,
    TOKEN_OR,
    TOKEN_RETURN,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct
{
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;

void initScanner(const char *source);
Token scanToken();

#endif
