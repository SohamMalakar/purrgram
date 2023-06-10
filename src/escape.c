#include "escape.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *convert_string(const char *raw)
{
    size_t raw_len = strlen(raw);
    size_t converted_len = 0;

    char *converted = malloc((raw_len + 1) * sizeof(char));
    if (converted == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    for (size_t i = 0; i < raw_len; i++)
    {
        if (raw[i] == '\\' && i < raw_len - 1)
        {
            switch (raw[i + 1])
            {
            case 'n':
                converted[converted_len++] = '\n';
                i++;
                break;
            case 'r':
                converted[converted_len++] = '\r';
                i++;
                break;
            case 't':
                converted[converted_len++] = '\t';
                i++;
                break;
            case 'b':
                converted[converted_len++] = '\b';
                i++;
                break;
            case 'f':
                converted[converted_len++] = '\f';
                i++;
                break;
            case 'v':
                converted[converted_len++] = '\v';
                i++;
                break;
            case '\\':
                converted[converted_len++] = '\\';
                i++;
                break;
            case '\'':
                converted[converted_len++] = '\'';
                i++;
                break;
            case '\"':
                converted[converted_len++] = '\"';
                i++;
                break;
            case '0':
                if (i < raw_len - 3 && raw[i + 2] >= '0' && raw[i + 2] <= '7' && raw[i + 3] >= '0' && raw[i + 3] <= '7')
                {
                    char octal[4] = {raw[i + 1], raw[i + 2], raw[i + 3], '\0'};
                    converted[converted_len++] = strtol(octal, NULL, 8);
                    i += 3;
                }
                else
                {
                    converted[converted_len++] = raw[i];
                }
                break;
            case 'x':
                if (i < raw_len - 3 && isxdigit(raw[i + 2]) && isxdigit(raw[i + 3]))
                {
                    char hex[3] = {raw[i + 2], raw[i + 3], '\0'};
                    converted[converted_len++] = strtol(hex, NULL, 16);
                    i += 3;
                }
                else
                {
                    converted[converted_len++] = raw[i];
                }
                break;
            default:
                converted[converted_len++] = raw[i];
                break;
            }
        }
        else
        {
            converted[converted_len++] = raw[i];
        }
    }

    converted[converted_len] = '\0';
    return converted;
}