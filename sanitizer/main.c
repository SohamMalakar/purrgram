#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/scanner.h"
#include "vector.h"

static char *readFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

DEFINE_VECTOR(int)

void addsemicolon(const char *source)
{
    vector_int pos;
    vector_init_int(&pos);

    initScanner(source);

    Token prevToken, curToken;

    curToken = scanToken();

    int semicolonCount = 0;

    while (curToken.type != TOKEN_EOF)
    {
        prevToken = curToken;
        const char *curPos = curToken.start + curToken.length;
        curToken = scanToken();

        if (prevToken.line != curToken.line)
        {
            switch (prevToken.type)
            {
            case TOKEN_RIGHT_PAREN:
            case TOKEN_RIGHT_BRACKET:
            case TOKEN_IDENTIFIER:
            case TOKEN_STRING:
            case TOKEN_NUMBER:
            case TOKEN_FALSE:
            case TOKEN_NONE:
            case TOKEN_TRUE: {
                switch (curToken.type)
                {
                case TOKEN_LEFT_PAREN:
                case TOKEN_LEFT_BRACKET:
                case TOKEN_IDENTIFIER:
                case TOKEN_STRING:
                case TOKEN_NUMBER:
                case TOKEN_BREAK:
                case TOKEN_CONTINUE:
                case TOKEN_DEF:
                case TOKEN_ELIF:
                case TOKEN_ELSE:
                case TOKEN_END:
                case TOKEN_FALSE:
                case TOKEN_FOR:
                case TOKEN_IF:
                case TOKEN_NONE:
                case TOKEN_RETURN:
                case TOKEN_TRUE:
                case TOKEN_VAR:
                case TOKEN_WHILE:
                    semicolonCount++;
                    vector_push_back_int(&pos, curPos - source);
                    break;
                }
                break;
            }
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
                semicolonCount++;
                vector_push_back_int(&pos, curPos - source);
                break;
            case TOKEN_RETURN: {
                switch (curToken.type)
                {
                case TOKEN_BREAK:
                case TOKEN_CONTINUE:
                case TOKEN_DEF:
                case TOKEN_ELIF:
                case TOKEN_ELSE:
                case TOKEN_END:
                case TOKEN_FOR:
                case TOKEN_IF:
                case TOKEN_RETURN:
                case TOKEN_VAR:
                case TOKEN_WHILE:
                    semicolonCount++;
                    vector_push_back_int(&pos, curPos - source);
                    break;
                }
                break;
            }
            }
        }
    }

    switch (prevToken.type)
    {
    case TOKEN_RIGHT_PAREN:
    case TOKEN_RIGHT_BRACKET:
    case TOKEN_IDENTIFIER:
    case TOKEN_STRING:
    case TOKEN_NUMBER:
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
    case TOKEN_FALSE:
    case TOKEN_NONE:
    case TOKEN_RETURN:
    case TOKEN_TRUE:
        semicolonCount++;
        vector_push_back_int(&pos, prevToken.start + prevToken.length - source);
        break;
    }

    int srcLen = strlen(source);
    int len = srcLen + semicolonCount;
    char *rectified = (char *)malloc(sizeof(char) * (len + 1));

    int j = 0;

    for (int i = 0, k = 0; i < srcLen;)
    {
        if (i == pos.elements[k])
        {
            rectified[j++] = ';';
            k++;
        }
        else
        {
            rectified[j++] = source[i++];
        }
    }

    rectified[j] = '\0';

    printf("%s", rectified);

    free(rectified);
    vector_free_int(&pos);
}

int main(int argc, const char *argv[])
{
    if (argc == 2)
    {
        char *source = readFile(argv[1]);
        addsemicolon(source);
        free(source);
    }
    else
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(1);
    }

    return 0;
}
