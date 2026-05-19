#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

#define MAX_TAB 512
#define MAX_BTAB 64
#define MAX_ATAB 128
#define MAX_DISPLAY 64

typedef enum {
    OBJ_CONSTANT,
    OBJ_VARIABLE,
    OBJ_TYPE,
    OBJ_PROCEDURE,
    OBJ_FUNCTION,
    OBJ_PROGRAM
} ObjClass;

typedef enum {
    TYPE_NONE,
    TYPE_INTEGER,
    TYPE_REAL,
    TYPE_BOOLEAN,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_RECORD,
    TYPE_VOID
} BaseType;

typedef struct {
    char *identifier;
    int link;
    ObjClass obj;
    BaseType type;
    int ref;
    int nrm;
    int lev;
    int adr;
} TabEntry;

typedef struct {
    int last;
    int lpar;
    int psze;
    int vsze;
} BtabEntry;

typedef struct {
    BaseType xtyp;
    BaseType etyp;
    int eref;
    int low;
    int high;
    int elsz;
    int size;
} AtabEntry;

extern TabEntry tab[MAX_TAB];
extern BtabEntry btab[MAX_BTAB];
extern AtabEntry atab[MAX_ATAB];
extern int display[MAX_DISPLAY];
extern int currentLevel;

void symInit(void);
void symEnterScope(void);
void symExitScope(void);
int symEnter(const char *name, ObjClass obj, BaseType type, int ref, int nrm, int adr);
int symLookup(const char *name);
int symEnterArray(BaseType xtyp, BaseType etyp, int eref, int low, int high, int elsz);
void symPrint(void);

const char *objClassToString(ObjClass obj);
const char *baseTypeToString(BaseType type);
int sizeOfBaseType(BaseType type);

#endif // SYMBOL_TABLE_H
