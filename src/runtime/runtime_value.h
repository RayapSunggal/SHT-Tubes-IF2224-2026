#ifndef RUNTIME_VALUE_H
#define RUNTIME_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    RUNTIME_VALUE_NONE,
    RUNTIME_VALUE_INTEGER,
    RUNTIME_VALUE_REAL,
    RUNTIME_VALUE_BOOLEAN,
    RUNTIME_VALUE_CHAR,
    RUNTIME_VALUE_STRING
} RuntimeValueType;

typedef struct {
    RuntimeValueType type;
    long long integerValue;
    double realValue;
    char charValue;
    char *stringValue;
} RuntimeValue;

RuntimeValue runtimeValueNone(void);
RuntimeValue runtimeValueInteger(long long value);
RuntimeValue runtimeValueReal(double value);
RuntimeValue runtimeValueBoolean(bool value);
RuntimeValue runtimeValueChar(char value);
RuntimeValue runtimeValueString(const char *value);

RuntimeValue runtimeValueCopy(RuntimeValue value);
void runtimeValueFree(RuntimeValue *value);
void runtimeValueToString(RuntimeValue value, char *buffer, size_t bufferSize);
void runtimeValuePrint(FILE *stream, RuntimeValue value);

bool runtimeValueIsZero(RuntimeValue value);
bool runtimeValueIsNumeric(RuntimeValue value);
double runtimeValueAsDouble(RuntimeValue value);
long long runtimeValueAsInteger(RuntimeValue value);

#endif
