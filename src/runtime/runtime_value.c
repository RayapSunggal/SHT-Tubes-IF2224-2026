#include "runtime_value.h"

#include <stdlib.h>
#include <string.h>

static char *copyString(const char *src) {
    char *dst;
    size_t len;

    if (src == NULL) {
        src = "";
    }

    len = strlen(src);
    dst = (char *)malloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, src, len + 1);
    return dst;
}

RuntimeValue runtimeValueNone(void) {
    RuntimeValue value;

    value.type = RUNTIME_VALUE_NONE;
    value.integerValue = 0;
    value.realValue = 0.0;
    value.charValue = '\0';
    value.stringValue = NULL;
    return value;
}

RuntimeValue runtimeValueInteger(long long value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    runtimeValue.type = RUNTIME_VALUE_INTEGER;
    runtimeValue.integerValue = value;
    runtimeValue.realValue = (double)value;
    return runtimeValue;
}

RuntimeValue runtimeValueReal(double value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    runtimeValue.type = RUNTIME_VALUE_REAL;
    runtimeValue.realValue = value;
    runtimeValue.integerValue = (long long)value;
    return runtimeValue;
}

RuntimeValue runtimeValueBoolean(bool value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    runtimeValue.type = RUNTIME_VALUE_BOOLEAN;
    runtimeValue.integerValue = value ? 1 : 0;
    runtimeValue.realValue = value ? 1.0 : 0.0;
    return runtimeValue;
}

RuntimeValue runtimeValueChar(char value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    runtimeValue.type = RUNTIME_VALUE_CHAR;
    runtimeValue.charValue = value;
    runtimeValue.integerValue = (unsigned char)value;
    runtimeValue.realValue = (double)(unsigned char)value;
    return runtimeValue;
}

RuntimeValue runtimeValueString(const char *value) {
    RuntimeValue runtimeValue = runtimeValueNone();

    runtimeValue.type = RUNTIME_VALUE_STRING;
    runtimeValue.stringValue = copyString(value);
    return runtimeValue;
}

RuntimeValue runtimeValueCopy(RuntimeValue value) {
    RuntimeValue copy = value;

    if (value.type == RUNTIME_VALUE_STRING) {
        copy.stringValue = copyString(value.stringValue);
    }

    return copy;
}

void runtimeValueFree(RuntimeValue *value) {
    if (value == NULL) {
        return;
    }

    if (value->type == RUNTIME_VALUE_STRING) {
        free(value->stringValue);
    }

    *value = runtimeValueNone();
}

void runtimeValueToString(RuntimeValue value, char *buffer, size_t bufferSize) {
    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    switch (value.type) {
        case RUNTIME_VALUE_INTEGER:
            (void)snprintf(buffer, bufferSize, "%lld", value.integerValue);
            break;
        case RUNTIME_VALUE_REAL:
            (void)snprintf(buffer, bufferSize, "%g", value.realValue);
            break;
        case RUNTIME_VALUE_BOOLEAN:
            (void)snprintf(buffer, bufferSize, "%s", value.integerValue != 0 ? "true" : "false");
            break;
        case RUNTIME_VALUE_CHAR:
            (void)snprintf(buffer, bufferSize, "%c", value.charValue);
            break;
        case RUNTIME_VALUE_STRING:
            (void)snprintf(buffer, bufferSize, "%s", value.stringValue != NULL ? value.stringValue : "");
            break;
        case RUNTIME_VALUE_NONE:
        default:
            (void)snprintf(buffer, bufferSize, "<none>");
            break;
    }
}

void runtimeValuePrint(FILE *stream, RuntimeValue value) {
    char buffer[256];

    if (stream == NULL) {
        return;
    }

    runtimeValueToString(value, buffer, sizeof(buffer));
    fputs(buffer, stream);
}

bool runtimeValueIsZero(RuntimeValue value) {
    switch (value.type) {
        case RUNTIME_VALUE_INTEGER:
        case RUNTIME_VALUE_BOOLEAN:
            return value.integerValue == 0;
        case RUNTIME_VALUE_REAL:
            return value.realValue == 0.0;
        case RUNTIME_VALUE_CHAR:
            return value.charValue == '\0';
        case RUNTIME_VALUE_STRING:
            return value.stringValue == NULL || value.stringValue[0] == '\0';
        case RUNTIME_VALUE_NONE:
        default:
            return true;
    }
}

bool runtimeValueIsNumeric(RuntimeValue value) {
    return value.type == RUNTIME_VALUE_INTEGER ||
           value.type == RUNTIME_VALUE_REAL ||
           value.type == RUNTIME_VALUE_BOOLEAN ||
           value.type == RUNTIME_VALUE_CHAR;
}

double runtimeValueAsDouble(RuntimeValue value) {
    if (value.type == RUNTIME_VALUE_REAL) {
        return value.realValue;
    }
    if (value.type == RUNTIME_VALUE_CHAR) {
        return (double)(unsigned char)value.charValue;
    }
    return (double)value.integerValue;
}

long long runtimeValueAsInteger(RuntimeValue value) {
    if (value.type == RUNTIME_VALUE_CHAR) {
        return (unsigned char)value.charValue;
    }
    if (value.type == RUNTIME_VALUE_REAL) {
        return (long long)value.realValue;
    }
    return value.integerValue;
}
