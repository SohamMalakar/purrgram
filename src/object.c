#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "escape.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type)
{
    Obj *object = (Obj *)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

ObjFunction *newFunction()
{
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjNative *newNative(NativeFn function)
{
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NONE_VAL);
    return string;
}

static uint32_t hashString(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString *takeString(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction *function)
{
    if (function->name == NULL)
    {
        printf("<script>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_FUNCTION:
        printFunction(AS_FUNCTION(value));
        break;
    case OBJ_NATIVE:
        printf("<native fn>");
        break;
    case OBJ_STRING: {
        char *converted = convert_string(AS_CSTRING(value));

        if (converted != NULL)
        {
            printf("%s", converted);
            free(converted);
        }

        break;
    }
    case OBJ_LIST: {
        ObjList *list = AS_LIST(value);
        printf("[");
        for (int i = 0; i < list->count; i++)
        {
            printValue(list->items[i]);

            if (i != list->count - 1)
                printf(", ");
        }
        printf("]");
    }
    }
}

ObjList *newList()
{
    ObjList *list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void appendToList(ObjList *list, Value value)
{
    // Grow the array if necessary
    if (list->capacity < list->count + 1)
    {
        int oldCapacity = list->capacity;
        list->capacity = GROW_CAPACITY(oldCapacity);
        list->items = GROW_ARRAY(Value, list->items, oldCapacity, list->capacity);
    }
    list->items[list->count] = value;
    list->count++;
    return;
}

void storeToList(ObjList *list, int index, Value value)
{
    list->items[index] = value;
}

Value indexFromList(ObjList *list, int index)
{
    return list->items[index];
}

void deleteFromList(ObjList *list, int index)
{
    for (int i = index; i < list->count - 1; i++)
    {
        list->items[i] = list->items[i + 1];
    }
    list->items[list->count - 1] = NONE_VAL;
    list->count--;
}

bool isValidListIndex(ObjList *list, int index)
{
    if (index < 0 || index > list->count - 1)
    {
        return false;
    }
    return true;
}