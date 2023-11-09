#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_VECTOR_SIZE 8
#define CAPACITY_THRESHOLD 1024

#define DEFINE_VECTOR(type)                                                                                            \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        type *elements;                                                                                                \
        uint64_t len;                                                                                                  \
        uint64_t capacity;                                                                                             \
                                                                                                                       \
    } vector_##type;                                                                                                   \
                                                                                                                       \
    bool vector_init_##type(vector_##type *vec)                                                                        \
    {                                                                                                                  \
        type *elements = (type *)malloc(DEFAULT_VECTOR_SIZE * sizeof(type));                                           \
                                                                                                                       \
        if (elements == NULL)                                                                                          \
            return false;                                                                                              \
                                                                                                                       \
        vec->elements = elements;                                                                                      \
        vec->len = 0;                                                                                                  \
        vec->capacity = DEFAULT_VECTOR_SIZE;                                                                           \
                                                                                                                       \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    bool vector_modify_capacity_##type(vector_##type *vec, uint64_t capacity)                                          \
    {                                                                                                                  \
        if (vec->capacity == capacity)                                                                                 \
            return true;                                                                                               \
                                                                                                                       \
        vec->elements = (type *)realloc(vec->elements, capacity * sizeof(type));                                       \
                                                                                                                       \
        if (vec->elements == NULL && capacity != 0)                                                                    \
        {                                                                                                              \
            fprintf(stderr, "Out of Memory!\n");                                                                       \
            vec->capacity = vec->len = 0;                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        vec->capacity = capacity;                                                                                      \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    bool vector_resize_##type(vector_##type *vec, uint64_t len, type *default_value)                                   \
    {                                                                                                                  \
        if (len == vec->len)                                                                                           \
            return true;                                                                                               \
                                                                                                                       \
        if (len < vec->len)                                                                                            \
        {                                                                                                              \
            vec->len = len;                                                                                            \
                                                                                                                       \
            if ((vec->capacity - len) > CAPACITY_THRESHOLD)                                                            \
                vector_modify_capacity_##type(vec, len);                                                               \
                                                                                                                       \
            return true;                                                                                               \
        }                                                                                                              \
                                                                                                                       \
        if (vec->capacity < len && !vector_modify_capacity_##type(vec, len))                                           \
            return false;                                                                                              \
                                                                                                                       \
        if (default_value != NULL)                                                                                     \
            for (uint64_t i = vec->len; i < len; i++)                                                                  \
                vec->elements[i] = *default_value;                                                                     \
                                                                                                                       \
        vec->len = len;                                                                                                \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    type *vector_at_##type(vector_##type *vec, uint64_t index)                                                         \
    {                                                                                                                  \
        if (vec->len <= index)                                                                                         \
        {                                                                                                              \
            fprintf(stderr, "Memory Out of Bounds!\n");                                                                \
            abort();                                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        return &vec->elements[index];                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    bool vector_push_back_##type(vector_##type *vec, type element)                                                     \
    {                                                                                                                  \
        if (vec->len == vec->capacity &&                                                                               \
            !vector_modify_capacity_##type(vec, vec->capacity < 8 ? 8 : vec->capacity << 1))                           \
            return false;                                                                                              \
                                                                                                                       \
        vec->elements[vec->len++] = element;                                                                           \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    type vector_pop_back_##type(vector_##type *vec)                                                                    \
    {                                                                                                                  \
        if (vec->len == 0)                                                                                             \
        {                                                                                                              \
            fprintf(stderr, "Vector Underflow!\n");                                                                    \
            abort();                                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        return vec->elements[--vec->len];                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    bool vector_free_##type(vector_##type *vec)                                                                        \
    {                                                                                                                  \
        if (vec->elements == NULL)                                                                                     \
            return false;                                                                                              \
                                                                                                                       \
        free(vec->elements);                                                                                           \
                                                                                                                       \
        vec->elements = NULL;                                                                                          \
        vec->len = vec->capacity = 0;                                                                                  \
        return true;                                                                                                   \
    }

#endif /* _VECTOR_H_ */
