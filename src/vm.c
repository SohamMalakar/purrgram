#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

VM vm;

static Value printNative(int argCount, Value *args)
{
    for (int i = 0; i < argCount; i++)
    {
        printValue(args[i]);
    }

    return NONE_VAL;
}

static Value clockNative(int argCount, Value *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value appendNative(int argCount, Value *args)
{
    // Append a value to the end of a list increasing the list's length by 1
    if (argCount != 2 || !IS_LIST(args[0]))
    {
        // Handle error
    }
    ObjList *list = AS_LIST(args[0]);
    Value item = args[1];
    appendToList(list, item);
    return NONE_VAL;
}

static Value deleteNative(int argCount, Value *args)
{
    // Delete an item from a list at the given index.
    if (argCount != 2 || !IS_LIST(args[0]) || !IS_NUMBER(args[1]))
    {
        // Handle error
    }

    ObjList *list = AS_LIST(args[0]);
    int index = AS_NUMBER(args[1]);

    if (!isValidListIndex(list, index))
    {
        // Handle error
    }

    deleteFromList(list, index);
    return NONE_VAL;
}

static void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--)
    {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char *name, NativeFn function)
{
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM()
{
    resetStack();
    vm.objects = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    defineNative("print", printNative);
    defineNative("clock", clockNative);
    defineNative("append", appendNative);
    defineNative("delete", deleteNative);
}

void freeVM()
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

static bool call(ObjFunction *function, int argCount)
{
    if (argCount != function->arity)
    {
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJ_FUNCTION:
            return call(AS_FUNCTION(callee), argCount);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }
        default:
            break; // Non-callable object type.
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

static bool isFalsey(Value value)
{
    return IS_NONE(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && !AS_NUMBER(value)) ||
           (IS_STRING(value) && !AS_STRING(value)->length) || (IS_LIST(value) && !AS_LIST(value)->count);
}

static void concatenate()
{
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = takeString(chars, length);
    push(OBJ_VAL(result));
}

inline static double mod(double x, double y)
{
    return x - y * floor(x / y);
}

static InterpretResult run()
{
    CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))                                                                \
        {                                                                                                              \
            runtimeError("Operands must be numbers.");                                                                 \
            return INTERPRET_RUNTIME_ERROR;                                                                            \
        }                                                                                                              \
        double b = AS_NUMBER(pop());                                                                                   \
        double a = AS_NUMBER(pop());                                                                                   \
        push(valueType(a op b));                                                                                       \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
        {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_NONE:
            push(NONE_VAL);
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_POP:
            pop();
            break;
        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value))
            {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            tableSet(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0)))
            {
                tableDelete(&vm.globals, name);
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_BUILD_LIST: {
            // Stack before: [item1, item2, ..., itemN] and after: [list]
            ObjList *list = newList();
            uint8_t itemCount = READ_BYTE();

            // Add items to list
            push(OBJ_VAL(list)); // So list isn't sweeped by GC in appendToList
            for (int i = itemCount; i > 0; i--)
            {
                appendToList(list, peek(i));
            }
            pop();

            // Pop items from stack
            while (itemCount-- > 0)
            {
                pop();
            }

            push(OBJ_VAL(list));
            break;
        }
        case OP_INDEX_SUBSCR: {
            // Stack before: [list, index] and after: [index(list, index)]
            Value v_index = pop();
            Value v_list = pop();
            Value result;

            if (!IS_LIST(v_list))
            {
                runtimeError("Invalid type to index into.");
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjList *list = AS_LIST(v_list);

            if (!IS_NUMBER(v_index))
            {
                runtimeError("List index is not a number.");
                return INTERPRET_RUNTIME_ERROR;
            }

            int index = AS_NUMBER(v_index);

            if (!isValidListIndex(list, index))
            {
                runtimeError("List index out of range.");
                return INTERPRET_RUNTIME_ERROR;
            }

            result = indexFromList(list, AS_NUMBER(v_index));
            push(result);
            break;
        }
        case OP_STORE_SUBSCR: {
            // Stack before: [list, index, item] and after: [item]
            Value item = pop();
            Value v_index = pop();
            Value v_list = pop();

            if (!IS_LIST(v_list))
            {
                runtimeError("Cannot store value in a non-list.");
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjList *list = AS_LIST(v_list);

            if (!IS_NUMBER(v_index))
            {
                runtimeError("List index is not a number.");
                return INTERPRET_RUNTIME_ERROR;
            }

            int index = AS_NUMBER(v_index);

            if (!isValidListIndex(list, index))
            {
                runtimeError("Invalid list index.");
                return INTERPRET_RUNTIME_ERROR;
            }

            storeToList(list, index, item);
            push(item);
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            }
            else
            {
                runtimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_INTDIV: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))
            {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(floor(a / b)));
            break;
        }
        case OP_MOD: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))
            {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(mod(a, b)));
            break;
        }
        case OP_POW: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))
            {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(pow(a, b)));
            break;
        }
        case OP_NOT:
            push(BOOL_VAL(isFalsey(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
                frame->ip += offset;
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_RETURN: {
            Value result = pop();
            vm.frameCount--;
            if (vm.frameCount == 0)
            {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots;
            push(result);
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source)
{
    ObjFunction *function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    call(function, 0);

    return run();
}
