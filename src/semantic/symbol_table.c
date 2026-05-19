#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TabEntry tab[MAX_TAB];
BtabEntry btab[MAX_BTAB];
AtabEntry atab[MAX_ATAB];
int display[MAX_DISPLAY];
int currentLevel = 0;

static int tabCount = 0;
static int btabCount = 0;
static int atabCount = 0;

static char *copyString(const char *src) {
    if (src == NULL) {
        return NULL;
    }

    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, src, len + 1);
    return dst;
}

static void initializeTabEntry(TabEntry *entry) {
    entry->identifier = NULL;
    entry->link = -1;
    entry->obj = OBJ_TYPE;
    entry->type = TYPE_NONE;
    entry->ref = -1;
    entry->nrm = 0;
    entry->lev = 0;
    entry->adr = 0;
}

static void initializeBtabEntry(BtabEntry *entry) {
    entry->last = -1;
    entry->lpar = -1;
    entry->psze = 0;
    entry->vsze = 0;
}

static void initializeAtabEntry(AtabEntry *entry) {
    entry->xtyp = TYPE_NONE;
    entry->etyp = TYPE_NONE;
    entry->eref = -1;
    entry->low = 0;
    entry->high = 0;
    entry->elsz = 0;
    entry->size = 0;
}

static int findSameNameInBlock(const char *name, int blockIndex) {
    if (name == NULL || blockIndex < 0 || blockIndex >= btabCount) {
        return -1;
    }

    int i = btab[blockIndex].last;
    while (i != -1) {
        if (tab[i].identifier != NULL && strcmp(tab[i].identifier, name) == 0) {
            return i;
        }
        i = tab[i].link;
    }

    return -1;
}

const char *objClassToString(ObjClass obj) {
    switch (obj) {
        case OBJ_CONSTANT: return "OBJ_CONSTANT";
        case OBJ_VARIABLE: return "OBJ_VARIABLE";
        case OBJ_TYPE: return "OBJ_TYPE";
        case OBJ_PROCEDURE: return "OBJ_PROCEDURE";
        case OBJ_FUNCTION: return "OBJ_FUNCTION";
        case OBJ_PROGRAM: return "OBJ_PROGRAM";
        default: return "OBJ_UNKNOWN";
    }
}

const char *baseTypeToString(BaseType type) {
    switch (type) {
        case TYPE_NONE: return "TYPE_NONE";
        case TYPE_INTEGER: return "TYPE_INTEGER";
        case TYPE_REAL: return "TYPE_REAL";
        case TYPE_BOOLEAN: return "TYPE_BOOLEAN";
        case TYPE_CHAR: return "TYPE_CHAR";
        case TYPE_STRING: return "TYPE_STRING";
        case TYPE_ARRAY: return "TYPE_ARRAY";
        case TYPE_RECORD: return "TYPE_RECORD";
        case TYPE_VOID: return "TYPE_VOID";
        default: return "TYPE_UNKNOWN";
    }
}

int sizeOfBaseType(BaseType type) {
    switch (type) {
        case TYPE_INTEGER: return 4;
        case TYPE_REAL: return 8;
        case TYPE_BOOLEAN: return 4;
        case TYPE_CHAR: return 1;
        case TYPE_STRING: return 8;
        case TYPE_VOID: return 0;
        case TYPE_ARRAY:
        case TYPE_RECORD:
        case TYPE_NONE:
        default:
            return 0;
    }
}

void symInit(void) {
    static const char *reservedWords[33] = {
        "and", "array", "begin", "case", "const", "div", "do", "downto",
        "else", "end", "file", "for", "function", "goto", "if", "in",
        "label", "mod", "nil", "not", "of", "or", "packed", "procedure",
        "program", "record", "repeat", "then", "to", "type", "until",
        "var", "while"
    };

    static const char *predefinedNames[6] = {
        "true", "false", "writeln", "readln", "write", "read"
    };

    currentLevel = 0;
    tabCount = 0;
    btabCount = 0;
    atabCount = 0;

    for (int i = 0; i < MAX_TAB; i++) {
        initializeTabEntry(&tab[i]);
    }

    for (int i = 0; i < MAX_BTAB; i++) {
        initializeBtabEntry(&btab[i]);
    }

    for (int i = 0; i < MAX_ATAB; i++) {
        initializeAtabEntry(&atab[i]);
    }

    for (int i = 0; i < MAX_DISPLAY; i++) {
        display[i] = -1;
    }

    for (int i = 0; i < 33; i++) {
        if (tabCount >= MAX_TAB) {
            break;
        }
        tab[tabCount].identifier = copyString(reservedWords[i]);
        tab[tabCount].link = -1;
        tab[tabCount].obj = OBJ_TYPE;
        tab[tabCount].type = TYPE_NONE;
        tab[tabCount].ref = -1;
        tab[tabCount].nrm = 0;
        tab[tabCount].lev = 0;
        tab[tabCount].adr = 0;
        tabCount++;
    }

    for (int i = 0; i < 6; i++) {
        if (tabCount >= MAX_TAB) {
            break;
        }

        tab[tabCount].identifier = copyString(predefinedNames[i]);
        tab[tabCount].link = -1;
        tab[tabCount].type = TYPE_VOID;
        tab[tabCount].ref = -1;
        tab[tabCount].nrm = 0;
        tab[tabCount].lev = 0;
        tab[tabCount].adr = 0;

        if (i == 0 || i == 1) {
            tab[tabCount].obj = OBJ_CONSTANT;
            tab[tabCount].type = TYPE_BOOLEAN;
        } else {
            tab[tabCount].obj = OBJ_PROCEDURE;
            tab[tabCount].type = TYPE_VOID;
        }

        tabCount++;
    }

    initializeBtabEntry(&btab[0]);
    btab[0].last = -1;
    btab[0].lpar = -1;
    btab[0].psze = 0;
    btab[0].vsze = 0;
    btabCount = 1;
    display[0] = 0;
}

void symEnterScope(void) {
    if (currentLevel + 1 >= MAX_DISPLAY || btabCount >= MAX_BTAB) {
        return;
    }

    int parentBlock = display[currentLevel];
    int newBlock = btabCount++;

    initializeBtabEntry(&btab[newBlock]);
    btab[newBlock].last = -1;
    btab[newBlock].lpar = parentBlock;
    btab[newBlock].psze = 0;
    btab[newBlock].vsze = 0;

    currentLevel++;
    display[currentLevel] = newBlock;
}

void symExitScope(void) {
    if (currentLevel > 0) {
        currentLevel--;
    }
}

int symEnter(const char *name, ObjClass obj, BaseType type, int ref, int nrm, int adr) {
    if (name == NULL || name[0] == '\0' || tabCount >= MAX_TAB) {
        return -1;
    }

    int blockIndex = display[currentLevel];
    if (blockIndex < 0 || blockIndex >= btabCount) {
        return -1;
    }

    if (findSameNameInBlock(name, blockIndex) != -1) {
        return -1;
    }

    int idx = tabCount++;
    tab[idx].identifier = copyString(name);
    tab[idx].link = btab[blockIndex].last;
    tab[idx].obj = obj;
    tab[idx].type = type;
    tab[idx].ref = ref;
    tab[idx].nrm = nrm;
    tab[idx].lev = currentLevel;
    tab[idx].adr = adr;

    btab[blockIndex].last = idx;

    if (obj == OBJ_VARIABLE) {
        int size = sizeOfBaseType(type);
        if (nrm == 1) {
            btab[blockIndex].psze += size;
        } else {
            btab[blockIndex].vsze += size;
        }
    }

    return idx;
}

int symLookup(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }

    for (int level = currentLevel; level >= 0; level--) {
        int blockIndex = display[level];
        if (blockIndex < 0 || blockIndex >= btabCount) {
            continue;
        }

        int idx = btab[blockIndex].last;
        while (idx != -1) {
            if (tab[idx].identifier != NULL && strcmp(tab[idx].identifier, name) == 0) {
                return idx;
            }
            idx = tab[idx].link;
        }
    }

    for (int i = 33; i < 39 && i < tabCount; i++) {
        if (tab[i].identifier != NULL && strcmp(tab[i].identifier, name) == 0) {
            return i;
        }
    }

    return -1;
}

int symEnterArray(BaseType xtyp, BaseType etyp, int eref, int low, int high, int elsz) {
    if (atabCount >= MAX_ATAB || low > high || elsz < 0) {
        return -1;
    }

    int idx = atabCount++;
    atab[idx].xtyp = xtyp;
    atab[idx].etyp = etyp;
    atab[idx].eref = eref;
    atab[idx].low = low;
    atab[idx].high = high;
    atab[idx].elsz = elsz;
    atab[idx].size = (high - low + 1) * elsz;
    return idx;
}

void symPrint(void) {
    printf("TAB:\n");
    printf(" idx | identifier | link | obj | type | ref | nrm | lev | adr\n");
    printf("-----+------------+------+------+------+-----+-----+-----+-----\n");

    for (int i = 0; i < tabCount; i++) {
        if (tab[i].identifier == NULL) {
            continue;
        }
        printf(" %3d | %10s | %4d | %s | %s | %3d | %3d | %3d | %3d\n",
               i,
               tab[i].identifier,
               tab[i].link,
               objClassToString(tab[i].obj),
               baseTypeToString(tab[i].type),
               tab[i].ref,
               tab[i].nrm,
               tab[i].lev,
               tab[i].adr);
    }

    printf("\nBTAB:\n");
    printf(" idx | last | lpar | psze | vsze\n");
    printf("-----+------+------+------+------\n");
    for (int i = 0; i < btabCount; i++) {
        printf(" %3d | %4d | %4d | %4d | %4d\n",
               i,
               btab[i].last,
               btab[i].lpar,
               btab[i].psze,
               btab[i].vsze);
    }

    printf("\nATAB:\n");
    printf(" idx | xtyp | etyp | eref | low | high | elsz | size\n");
    printf("-----+------+------ +------+-----+------+------+------\n");
    for (int i = 0; i < atabCount; i++) {
        printf(" %3d | %5s | %5s | %4d | %3d | %4d | %4d | %4d\n",
               i,
               baseTypeToString(atab[i].xtyp),
               baseTypeToString(atab[i].etyp),
               atab[i].eref,
               atab[i].low,
               atab[i].high,
               atab[i].elsz,
               atab[i].size);
    }

    printf("\nDISPLAY and LEVEL:\n");
    printf(" currentLevel = %d\n", currentLevel);
    for (int i = 0; i <= currentLevel; i++) {
        printf(" display[%d] = %d\n", i, display[i]);
    }
}
