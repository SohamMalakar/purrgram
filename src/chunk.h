#ifndef purr_chunk_h
#define purr_chunk_h

#include "common.h"
#include "value.h"

typedef enum
{
    OP_CONSTANT,
    OP_NONE,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_BUILD_LIST,
    OP_INDEX_SUBSCR,
    OP_STORE_SUBSCR,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_INTDIV,
    OP_MOD,
    OP_POW,
    OP_NOT,
    OP_NEGATE,
    OP_BAND,
    OP_BOR,
    OP_XOR,
    OP_BNOT,
    OP_LSHIFT,
    OP_RSHIFT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,
} OpCode;

typedef struct
{
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);
int addConstant(Chunk *chunk, Value value);

#endif