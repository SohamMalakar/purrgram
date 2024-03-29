#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_SHIFT,      // << >>
    PREC_BAND,       // &
    PREC_XOR,        // ^
    PREC_BOR,        // |
    PREC_TERM,       // + - ~
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_EXPONENT,   // **
    PREC_CALL,       // . ()
    PREC_SUBSCRIPT,
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
} Local;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler
{
    struct Compiler *enclosing;
    ObjFunction *function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler *current = NULL;

static int *currentBreakJumps = NULL;
static int *currentBreakJumpIndex = NULL;
static int *currentLoopStart = NULL;

static int loopDepth = 0;

static Chunk *currentChunk()
{
    return &current->function->chunk;
}

static void errorAt(Token *token, const char *message)
{
    if (parser.panicMode)
        return;

    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message)
{
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message)
{
    errorAt(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type)
{
    return parser.current.type == type;
}

static bool match(TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn()
{
    emitByte(OP_NONE);
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction *endCompiler()
{
    emitReturn();
    ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope()
{
    current->scopeDepth++;
}

static void endScope()
{
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        emitByte(OP_POP);
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name)
{
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];

        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }

            return i;
        }
    }

    return -1;
}

static void addLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static void declareVariable()
{
    if (current->scopeDepth == 0)
        return;

    Token *name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];

        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

static uint8_t parseVariable(const char *errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized()
{
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0)
    {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();
            if (argCount == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void binary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType)
    {
    case TOKEN_BANG_EQUAL:
        emitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        emitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        emitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        emitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        emitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        emitBytes(OP_GREATER, OP_NOT);
        break;
    case TOKEN_PLUS:
        emitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        emitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        emitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        emitByte(OP_DIVIDE);
        break;
    case TOKEN_SLASH_SLASH:
        emitByte(OP_INTDIV);
        break;
    case TOKEN_PERCENT:
        emitByte(OP_MOD);
        break;
    case TOKEN_STAR_STAR:
        emitByte(OP_POW);
        break;
    case TOKEN_AMPERSAND:
        emitByte(OP_BAND);
        break;
    case TOKEN_PIPE:
        emitByte(OP_BOR);
        break;
    case TOKEN_CARET:
        emitByte(OP_XOR);
        break;
    case TOKEN_GREATER_GREATER:
        emitByte(OP_LSHIFT);
        break;
    case TOKEN_LESS_LESS:
        emitByte(OP_RSHIFT);
        break;
    default:
        return; // Unreachable.
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign)
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        emitByte(OP_FALSE);
        break;
    case TOKEN_NONE:
        emitByte(OP_NONE);
        break;
    case TOKEN_TRUE:
        emitByte(OP_TRUE);
        break;
    default:
        return; // Unreachable.
    }
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);

    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType)
    {
    case TOKEN_NOT:
        emitByte(OP_NOT);
        break;
    case TOKEN_MINUS:
        emitByte(OP_NEGATE);
        break;
    case TOKEN_TILDE:
        emitByte(OP_BNOT);
        break;
    default:
        return; // Unreachable.
    }
}

static void list(bool canAssign)
{
    int itemCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET))
    {
        do
        {
            if (check(TOKEN_RIGHT_BRACKET))
            {
                // Trailing comma case
                break;
            }

            parsePrecedence(PREC_OR);

            if (itemCount == UINT8_COUNT)
            {
                error("Cannot have more than 256 items in a list literal.");
            }
            itemCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list literal.");

    emitByte(OP_BUILD_LIST);
    emitByte(itemCount);
    return;
}

static void subscript(bool canAssign)
{
    parsePrecedence(PREC_OR);
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitByte(OP_STORE_SUBSCR);
    }
    else
    {
        emitByte(OP_INDEX_SUBSCR);
    }
    return;
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACKET] = {list, subscript, PREC_SUBSCRIPT},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_PERCENT] = {NULL, binary, PREC_FACTOR},
    [TOKEN_SLASH_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR_STAR] = {NULL, binary, PREC_EXPONENT},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_AMPERSAND] = {NULL, binary, PREC_BAND},
    [TOKEN_PIPE] = {NULL, binary, PREC_BOR},
    [TOKEN_CARET] = {NULL, binary, PREC_XOR},
    [TOKEN_TILDE] = {unary, NULL, PREC_TERM},
    [TOKEN_GREATER_GREATER] = {NULL, binary, PREC_SHIFT},
    [TOKEN_LESS_LESS] = {NULL, binary, PREC_SHIFT},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
    [TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
    [TOKEN_DEF] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELIF] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_END] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NONE] = {literal, NULL, PREC_NONE},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence)
{
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL)
    {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

static ParseRule *getRule(TokenType type)
{
    return &rules[type];
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_COLON, "Expect ':' before function body.");

    while (!check(TOKEN_EOF) && !check(TOKEN_END))
    {
        declaration();
    }

    ObjFunction *function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));

    consume(TOKEN_END, "Expect 'end' keyword after def block.");
}

static void funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration()
{
    do
    {
        uint8_t global = parseVariable("Expect variable name.");

        if (match(TOKEN_EQUAL))
        {
            expression();
        }
        else
        {
            emitByte(OP_NONE);
        }

        defineVariable(global);
    } while (match(TOKEN_COMMA));

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    // defineVariable(global);
}

static void synchronize()
{
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type)
        {
        case TOKEN_DEF:
        case TOKEN_VAR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
        case TOKEN_RETURN:
            return;

        default:; // Do nothing.
        }

        advance();
    }
}

static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void ifStatement()
{
    int exitJumps[UINT8_COUNT];
    int exitJumpIndex = 0;

    expression();
    consume(TOKEN_COLON, "Expect ':' after if condition.");

    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    while (!check(TOKEN_EOF) && !check(TOKEN_ELIF) && !check(TOKEN_ELSE) && !check(TOKEN_END))
    {
        statement();
    }

    int exitJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);

    while (match(TOKEN_ELIF))
    {
        expression();
        consume(TOKEN_COLON, "Expect ':' after elif condition.");

        int elifJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);

        while (!check(TOKEN_EOF) && !check(TOKEN_ELIF) && !check(TOKEN_ELSE) && !check(TOKEN_END))
        {
            statement();
        }

        int nextExitJump = emitJump(OP_JUMP);
        patchJump(elifJump);
        emitByte(OP_POP);

        exitJumps[exitJumpIndex++] = exitJump;
        exitJump = nextExitJump;

        if (exitJumpIndex >= UINT8_COUNT)
        {
            error("Too many elif statements inside a if-elif-else block.");
            return;
        }
    }

    if (match(TOKEN_ELSE))
    {
        consume(TOKEN_COLON, "Expect ':' after else keyword.");

        while (!check(TOKEN_EOF) && !check(TOKEN_END))
        {
            statement();
        }
    }

    exitJumps[exitJumpIndex++] = exitJump;

    while (exitJumpIndex > 0)
    {
        int prevExitJump = exitJumps[--exitJumpIndex];
        patchJump(prevExitJump);
    }

    consume(TOKEN_END, "Expect 'end' keyword after if-elif-else block.");
}

static void returnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement()
{
    loopDepth++;
    int breakJumps[UINT8_COUNT];
    int breakJumpIndex = 0;

    int loopStart = currentChunk()->count;
    expression();
    consume(TOKEN_COLON, "Expect ':' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    currentBreakJumps = breakJumps;
    currentBreakJumpIndex = &breakJumpIndex;
    currentLoopStart = &loopStart;

    while (!check(TOKEN_EOF) && !check(TOKEN_END))
    {
        statement();
    }

    emitLoop(loopStart);

    patchJump(exitJump);

    while (breakJumpIndex--)
    {
        patchJump(breakJumps[breakJumpIndex]);
    }

    emitByte(OP_POP);

    consume(TOKEN_END, "Expect 'end' keyword after while block.");
    loopDepth--;
}

static void breakStatement()
{
    if (loopDepth == 0)
    {
        error("Can't break from outside of a loop.");
        return;
    }

    if (*currentBreakJumpIndex >= UINT8_COUNT)
    {
        error("Too many break statements inside a loop.");
        return;
    }

    currentBreakJumps[(*currentBreakJumpIndex)++] = emitJump(OP_JUMP);
    consume(TOKEN_SEMICOLON, "Expect ';' after break keyword.");
}

static void continueStatement()
{
    if (loopDepth == 0)
    {
        error("Can't continue from outside of a loop.");
        return;
    }

    if (*currentLoopStart >= UINT8_COUNT)
    {
        error("Too many continue statements inside a loop.");
        return;
    }

    emitLoop(*currentLoopStart);
    consume(TOKEN_SEMICOLON, "Expect ';' after continue keyword.");
}

static void declaration()
{
    if (match(TOKEN_DEF))
    {
        funDeclaration();
    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

static void statement()
{
    if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_BREAK))
    {
        breakStatement();
    }
    else if (match(TOKEN_CONTINUE))
    {
        continueStatement();
    }
    else
    {
        expressionStatement();
    }
}

ObjFunction *compile(const char *source)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF))
    {
        declaration();
    }

    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function;
}
