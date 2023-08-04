#ifndef purr_object_h
#define purr_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_LIST(value) isObjType(value, OBJ_LIST)

#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_LIST(value) ((ObjList *)AS_OBJ(value))

typedef enum
{
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_LIST,
} ObjType;

struct Obj
{
    ObjType type;
    struct Obj *next;
};

typedef struct
{
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString
{
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

typedef struct
{
    Obj obj;
    int count;
    int capacity;
    Value *items;
} ObjList;

ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);

ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);
Value indexFromString(ObjString *str, int index);
bool isValidStringIndex(ObjString *str, int index);

ObjList *newList();
void appendToList(ObjList *list, Value value);
void storeToList(ObjList *list, int index, Value value);
Value indexFromList(ObjList *list, int index);
void deleteFromList(ObjList *list, int index);
bool isValidListIndex(ObjList *list, int index);

bool isInt(double num);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif