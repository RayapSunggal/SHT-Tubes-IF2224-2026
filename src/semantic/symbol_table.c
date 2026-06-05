#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

TabEntry tab[MAX_TAB];
BtabEntry btab[MAX_BTAB];
AtabEntry atab[MAX_ATAB];
ETabEntry etab[MAX_ETAB];
int display[MAX_DISPLAY];
int currentLevel = 0;

static int tabCount = 0;
static int btabCount = 0;
static int atabCount = 0;
static int etabCount = 0;
static int predefinedCount = 0;
static int recordBlockStack[MAX_BTAB];
static int recordBlockDepth = 0;

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
    entry->hasRange = false;
    entry->rangeBase = TYPE_NONE;
    entry->rangeLow = 0;
    entry->rangeHigh = 0;
    entry->initialized = false;
}

static void initializeBtabEntry(BtabEntry *entry) {
    entry->last = -1;
    entry->lpar = -1;
    entry->psze = 0;
    entry->vsze = 0;
}

static void initializeAtabEntry(AtabEntry *entry) {
    entry->xtyp = TYPE_NONE;
    entry->xref = -1;
    entry->etyp = TYPE_NONE;
    entry->eref = -1;
    entry->low = 0;
    entry->high = 0;
    entry->elsz = 0;
    entry->size = 0;
    entry->elemHasRange = false;
    entry->elemRangeBase = TYPE_NONE;
    entry->elemRangeLow = 0;
    entry->elemRangeHigh = 0;
}

static void initializeEtabEntry(ETabEntry *entry) {
    entry->count = 0;
}

static int findSameNameInBlock(const char *name, int blockIndex) {
    if (name == NULL || blockIndex < 0 || blockIndex >= btabCount) {
        return -1;
    }

    int i = btab[blockIndex].last;
    while (i != -1) {
        if (tab[i].identifier != NULL && strcasecmp(tab[i].identifier, name) == 0) {
            return i;
        }
        i = tab[i].link;
    }

    return -1;
}

static int findPermanentName(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }

    for (int i = 0; i < predefinedCount && i < tabCount; i++) {
        if (tab[i].identifier != NULL && strcasecmp(tab[i].identifier, name) == 0) {
            return i;
        }
    }

    return -1;
}

static BaseType predefinedTypeName(const char *name) {
    if (strcasecmp(name, "integer") == 0) return TYPE_INTEGER;
    if (strcasecmp(name, "real") == 0) return TYPE_REAL;
    if (strcasecmp(name, "boolean") == 0) return TYPE_BOOLEAN;
    if (strcasecmp(name, "char") == 0) return TYPE_CHAR;
    if (strcasecmp(name, "string") == 0) return TYPE_STRING;
    return TYPE_NONE;
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
        case TYPE_INTEGER: return 1;
        case TYPE_REAL: return 1;
        case TYPE_BOOLEAN: return 1;
        case TYPE_CHAR: return 1;
        case TYPE_STRING: return 1;
        case TYPE_VOID: return 0;
        case TYPE_ARRAY:
        case TYPE_RECORD:
        case TYPE_NONE:
        default:
            return 0;
    }
}

void symInit(void) {
    static const char *reservedWords[32] = {
        "and", "array", "begin", "case", "const", "div", "downto", "do",
        "else", "end", "for", "function", "if", "mod", "not", "of",
        "or", "procedure", "program", "record", "repeat", "integer",
        "real", "boolean", "char", "string", "then", "to", "type", "until",
        "var", "while"
    };

    static const struct {
        const char *name;
        ObjClass obj;
        BaseType type;
        int nrm;
        int adr;
    } predefinedNames[] = {
        { "true", OBJ_CONSTANT, TYPE_BOOLEAN, 1, 1 },
        { "false", OBJ_CONSTANT, TYPE_BOOLEAN, 1, 0 },
        { "writeln", OBJ_PROCEDURE, TYPE_VOID, -1, 0 },
        { "readln", OBJ_PROCEDURE, TYPE_VOID, -1, 0 },
        { "write", OBJ_PROCEDURE, TYPE_VOID, -1, 0 },
        { "read", OBJ_PROCEDURE, TYPE_VOID, -1, 0 }
    };

    currentLevel = 0;
    tabCount = 0;
    btabCount = 0;
    atabCount = 0;
    etabCount = 0;
    predefinedCount = 0;
    recordBlockDepth = 0;

    for (int i = 0; i < MAX_TAB; i++) {
        initializeTabEntry(&tab[i]);
    }

    for (int i = 0; i < MAX_BTAB; i++) {
        initializeBtabEntry(&btab[i]);
    }

    for (int i = 0; i < MAX_ATAB; i++) {
        initializeAtabEntry(&atab[i]);
    }

    for (int i = 0; i < MAX_ETAB; i++) {
        initializeEtabEntry(&etab[i]);
    }

    for (int i = 0; i < MAX_DISPLAY; i++) {
        display[i] = -1;
    }

    for (int i = 0; i < 32; i++) {
        if (tabCount >= MAX_TAB) {
            break;
        }
        tab[tabCount].identifier = copyString(reservedWords[i]);
        tab[tabCount].link = -1;
        tab[tabCount].obj = OBJ_TYPE;
        tab[tabCount].type = predefinedTypeName(reservedWords[i]);
        tab[tabCount].ref = -1;
        tab[tabCount].nrm = 0;
        tab[tabCount].lev = 0;
        tab[tabCount].adr = 0;
        tab[tabCount].initialized = true;
        tabCount++;
    }

    for (int i = 0; i < (int)(sizeof(predefinedNames) / sizeof(predefinedNames[0])); i++) {
        if (tabCount >= MAX_TAB) {
            break;
        }

        tab[tabCount].identifier = copyString(predefinedNames[i].name);
        tab[tabCount].link = -1;
        tab[tabCount].obj = predefinedNames[i].obj;
        tab[tabCount].type = predefinedNames[i].type;
        tab[tabCount].ref = -1;
        tab[tabCount].nrm = predefinedNames[i].nrm;
        tab[tabCount].lev = 0;
        tab[tabCount].adr = predefinedNames[i].adr;
        tab[tabCount].initialized = true;

        tabCount++;
    }
    predefinedCount = tabCount;

    initializeBtabEntry(&btab[0]);
    btab[0].last = -1;
    btab[0].lpar = 0;
    btab[0].psze = 0;
    btab[0].vsze = 0;
    btabCount = 1;
    display[0] = 0;
}

void symEnterScope(void) {
    if (currentLevel + 1 >= MAX_DISPLAY || btabCount >= MAX_BTAB) {
        return;
    }

    int newBlock = btabCount++;

    initializeBtabEntry(&btab[newBlock]);
    btab[newBlock].last = -1;
    btab[newBlock].lpar = 0;
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

int symEnterRecordBlock(void) {
    if (btabCount >= MAX_BTAB) return -1;
    int idx = btabCount++;
    initializeBtabEntry(&btab[idx]);
    btab[idx].last = -1;
    btab[idx].lpar = 0;
    btab[idx].psze = 0;
    btab[idx].vsze = 0;
    if (recordBlockDepth < MAX_BTAB) {
        recordBlockStack[recordBlockDepth++] = idx;
    }
    return idx;
}

void symExitRecordBlock(void) {
    if (recordBlockDepth > 0) {
        recordBlockDepth--;
    }
}

int sizeOfType(BaseType type, int ref) {
    if (type == TYPE_ARRAY && ref >= 0 && ref < atabCount) {
        return atab[ref].size;
    }
    if (type == TYPE_RECORD && ref >= 0 && ref < btabCount) {
        return btab[ref].vsze;
    }
    return sizeOfBaseType(type);
}

int symEnterField(const char *name, BaseType type, int ref, int adr) {
    if (name == NULL || name[0] == '\0' || tabCount >= MAX_TAB || recordBlockDepth == 0) return -1;
    int blockIndex = recordBlockStack[recordBlockDepth - 1];
    if (findSameNameInBlock(name, blockIndex) != -1) return -1;

    int idx = tabCount++;
    tab[idx].identifier = copyString(name);
    tab[idx].link = btab[blockIndex].last;
    tab[idx].obj = OBJ_VARIABLE;
    tab[idx].type = type;
    tab[idx].ref = ref;
    tab[idx].nrm = 0;
    tab[idx].lev = currentLevel;
    tab[idx].adr = adr;
    tab[idx].initialized = true;

    btab[blockIndex].last = idx;
    btab[blockIndex].vsze += sizeOfType(type, ref);
    return idx;
}

int symEnter(const char *name, ObjClass obj, BaseType type, int ref, int nrm, int adr) {
    if (name == NULL || name[0] == '\0' || tabCount >= MAX_TAB) {
        return -1;
    }

    int blockIndex = display[currentLevel];
    if (blockIndex < 0 || blockIndex >= btabCount) {
        return -1;
    }

    if (findSameNameInBlock(name, blockIndex) != -1 || findPermanentName(name) != -1) {
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
    tab[idx].initialized = obj != OBJ_VARIABLE || nrm == 0;

    btab[blockIndex].last = idx;

    if (obj == OBJ_VARIABLE) {
        int size = sizeOfType(type, ref);
        if (nrm == 0) {
            btab[blockIndex].psze += size;
            btab[blockIndex].lpar = idx;
        } else {
            btab[blockIndex].vsze += size;
        }
    }

    return idx;
}

void symSetRange(int idx, BaseType rangeBase, int low, int high) {
    if (idx < 0 || idx >= tabCount) {
        return;
    }
    tab[idx].hasRange = true;
    tab[idx].rangeBase = rangeBase;
    tab[idx].rangeLow = low;
    tab[idx].rangeHigh = high;
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
            if (tab[idx].identifier != NULL && strcasecmp(tab[idx].identifier, name) == 0) {
                return idx;
            }
            idx = tab[idx].link;
        }
    }

    for (int i = 0; i < predefinedCount && i < tabCount; i++) {
        if (tab[i].identifier != NULL && strcasecmp(tab[i].identifier, name) == 0) {
            return i;
        }
    }

    return -1;
}

int symEnterEnum(int count) {
    if (etabCount >= MAX_ETAB || count <= 0) {
        return -1;
    }

    int idx = etabCount++;
    etab[idx].count = count;
    return idx;
}

int symEnumCount(int enumRef) {
    if (enumRef < 0 || enumRef >= etabCount) {
        return 0;
    }
    return etab[enumRef].count;
}

int symEnterArray(BaseType xtyp, int xref, BaseType etyp, int eref, int low, int high, int elsz,
                  bool elemHasRange, BaseType elemRangeBase, int elemRangeLow, int elemRangeHigh) {
    if (atabCount >= MAX_ATAB || low > high || elsz < 0) {
        return -1;
    }

    int idx = atabCount++;
    atab[idx].xtyp = xtyp;
    atab[idx].xref = xref;
    atab[idx].etyp = etyp;
    atab[idx].eref = eref;
    atab[idx].low = low;
    atab[idx].high = high;
    atab[idx].elsz = elsz;
    atab[idx].size = (high - low + 1) * elsz;
    atab[idx].elemHasRange = elemHasRange;
    atab[idx].elemRangeBase = elemRangeBase;
    atab[idx].elemRangeLow = elemRangeLow;
    atab[idx].elemRangeHigh = elemRangeHigh;
    return idx;
}

bool symLoadTabEntry(int idx, const char *name, ObjClass obj, BaseType type, int link, int ref, int nrm, int lev, int adr) {
    if (idx < 0 || idx >= MAX_TAB || name == NULL) {
        return false;
    }

    free(tab[idx].identifier);
    initializeTabEntry(&tab[idx]);
    tab[idx].identifier = copyString(name);
    if (tab[idx].identifier == NULL) {
        return false;
    }
    tab[idx].link = link;
    tab[idx].obj = obj;
    tab[idx].type = type;
    tab[idx].ref = ref;
    tab[idx].nrm = nrm;
    tab[idx].lev = lev;
    tab[idx].adr = adr;
    tab[idx].initialized = true;

    if (idx + 1 > tabCount) {
        tabCount = idx + 1;
    }

    return true;
}

bool symLoadBtabEntry(int idx, int last, int lpar, int psze, int vsze) {
    if (idx < 0 || idx >= MAX_BTAB) {
        return false;
    }

    initializeBtabEntry(&btab[idx]);
    btab[idx].last = last;
    btab[idx].lpar = lpar;
    btab[idx].psze = psze;
    btab[idx].vsze = vsze;

    if (idx + 1 > btabCount) {
        btabCount = idx + 1;
    }

    return true;
}

bool symLoadAtabEntry(int idx, BaseType xtyp, BaseType etyp, int eref, int low, int high, int elsz, int size) {
    if (idx < 0 || idx >= MAX_ATAB || low > high || elsz < 0 || size < 0) {
        return false;
    }

    initializeAtabEntry(&atab[idx]);
    atab[idx].xtyp = xtyp;
    atab[idx].xref = -1;
    atab[idx].etyp = etyp;
    atab[idx].eref = eref;
    atab[idx].low = low;
    atab[idx].high = high;
    atab[idx].elsz = elsz;
    atab[idx].size = size;

    if (idx + 1 > atabCount) {
        atabCount = idx + 1;
    }

    return true;
}

int symBlockForTabIndex(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= tabCount) {
        return -1;
    }

    for (int b = 0; b < btabCount; b++) {
        int current = btab[b].last;
        while (current != -1) {
            if (current == tabIndex) {
                return b;
            }
            current = tab[current].link;
        }
    }

    return -1;
}

int symFrameOffsetForTabIndex(int tabIndex) {
    int blockIndex = symBlockForTabIndex(tabIndex);
    int offset = 0;

    if (blockIndex < 0 || tabIndex < 0 || tabIndex >= tabCount || tab[tabIndex].obj != OBJ_VARIABLE) {
        return -1;
    }

    for (int i = 0; i < tabCount; i++) {
        if (symBlockForTabIndex(i) != blockIndex || tab[i].obj != OBJ_VARIABLE) {
            continue;
        }
        if (i == tabIndex) {
            return offset;
        }
        offset += sizeOfType(tab[i].type, tab[i].ref) > 0 ? sizeOfType(tab[i].type, tab[i].ref) : 1;
    }

    return -1;
}

int symFrameSlotCountForBlock(int blockIndex) {
    int slots = 0;

    if (blockIndex < 0 || blockIndex >= btabCount) {
        return 0;
    }

    for (int i = 0; i < tabCount; i++) {
        if (symBlockForTabIndex(i) == blockIndex && tab[i].obj == OBJ_VARIABLE) {
            int size = sizeOfType(tab[i].type, tab[i].ref);
            slots += size > 0 ? size : 1;
        }
    }

    return slots;
}

int symTabCount(void) {
    return tabCount;
}

int symBtabCount(void) {
    return btabCount;
}

int symAtabCount(void) {
    return atabCount;
}

int symEtabCount(void) {
    return etabCount;
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
               tab[i].link < 0 ? 0 : tab[i].link,
               objClassToString(tab[i].obj),
               baseTypeToString(tab[i].type),
               tab[i].ref < 0 ? 0 : tab[i].ref,
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
               btab[i].last < 0 ? 0 : btab[i].last,
               btab[i].lpar < 0 ? 0 : btab[i].lpar,
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
               atab[i].eref < 0 ? 0 : atab[i].eref,
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
