#include "ast_decorator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "symbol_table.h"

static char *g_message = NULL;
static size_t g_messageSize = 0;
static bool g_hasError = false;

static void semError(const char *format, ...) {
    if (g_hasError) {
        return;
    }
    g_hasError = true;

    if (g_message == NULL || g_messageSize == 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsnprintf(g_message, g_messageSize, format, args);
    va_end(args);
}

static BaseType baseTypeFromName(const char *name) {
    if (name == NULL) {
        return TYPE_NONE;
    }
    if (strcasecmp(name, "integer") == 0) return TYPE_INTEGER;
    if (strcasecmp(name, "real") == 0)    return TYPE_REAL;
    if (strcasecmp(name, "boolean") == 0) return TYPE_BOOLEAN;
    if (strcasecmp(name, "char") == 0)    return TYPE_CHAR;
    if (strcasecmp(name, "string") == 0)  return TYPE_STRING;
    return TYPE_NONE;
}

static bool typesCompatible(BaseType a, BaseType b) {
    if (a == TYPE_NONE || b == TYPE_NONE) {
        return false;
    }
    if (a == b) {
        return true;
    }
    if ((a == TYPE_INTEGER || a == TYPE_REAL) &&
        (b == TYPE_INTEGER || b == TYPE_REAL)) {
        return true;
    }
    return false;
}

static bool enumRefCompatible(int targetRef, int sourceRef) {
    bool targetIsEnum = targetRef >= 0 && symEnumCount(targetRef) > 0;
    bool sourceIsEnum = sourceRef >= 0 && symEnumCount(sourceRef) > 0;

    if (!targetIsEnum && !sourceIsEnum) {
        return true;
    }

    if (targetRef < 0 && sourceRef < 0) {
        return true;
    }
    return targetIsEnum && sourceIsEnum && targetRef == sourceRef;
}

static bool arrayTypesCompatible(int leftRef, int rightRef) {
    if (leftRef < 0 || rightRef < 0 ||
        leftRef >= symAtabCount() || rightRef >= symAtabCount()) {
        return false;
    }

    if (atab[leftRef].xtyp != atab[rightRef].xtyp ||
        atab[leftRef].xref != atab[rightRef].xref ||
        atab[leftRef].low != atab[rightRef].low ||
        atab[leftRef].high != atab[rightRef].high ||
        atab[leftRef].etyp != atab[rightRef].etyp ||
        atab[leftRef].elemHasRange != atab[rightRef].elemHasRange) {
        return false;
    }

    if (atab[leftRef].elemHasRange &&
        (atab[leftRef].elemRangeBase != atab[rightRef].elemRangeBase ||
         atab[leftRef].elemRangeLow != atab[rightRef].elemRangeLow ||
         atab[leftRef].elemRangeHigh != atab[rightRef].elemRangeHigh)) {
        return false;
    }

    if (atab[leftRef].etyp == TYPE_ARRAY) {
        return arrayTypesCompatible(atab[leftRef].eref, atab[rightRef].eref);
    }

    if (atab[leftRef].etyp == TYPE_RECORD) {
        return atab[leftRef].eref == atab[rightRef].eref;
    }

    return enumRefCompatible(atab[leftRef].eref, atab[rightRef].eref);
}

static bool assignmentCompatibleRef(BaseType targetType, int targetRef,
                                    BaseType sourceType, int sourceRef) {
    if (targetType == TYPE_ARRAY || sourceType == TYPE_ARRAY) {
        return targetType == TYPE_ARRAY && sourceType == TYPE_ARRAY &&
               arrayTypesCompatible(targetRef, sourceRef);
    }

    if (targetType == TYPE_RECORD || sourceType == TYPE_RECORD) {
        return targetType == TYPE_RECORD && sourceType == TYPE_RECORD &&
               targetRef == sourceRef;
    }

    if (targetType == TYPE_INTEGER || sourceType == TYPE_INTEGER) {
        if (!enumRefCompatible(targetRef, sourceRef)) {
            return false;
        }
    }

    if (targetType == TYPE_NONE || sourceType == TYPE_NONE) {
        return false;
    }
    if (targetType == sourceType) {
        return true;
    }
    if (targetType == TYPE_REAL && sourceType == TYPE_INTEGER && sourceRef < 0) {
        return true;
    }
    return false;
}

typedef struct {
    BaseType type;
    int ref;
    bool hasRange;
    BaseType rangeBase;
    int low;
    int high;
} TypeInfo;

static TypeInfo makeTypeInfo(BaseType type, int ref) {
    TypeInfo info;
    info.type = type;
    info.ref = ref;
    info.hasRange = false;
    info.rangeBase = TYPE_NONE;
    info.low = 0;
    info.high = 0;
    return info;
}

static TypeInfo typeInfoFromTab(int idx) {
    TypeInfo info = makeTypeInfo(TYPE_NONE, -1);
    if (idx < 0) {
        return info;
    }
    info.type = tab[idx].type;
    info.ref = tab[idx].ref;
    info.hasRange = tab[idx].hasRange;
    info.rangeBase = tab[idx].rangeBase;
    info.low = tab[idx].rangeLow;
    info.high = tab[idx].rangeHigh;
    return info;
}

static bool constantValue(AstNode *node, long long *value, BaseType *type) {
    if (node == NULL || value == NULL || type == NULL) {
        return false;
    }

    switch (node->type) {
        case AST_INT_LIT:
            *value = node->ival;
            *type = TYPE_INTEGER;
            return true;
        case AST_CHAR_LIT:
            *value = node->sval != NULL ? (unsigned char)node->sval[0] : 0;
            *type = TYPE_CHAR;
            return true;
        case AST_BOOL_LIT:
            *value = (node->sval != NULL && strcasecmp(node->sval, "true") == 0) ? 1 : 0;
            *type = TYPE_BOOLEAN;
            return true;
        case AST_VAR: {
            int idx = symLookup(node->sval);
            if (idx != -1 && tab[idx].obj == OBJ_CONSTANT) {
                *value = tab[idx].adr;
                *type = tab[idx].type;
                return true;
            }
            return false;
        }
        case AST_UNOP: {
            long long innerValue = 0;
            BaseType innerType = TYPE_NONE;
            if (node->sval != NULL && strcmp(node->sval, "-") == 0 &&
                node->childCount == 1 &&
                constantValue(node->children[0], &innerValue, &innerType) &&
                innerType == TYPE_INTEGER) {
                *value = -innerValue;
                *type = TYPE_INTEGER;
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

static void visit(AstNode *node);
static BaseType visitExpression(AstNode *node);
static void visitStatement(AstNode *node);
static void visitDeclPart(AstNode *node);
static void visitBlock(AstNode *node);
static int arrayAccessResultRef(AstNode *node);
static BaseType visitFieldAccessMode(AstNode *node, bool requireInitialized);
static int typeRefForNode(AstNode *node);
static BaseType lvalueType(AstNode *target);
static void markAssigned(AstNode *target);

static TypeInfo resolveTypeInfo(AstNode *typeNode) {
    if (typeNode == NULL) {
        return makeTypeInfo(TYPE_NONE, -1);
    }

    if (typeNode->type == AST_TYPE_IDENT) {
        BaseType bt = baseTypeFromName(typeNode->sval);
        if (bt != TYPE_NONE) {
            TypeInfo info = makeTypeInfo(bt, -1);
            if (bt == TYPE_BOOLEAN) {
                info.hasRange = true;
                info.rangeBase = TYPE_BOOLEAN;
                info.low = 0;
                info.high = 1;
            }
            return info;
        }
        int idx = symLookup(typeNode->sval);
        if (idx != -1 && tab[idx].obj == OBJ_TYPE) {
            typeNode->tabIdx = idx;
            typeNode->typeIdx = tab[idx].type;
            typeNode->lexLevel = tab[idx].lev;
            return typeInfoFromTab(idx);
        }
        semError("Unknown type '%s'.", typeNode->sval ? typeNode->sval : "?");
        return makeTypeInfo(TYPE_NONE, -1);
    }

    if (typeNode->type == AST_RANGE) {
        long long lowValue = 0;
        long long highValue = 0;
        BaseType lowType = TYPE_NONE;
        BaseType highType = TYPE_NONE;
        TypeInfo info;

        if (typeNode->childCount < 2 ||
            !constantValue(typeNode->children[0], &lowValue, &lowType) ||
            !constantValue(typeNode->children[1], &highValue, &highType)) {
            semError("Batas subrange harus berupa konstanta yang diketahui.");
            return makeTypeInfo(TYPE_NONE, -1);
        }

        if (lowType == TYPE_REAL || highType == TYPE_REAL) {
            semError("Subrange tidak boleh memiliki type Real.");
            return makeTypeInfo(TYPE_NONE, -1);
        }

        if (lowType != highType) {
            semError("Batas subrange harus memiliki tipe yang sama.");
            return makeTypeInfo(TYPE_NONE, -1);
        }

        if (lowValue > highValue) {
            semError("Lower bound subrange (%lld) lebih besar dari upper bound (%lld).",
                     lowValue, highValue);
            return makeTypeInfo(TYPE_NONE, -1);
        }

        info = makeTypeInfo(lowType, -1);
        info.hasRange = true;
        info.rangeBase = lowType;
        info.low = (int)lowValue;
        info.high = (int)highValue;
        typeNode->typeIdx = lowType;
        return info;
    }

    if (typeNode->type == AST_ENUMERATED) {
        int enumRef = symEnterEnum((int)typeNode->childCount);
        if (enumRef == -1) {
            semError("Gagal membuat domain enumerated.");
            return makeTypeInfo(TYPE_NONE, -1);
        }

        for (size_t i = 0; i < typeNode->childCount; i++) {
            AstNode *id = typeNode->children[i];
            int r = symEnter(id->sval, OBJ_CONSTANT, TYPE_INTEGER, enumRef, 1, (int)i);
            if (r == -1) {
                semError("Redeklarasi identifier enumerated '%s'.", id->sval ? id->sval : "?");
                return makeTypeInfo(TYPE_NONE, -1);
            }
            id->tabIdx = r;
            id->typeIdx = TYPE_INTEGER;
            id->lexLevel = tab[r].lev;
        }
        typeNode->typeIdx = TYPE_INTEGER;
        TypeInfo info = makeTypeInfo(TYPE_INTEGER, enumRef);
        info.hasRange = true;
        info.rangeBase = TYPE_INTEGER;
        info.low = 0;
        info.high = (int)typeNode->childCount - 1;
        return info;
    }

    if (typeNode->type == AST_ARRAY_TYPE) {
        AstNode *indexN = typeNode->childCount >= 1 ? typeNode->children[0] : NULL;
        AstNode *elemN = typeNode->childCount >= 2 ? typeNode->children[1] : NULL;
        TypeInfo indexInfo = makeTypeInfo(TYPE_INTEGER, -1);
        TypeInfo elemInfo = makeTypeInfo(TYPE_INTEGER, -1);
        int low = 0;
        int high = 0;

        if (indexN != NULL) {
            indexInfo = resolveTypeInfo(indexN);
            if (indexInfo.type == TYPE_NONE) {
                return makeTypeInfo(TYPE_NONE, -1);
            }
            if (indexInfo.type == TYPE_REAL || indexInfo.type == TYPE_ARRAY ||
                indexInfo.type == TYPE_RECORD || indexInfo.type == TYPE_STRING ||
                indexInfo.type == TYPE_VOID) {
                semError("Index type array harus simple type dan bukan Real.");
                return makeTypeInfo(TYPE_NONE, -1);
            }
            if (indexInfo.hasRange) {
                low = indexInfo.low;
                high = indexInfo.high;
            } else if (indexInfo.type == TYPE_BOOLEAN) {
                low = 0;
                high = 1;
            } else if (indexInfo.ref >= 0 && indexInfo.type == TYPE_INTEGER &&
                       symEnumCount(indexInfo.ref) > 0) {
                low = 0;
                high = symEnumCount(indexInfo.ref) - 1;
            }
        }

        if (elemN != NULL) {
            elemInfo = resolveTypeInfo(elemN);
            if (elemInfo.type == TYPE_NONE) {
                return makeTypeInfo(TYPE_NONE, -1);
            }
        }

        int elsz = sizeOfType(elemInfo.type, elemInfo.ref);
        if (elsz == 0) {
            elsz = 1;
        }

        int aref = symEnterArray(indexInfo.type, indexInfo.ref,
                                 elemInfo.type, elemInfo.ref,
                                 low, high, elsz,
                                 elemInfo.hasRange, elemInfo.rangeBase,
                                 elemInfo.low, elemInfo.high);
        if (aref == -1) {
            semError("Gagal memasukkan tipe array ke symbol table.");
            return makeTypeInfo(TYPE_NONE, -1);
        }
        typeNode->typeIdx = TYPE_ARRAY;
        return makeTypeInfo(TYPE_ARRAY, aref);
    }

    if (typeNode->type == AST_RECORD_TYPE) {
        int blockIdx = symEnterRecordBlock();
        int adr = 0;
        if (blockIdx == -1) {
            semError("Gagal membuat block record.");
            return makeTypeInfo(TYPE_NONE, -1);
        }
        for (size_t i = 0; i < typeNode->childCount; i++) {
            AstNode *fp = typeNode->children[i];
            if (fp->type != AST_FIELD_PART || fp->childCount < 2) {
                continue;
            }
            AstNode *idList = fp->children[0];
            AstNode *fieldType = fp->children[1];
            TypeInfo fieldInfo = resolveTypeInfo(fieldType);
            if (fieldInfo.type == TYPE_NONE) {
                symExitRecordBlock();
                return makeTypeInfo(TYPE_NONE, -1);
            }
            for (size_t j = 0; j < idList->childCount; j++) {
                int f = symEnterField(idList->children[j]->sval, fieldInfo.type, fieldInfo.ref, adr);
                if (f == -1) {
                    semError("Redeklarasi field '%s'.", idList->children[j]->sval ? idList->children[j]->sval : "?");
                    symExitRecordBlock();
                    return makeTypeInfo(TYPE_NONE, -1);
                }
                if (fieldInfo.hasRange) {
                    symSetRange(f, fieldInfo.rangeBase, fieldInfo.low, fieldInfo.high);
                }
                idList->children[j]->tabIdx = f;
                idList->children[j]->typeIdx = fieldInfo.type;
                idList->children[j]->lexLevel = tab[f].lev;
                adr += sizeOfType(fieldInfo.type, fieldInfo.ref);
            }
        }
        symExitRecordBlock();
        typeNode->typeIdx = TYPE_RECORD;
        return makeTypeInfo(TYPE_RECORD, blockIdx);
    }

    return makeTypeInfo(TYPE_NONE, -1);
}

static BaseType resolveVarReference(AstNode *node, bool requireInitialized) {
    int idx = symLookup(node->sval);
    if (idx == -1) {
        semError("Identifier '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    if (tab[idx].obj != OBJ_VARIABLE && tab[idx].obj != OBJ_CONSTANT) {
        semError("Identifier '%s' tidak dapat digunakan sebagai nilai.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    if (requireInitialized && tab[idx].obj == OBJ_VARIABLE &&
        !tab[idx].initialized && tab[idx].lev == currentLevel) {
        semError("Variabel '%s' digunakan sebelum diinisialisasi.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;
    node->typeIdx = tab[idx].type;
    return tab[idx].type;
}

static BaseType visitVarRef(AstNode *node) {
    return resolveVarReference(node, true);
}

static BaseType visitArrayAccessMode(AstNode *node, bool requireInitialized) {
    if (node->childCount < 1) {
        return TYPE_NONE;
    }
    AstNode *base = node->children[0];
    BaseType baseType = TYPE_NONE;
    int aref = -1;

    if (base->type == AST_VAR) {
        baseType = resolveVarReference(base, requireInitialized);
        if (baseType == TYPE_NONE) {
            return TYPE_NONE;
        }
        aref = tab[base->tabIdx].ref;
    } else if (base->type == AST_ARRAY_ACCESS) {
        baseType = visitArrayAccessMode(base, requireInitialized);
        aref = arrayAccessResultRef(base);
    } else if (base->type == AST_FIELD_ACCESS) {
        baseType = visitFieldAccessMode(base, requireInitialized);
        aref = base->tabIdx >= 0 ? tab[base->tabIdx].ref : -1;
    } else {
        semError("Target index array bukan array.");
        return TYPE_NONE;
    }

    node->tabIdx = base->tabIdx;
    node->lexLevel = base->lexLevel;

    if (baseType != TYPE_ARRAY) {
        semError("Target index array bukan array.");
        return TYPE_NONE;
    }

    BaseType currentType = baseType;
    for (size_t i = 1; i < node->childCount; i++) {
        if (currentType != TYPE_ARRAY || aref < 0) {
            semError("Jumlah index array melebihi dimensi array.");
            return TYPE_NONE;
        }
        BaseType it = visitExpression(node->children[i]);
        if (it == TYPE_NONE) {
            return TYPE_NONE;
        }
        int indexRef = typeRefForNode(node->children[i]);
        if (aref >= 0 && it != atab[aref].xtyp &&
            !((it == TYPE_INTEGER) && (atab[aref].xtyp == TYPE_INTEGER))) {
            if (it != atab[aref].xtyp) {
                semError("Tipe index array tidak sesuai.");
                return TYPE_NONE;
            }
        }
        if (aref >= 0 && atab[aref].xref >= 0 &&
            indexRef >= 0 && indexRef != atab[aref].xref) {
            semError("Domain index array tidak sesuai.");
            return TYPE_NONE;
        }
        if (atab[aref].low < atab[aref].high) {
            long long value = 0;
            BaseType valueType = TYPE_NONE;
            if (constantValue(node->children[i], &value, &valueType) &&
                valueType == atab[aref].xtyp &&
                (value < atab[aref].low || value > atab[aref].high)) {
                semError("Index array berada di luar range %d..%d.",
                         atab[aref].low,
                         atab[aref].high);
                return TYPE_NONE;
            }
        }
        currentType = atab[aref].etyp;
        aref = atab[aref].eref;
    }

    node->typeIdx = currentType;
    return currentType;
}

static BaseType visitArrayAccess(AstNode *node) {
    return visitArrayAccessMode(node, true);
}

static int arrayAccessResultRef(AstNode *node) {
    if (node == NULL || node->type != AST_ARRAY_ACCESS || node->childCount < 1) {
        return -1;
    }

    AstNode *base = node->children[0];
    BaseType currentType = TYPE_NONE;
    int ref = -1;

    if (base->type == AST_VAR) {
        int idx = symLookup(base->sval);
        if (idx == -1) return -1;
        currentType = tab[idx].type;
        ref = tab[idx].ref;
    } else if (base->type == AST_ARRAY_ACCESS) {
        currentType = base->typeIdx;
        ref = arrayAccessResultRef(base);
    } else if (base->type == AST_FIELD_ACCESS) {
        currentType = base->typeIdx;
        ref = base->tabIdx >= 0 ? tab[base->tabIdx].ref : -1;
    }

    for (size_t i = 1; i < node->childCount; i++) {
        if (currentType != TYPE_ARRAY || ref < 0) {
            return -1;
        }
        currentType = atab[ref].etyp;
        ref = atab[ref].eref;
    }

    return ref;
}

static BaseType visitFieldAccessMode(AstNode *node, bool requireInitialized) {
    if (node->childCount < 2) {
        return TYPE_NONE;
    }
    AstNode *base = node->children[0];
    AstNode *field = node->children[1];
    BaseType baseType = TYPE_NONE;
    int baseRef = -1;

    if (base->type == AST_VAR) {
        baseType = resolveVarReference(base, requireInitialized);
        if (baseType == TYPE_NONE) {
            return TYPE_NONE;
        }
        int idx = base->tabIdx;
        baseRef = tab[idx].ref;
    } else if (base->type == AST_ARRAY_ACCESS) {
        baseType = visitArrayAccessMode(base, requireInitialized);
        baseRef = arrayAccessResultRef(base);
    } else if (base->type == AST_FIELD_ACCESS) {
        baseType = visitFieldAccessMode(base, requireInitialized);
        baseRef = base->tabIdx >= 0 ? tab[base->tabIdx].ref : -1;
    } else {
        semError("Akses field hanya didukung pada nilai bertipe record.");
        return TYPE_NONE;
    }

    if (baseType != TYPE_RECORD || baseRef < 0) {
        semError("Target akses field bukan record.");
        return TYPE_NONE;
    }

    int fi = btab[baseRef].last;
    while (fi != -1) {
        if (tab[fi].identifier != NULL && field->sval != NULL &&
            strcmp(tab[fi].identifier, field->sval) == 0) {
            field->tabIdx = fi;
            field->typeIdx = tab[fi].type;
            field->lexLevel = tab[fi].lev;
            node->tabIdx = fi;
            node->lexLevel = tab[fi].lev;
            node->typeIdx = tab[fi].type;
            return tab[fi].type;
        }
        fi = tab[fi].link;
    }

    semError("Field '%s' tidak ditemukan pada record.",
             field->sval ? field->sval : "?");
    return TYPE_NONE;
}

static BaseType visitFieldAccess(AstNode *node) {
    return visitFieldAccessMode(node, true);
}

static BaseType visitCall(AstNode *node) {
    int idx = symLookup(node->sval);
    if (idx != -1 && tab[idx].obj != OBJ_PROCEDURE && tab[idx].obj != OBJ_FUNCTION) {
        idx = -1;
    }
    if (idx == -1 && node->sval != NULL) {
        for (int i = symTabCount() - 1; i >= 0; i--) {
            if (tab[i].identifier != NULL &&
                strcasecmp(tab[i].identifier, node->sval) == 0 &&
                (tab[i].obj == OBJ_PROCEDURE || tab[i].obj == OBJ_FUNCTION)) {
                idx = i;
                break;
            }
        }
    }
    if (idx == -1) {
        semError("Procedure/function '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;

    if (tab[idx].obj != OBJ_PROCEDURE && tab[idx].obj != OBJ_FUNCTION) {
        semError("Identifier '%s' bukan procedure atau function.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    if (node->type == AST_FUNC_CALL && tab[idx].obj != OBJ_FUNCTION) {
        semError("Procedure '%s' tidak dapat digunakan sebagai expression.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }

    if (node->sval != NULL &&
        (strcasecmp(node->sval, "read") == 0 || strcasecmp(node->sval, "readln") == 0)) {
        for (size_t i = 0; i < node->childCount; i++) {
            AstNode *pl = node->children[i];
            if (pl->type == AST_PARAM_LIST) {
                for (size_t j = 0; j < pl->childCount; j++) {
                    if (lvalueType(pl->children[j]) == TYPE_NONE) {
                        semError("Parameter read/readln ke-%zu harus berupa variabel.",
                                 j + 1);
                        return TYPE_NONE;
                    }
                    markAssigned(pl->children[j]);
                }
            }
        }
        node->typeIdx = TYPE_VOID;
        return TYPE_VOID;
    }

    int actualCount = 0;
    BaseType actualTypes[128];
    int actualRefs[128];
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *pl = node->children[i];
        if (pl->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < pl->childCount; j++) {
                BaseType actual = visitExpression(pl->children[j]);
                if (actual == TYPE_NONE) {
                    return TYPE_NONE;
                }
                if (actualCount < (int)(sizeof(actualTypes) / sizeof(actualTypes[0]))) {
                    actualTypes[actualCount] = actual;
                    actualRefs[actualCount] = typeRefForNode(pl->children[j]);
                }
                actualCount++;
            }
        }
    }

    if (tab[idx].nrm >= 0) {
        if (actualCount != tab[idx].nrm) {
            semError("Jumlah parameter '%s' tidak sesuai: diharapkan %d, ditemukan %d.",
                     node->sval ? node->sval : "?", tab[idx].nrm, actualCount);
            return TYPE_NONE;
        }

        if (tab[idx].ref >= 0 && tab[idx].ref < symBtabCount() && tab[idx].nrm > 0) {
            int formalIdx[128];
            int formalCount = 0;
            int p = btab[tab[idx].ref].lpar;
            while (p > 0 && formalCount < tab[idx].nrm &&
                   formalCount < (int)(sizeof(formalIdx) / sizeof(formalIdx[0]))) {
                formalIdx[formalCount++] = p;
                p = tab[p].link;
            }
            if (formalCount == tab[idx].nrm) {
                for (int i = 0; i < formalCount; i++) {
                    int f = formalIdx[formalCount - 1 - i];
                    bool ok;
                    ok = assignmentCompatibleRef(tab[f].type, tab[f].ref,
                                                 actualTypes[i], actualRefs[i]);
                    if (!ok) {
                        semError("Tipe parameter ke-%d pada '%s' tidak sesuai.",
                                 i + 1, node->sval ? node->sval : "?");
                        return TYPE_NONE;
                    }
                }
            }
        }
    }

    node->typeIdx = tab[idx].type;
    return tab[idx].type;
}

static bool isArithmeticOp(const char *op) {
    return op != NULL && (strcmp(op, "plus") == 0 || strcmp(op, "minus") == 0 ||
                          strcmp(op, "times") == 0 || strcmp(op, "rdiv") == 0 ||
                          strcmp(op, "idiv") == 0 || strcmp(op, "imod") == 0);
}

static bool isRelationalOp(const char *op) {
    return op != NULL && (strcmp(op, "eql") == 0 || strcmp(op, "neq") == 0 ||
                          strcmp(op, "lss") == 0 || strcmp(op, "leq") == 0 ||
                          strcmp(op, "gtr") == 0 || strcmp(op, "geq") == 0);
}

static bool isBooleanOp(const char *op) {
    return op != NULL && (strcmp(op, "andsy") == 0 || strcmp(op, "orsy") == 0);
}

static BaseType visitBinOp(AstNode *node) {
    if (node->childCount < 2) {
        return TYPE_NONE;
    }
    BaseType lt = visitExpression(node->children[0]);
    BaseType rt = visitExpression(node->children[1]);
    if (lt == TYPE_NONE || rt == TYPE_NONE) {
        return TYPE_NONE;
    }

    const char *op = node->sval;

    if (isBooleanOp(op)) {
        if (lt != TYPE_BOOLEAN || rt != TYPE_BOOLEAN) {
            semError("Operator '%s' membutuhkan operand boolean.", op);
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (isRelationalOp(op)) {
        if (!typesCompatible(lt, rt)) {
            semError("Operand operator relasional '%s' bertipe tidak kompatibel.", op);
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (isArithmeticOp(op)) {
        if (!((lt == TYPE_INTEGER || lt == TYPE_REAL) &&
              (rt == TYPE_INTEGER || rt == TYPE_REAL))) {
            semError("Operator aritmatika '%s' membutuhkan operand numerik.", op);
            return TYPE_NONE;
        }
        if ((strcmp(op, "idiv") == 0 || strcmp(op, "imod") == 0) &&
            (lt != TYPE_INTEGER || rt != TYPE_INTEGER)) {
            semError("Operator '%s' membutuhkan operand integer.", op);
            return TYPE_NONE;
        }
        BaseType res;
        if (strcmp(op, "rdiv") == 0) {
            res = TYPE_REAL;
        } else if (lt == TYPE_REAL || rt == TYPE_REAL) {
            res = TYPE_REAL;
        } else {
            res = TYPE_INTEGER;
        }
        node->typeIdx = res;
        return res;
    }

    node->typeIdx = lt;
    return lt;
}

static BaseType visitUnOp(AstNode *node) {
    if (node->childCount < 1) {
        return TYPE_NONE;
    }
    BaseType t = visitExpression(node->children[0]);
    if (t == TYPE_NONE) {
        return TYPE_NONE;
    }

    if (node->sval != NULL && strcmp(node->sval, "not") == 0) {
        if (t != TYPE_BOOLEAN) {
            semError("Operator 'not' membutuhkan operand boolean.");
            return TYPE_NONE;
        }
        node->typeIdx = TYPE_BOOLEAN;
        return TYPE_BOOLEAN;
    }

    if (t != TYPE_INTEGER && t != TYPE_REAL) {
        semError("Operator unary '%s' membutuhkan operand numerik.",
                 node->sval ? node->sval : "?");
        return TYPE_NONE;
    }
    node->typeIdx = t;
    return t;
}

static BaseType visitExpression(AstNode *node) {
    if (node == NULL) {
        return TYPE_NONE;
    }

    switch (node->type) {
        case AST_INT_LIT:
            node->typeIdx = TYPE_INTEGER;
            return TYPE_INTEGER;
        case AST_REAL_LIT:
            node->typeIdx = TYPE_REAL;
            return TYPE_REAL;
        case AST_CHAR_LIT:
            node->typeIdx = TYPE_CHAR;
            return TYPE_CHAR;
        case AST_STRING_LIT:
            node->typeIdx = TYPE_STRING;
            return TYPE_STRING;
        case AST_BOOL_LIT:
            node->typeIdx = TYPE_BOOLEAN;
            return TYPE_BOOLEAN;
        case AST_VAR:
            return visitVarRef(node);
        case AST_ARRAY_ACCESS:
            return visitArrayAccess(node);
        case AST_FIELD_ACCESS:
            return visitFieldAccess(node);
        case AST_FUNC_CALL:
            return visitCall(node);
        case AST_BINOP:
            return visitBinOp(node);
        case AST_UNOP:
            return visitUnOp(node);
        default:
            return TYPE_NONE;
    }
}

static BaseType lvalueType(AstNode *target) {
    if (target == NULL) {
        return TYPE_NONE;
    }
    if (target->type == AST_VAR) {
        BaseType t = resolveVarReference(target, false);
        if (t != TYPE_NONE && target->tabIdx >= 0 && tab[target->tabIdx].obj != OBJ_VARIABLE) {
            semError("Identifier '%s' bukan variabel yang dapat di-assign.",
                     target->sval ? target->sval : "?");
            return TYPE_NONE;
        }
        return t;
    }
    if (target->type == AST_ARRAY_ACCESS) {
        return visitArrayAccessMode(target, false);
    }
    if (target->type == AST_FIELD_ACCESS) {
        return visitFieldAccessMode(target, false);
    }
    return TYPE_NONE;
}

static int typeRefForNode(AstNode *node) {
    if (node == NULL) {
        return -1;
    }
    if (node->type == AST_VAR) {
        return node->tabIdx >= 0 ? tab[node->tabIdx].ref : -1;
    }
    if (node->type == AST_ARRAY_ACCESS) {
        return arrayAccessResultRef(node);
    }
    if (node->type == AST_FIELD_ACCESS) {
        return node->tabIdx >= 0 ? tab[node->tabIdx].ref : -1;
    }
    if (node->type == AST_FUNC_CALL) {
        return node->tabIdx >= 0 ? tab[node->tabIdx].ref : -1;
    }
    return -1;
}

static void markAssigned(AstNode *target) {
    if (target == NULL) {
        return;
    }
    if (target->type == AST_VAR) {
        if (target->tabIdx >= 0 && tab[target->tabIdx].obj == OBJ_VARIABLE) {
            tab[target->tabIdx].initialized = true;
        }
        return;
    }
    if ((target->type == AST_ARRAY_ACCESS || target->type == AST_FIELD_ACCESS) &&
        target->childCount > 0) {
        markAssigned(target->children[0]);
    }
}

static void visitAssign(AstNode *node) {
    if (node->childCount < 2) {
        return;
    }
    AstNode *target = node->children[0];
    AstNode *expr = node->children[1];

    BaseType lt = lvalueType(target);
    if (lt == TYPE_NONE) {
        return;
    }
    BaseType rt = visitExpression(expr);
    if (rt == TYPE_NONE) {
        return;
    }

    if (lt == TYPE_ARRAY || lt == TYPE_RECORD || rt == TYPE_ARRAY || rt == TYPE_RECORD) {
        semError("Structured assignment tidak didukung: assign array/record secara utuh tidak diperbolehkan.");
        return;
    } else if (!assignmentCompatibleRef(lt, typeRefForNode(target), rt, typeRefForNode(expr))) {
        semError("Type mismatch: tidak dapat assign %s ke %s.",
                 baseTypeToString(rt), baseTypeToString(lt));
        return;
    }
    if (target->tabIdx >= 0 && tab[target->tabIdx].hasRange) {
        long long value = 0;
        BaseType valueType = TYPE_NONE;
        if (constantValue(expr, &value, &valueType) &&
            valueType == tab[target->tabIdx].rangeBase &&
            (value < tab[target->tabIdx].rangeLow || value > tab[target->tabIdx].rangeHigh)) {
            semError("Nilai assignment untuk '%s' berada di luar range %d..%d.",
                     target->sval ? target->sval : "?",
                     tab[target->tabIdx].rangeLow,
                     tab[target->tabIdx].rangeHigh);
            return;
        }
    }
    markAssigned(target);
    node->typeIdx = TYPE_VOID;
    node->tabIdx = target->tabIdx;
    node->lexLevel = target->lexLevel;
}

static void visitIf(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType ct = visitExpression(node->children[0]);
    if (ct == TYPE_NONE) {
        return;
    }
    if (ct != TYPE_BOOLEAN) {
        semError("Kondisi if-statement harus bertipe boolean.");
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        visitStatement(node->children[i]);
    }
}

static void visitWhile(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType ct = visitExpression(node->children[0]);
    if (ct == TYPE_NONE) {
        return;
    }
    if (ct != TYPE_BOOLEAN) {
        semError("Kondisi while-statement harus bertipe boolean.");
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        visitStatement(node->children[i]);
    }
}

static void visitFor(AstNode *node) {
    int idx = symLookup(node->sval);
    if (idx == -1) {
        semError("Variabel counter '%s' belum dideklarasikan.",
                 node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = idx;
    node->lexLevel = tab[idx].lev;
    if (tab[idx].obj != OBJ_VARIABLE) {
        semError("Counter for-loop '%s' harus berupa variabel.",
                 node->sval ? node->sval : "?");
        return;
    }
    if (tab[idx].type != TYPE_INTEGER && tab[idx].type != TYPE_CHAR) {
        semError("Variabel counter '%s' harus bertipe integer/char.",
                 node->sval ? node->sval : "?");
        return;
    }

    size_t exprSeen = 0;
    tab[idx].initialized = true;
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            visitStatement(c);
        } else if (exprSeen < 2) {
            BaseType bt = visitExpression(c);
            if (bt != TYPE_NONE && bt != tab[idx].type &&
                !(bt == TYPE_INTEGER && tab[idx].type == TYPE_INTEGER)) {
                if (bt != tab[idx].type) {
                    semError("Batas for-loop tidak kompatibel dengan counter '%s'.",
                             node->sval ? node->sval : "?");
                    return;
                }
            }
            exprSeen++;
        }
    }
}

static void visitRepeat(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_STMT_LIST) {
            visitStatement(c);
        } else {
            BaseType ct = visitExpression(c);
            if (ct != TYPE_NONE && ct != TYPE_BOOLEAN) {
                semError("Kondisi repeat-until harus bertipe boolean.");
                return;
            }
        }
    }
}

static void visitCase(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    BaseType sel = visitExpression(node->children[0]);
    if (sel == TYPE_NONE) {
        return;
    }
    for (size_t i = 1; i < node->childCount; i++) {
        AstNode *cb = node->children[i];
        if (cb->type != AST_CASE_BLOCK) {
            continue;
        }
        for (size_t j = 0; j < cb->childCount; j++) {
            AstNode *c = cb->children[j];
            if (c->type == AST_INT_LIT || c->type == AST_CHAR_LIT ||
                c->type == AST_BOOL_LIT || c->type == AST_VAR) {
                BaseType labelType = visitExpression(c);
                if (labelType == TYPE_NONE) {
                    return;
                }
                if (!typesCompatible(sel, labelType)) {
                    semError("Tipe label case tidak kompatibel dengan ekspresi case.");
                    return;
                }
            } else {
                visitStatement(c);
            }
        }
    }
}

static void visitStatement(AstNode *node) {
    if (node == NULL) {
        return;
    }
    switch (node->type) {
        case AST_COMPOUND:
        case AST_STMT_LIST:
            node->typeIdx = TYPE_VOID;
            node->lexLevel = currentLevel;
            for (size_t i = 0; i < node->childCount; i++) {
                visitStatement(node->children[i]);
            }
            break;
        case AST_ASSIGN:
            visitAssign(node);
            break;
        case AST_IF:
            visitIf(node);
            break;
        case AST_WHILE:
            visitWhile(node);
            break;
        case AST_FOR:
            visitFor(node);
            break;
        case AST_REPEAT:
            visitRepeat(node);
            break;
        case AST_CASE:
            visitCase(node);
            break;
        case AST_PROC_CALL:
            (void)visitCall(node);
            break;
        case AST_EMPTY_STMT:
            break;
        default:
            break;
    }
}

static void visitConstDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    AstNode *valNode = node->children[0];
    BaseType bt = TYPE_NONE;
    switch (valNode->type) {
        case AST_INT_LIT:    bt = TYPE_INTEGER; break;
        case AST_REAL_LIT:   bt = TYPE_REAL;    break;
        case AST_CHAR_LIT:   bt = TYPE_CHAR;    break;
        case AST_STRING_LIT: bt = TYPE_STRING;  break;
        case AST_BOOL_LIT:   bt = TYPE_BOOLEAN; break;
        case AST_VAR: {
            int idx = symLookup(valNode->sval);
            if (idx != -1 && tab[idx].obj == OBJ_CONSTANT) {
                bt = tab[idx].type;
            }
            break;
        }
        default: break;
    }
    if (bt == TYPE_NONE) {
        semError("Tipe konstanta '%s' tidak diketahui.",
                 node->sval ? node->sval : "?");
        return;
    }
    long long value = 0;
    BaseType valueType = TYPE_NONE;
    if (!constantValue(valNode, &value, &valueType)) {
        value = 0;
    }
    (void)valueType;
    int r = symEnter(node->sval, OBJ_CONSTANT, bt, -1, 1, (int)value);
    if (r == -1) {
        semError("Redeklarasi konstanta '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = r;
    node->typeIdx = bt;
    node->lexLevel = tab[r].lev;
}

static void visitTypeDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    TypeInfo info = resolveTypeInfo(node->children[0]);
    if (info.type == TYPE_NONE) {
        if (!g_hasError) {
            semError("Tipe pada deklarasi type '%s' tidak diketahui.",
                     node->sval ? node->sval : "?");
        }
        return;
    }
    int r = symEnter(node->sval, OBJ_TYPE, info.type, info.ref, 1, 0);
    if (r == -1) {
        semError("Redeklarasi type '%s'.", node->sval ? node->sval : "?");
        return;
    }
    if (info.hasRange) {
        symSetRange(r, info.rangeBase, info.low, info.high);
    }
    node->tabIdx = r;
    node->typeIdx = info.type;
    node->lexLevel = tab[r].lev;
}

static void visitVarDecl(AstNode *node) {
    if (node->childCount < 1) {
        return;
    }
    TypeInfo info = resolveTypeInfo(node->children[0]);
    if (info.type == TYPE_NONE) {
        if (!g_hasError) {
            semError("Tipe variabel '%s' tidak diketahui.",
                     node->sval ? node->sval : "?");
        }
        return;
    }
    int r = symEnter(node->sval, OBJ_VARIABLE, info.type, info.ref, 1,
                     btab[display[currentLevel]].vsze);
    if (r == -1) {
        semError("Redeklarasi identifier '%s' pada scope yang sama.",
                 node->sval ? node->sval : "?");
        return;
    }
    if (info.hasRange) {
        symSetRange(r, info.rangeBase, info.low, info.high);
    }
    node->tabIdx = r;
    node->typeIdx = info.type;
    node->lexLevel = tab[r].lev;
}

static void visitParamGroup(AstNode *node, int funcTabIdx) {
    if (node->childCount < 2) {
        return;
    }
    AstNode *idList = node->children[0];
    AstNode *typeN = node->children[1];
    TypeInfo info = resolveTypeInfo(typeN);
    if (info.type == TYPE_NONE) {
        semError("Tipe parameter tidak diketahui.");
        return;
    }
    for (size_t i = 0; i < idList->childCount; i++) {
        int r = symEnter(idList->children[i]->sval, OBJ_VARIABLE, info.type, info.ref, 0,
                         btab[display[currentLevel]].psze);
        if (r == -1) {
            semError("Redeklarasi parameter '%s'.",
                     idList->children[i]->sval);
            return;
        }
        if (info.hasRange) {
            symSetRange(r, info.rangeBase, info.low, info.high);
        }
        idList->children[i]->tabIdx = r;
        idList->children[i]->typeIdx = info.type;
        idList->children[i]->lexLevel = tab[r].lev;
        if (funcTabIdx >= 0) {
            tab[funcTabIdx].nrm += 1;
        }
    }
}

static void visitProcDecl(AstNode *node) {
    int procIdx = symEnter(node->sval, OBJ_PROCEDURE, TYPE_VOID, -1, 0, 0);
    if (procIdx == -1) {
        semError("Redeklarasi procedure '%s'.", node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = procIdx;
    node->lexLevel = tab[procIdx].lev;

    symEnterScope();
    tab[procIdx].ref = display[currentLevel];
    tab[procIdx].nrm = 0;

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < c->childCount; j++) {
                if (c->children[j]->type == AST_PARAM_GROUP) {
                    visitParamGroup(c->children[j], procIdx);
                    if (g_hasError) {
                        symExitScope();
                        return;
                    }
                }
            }
        } else if (c->type == AST_BLOCK) {
            visitBlock(c);
            if (g_hasError) {
                symExitScope();
                return;
            }
        }
    }

    symExitScope();
}

static void visitFuncDecl(AstNode *node) {
    TypeInfo retInfo = makeTypeInfo(TYPE_NONE, -1);
    AstNode *retNode = NULL;
    if (node->childCount >= 1 && node->children[0]->type == AST_TYPE_IDENT) {
        retNode = node->children[0];
        retInfo = resolveTypeInfo(retNode);
    }
    if (retInfo.type == TYPE_NONE) {
        semError("Tipe return function '%s' tidak diketahui.",
                 node->sval ? node->sval : "?");
        return;
    }

    int funcIdx = symEnter(node->sval, OBJ_FUNCTION, retInfo.type, retInfo.ref, 0, 0);
    if (funcIdx == -1) {
        semError("Redeklarasi function '%s'.", node->sval ? node->sval : "?");
        return;
    }
    if (retInfo.hasRange) {
        symSetRange(funcIdx, retInfo.rangeBase, retInfo.low, retInfo.high);
    }
    node->tabIdx = funcIdx;
    node->typeIdx = retInfo.type;
    node->lexLevel = tab[funcIdx].lev;
    if (retNode != NULL) {
        retNode->typeIdx = retInfo.type;
    }

    symEnterScope();
    tab[funcIdx].ref = display[currentLevel];
    tab[funcIdx].nrm = 0;

    {
        int retVar = symEnter(node->sval, OBJ_VARIABLE, retInfo.type, retInfo.ref, 1, 0);
        if (retVar == -1) {
            semError("Gagal membuat variabel return function '%s'.",
                     node->sval ? node->sval : "?");
            symExitScope();
            return;
        }
        if (retInfo.hasRange) {
            symSetRange(retVar, retInfo.rangeBase, retInfo.low, retInfo.high);
        }
    }

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_PARAM_LIST) {
            for (size_t j = 0; j < c->childCount; j++) {
                if (c->children[j]->type == AST_PARAM_GROUP) {
                    visitParamGroup(c->children[j], funcIdx);
                    if (g_hasError) {
                        symExitScope();
                        return;
                    }
                }
            }
        } else if (c->type == AST_BLOCK) {
            visitBlock(c);
            if (g_hasError) {
                symExitScope();
                return;
            }
        }
    }

    symExitScope();
}

static void visitDeclPart(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        switch (c->type) {
            case AST_DECL_PART:
                visitDeclPart(c);
                break;
            case AST_CONST_DECL:
                visitConstDecl(c);
                break;
            case AST_TYPE_DECL:
                visitTypeDecl(c);
                break;
            case AST_VAR_DECL:
                visitVarDecl(c);
                break;
            case AST_PROC_DECL:
                visitProcDecl(c);
                break;
            case AST_FUNC_DECL:
                visitFuncDecl(c);
                break;
            default:
                break;
        }
        if (g_hasError) {
            return;
        }
    }
}

static void visitBlock(AstNode *node) {
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_DECL_PART) {
            visitDeclPart(c);
        }
        if (g_hasError) {
            return;
        }
    }
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            visitStatement(c);
        }
        if (g_hasError) {
            return;
        }
    }
}

static void visit(AstNode *node) {
    if (node == NULL) {
        semError("AST kosong.");
        return;
    }
    if (node->type != AST_PROGRAM) {
        semError("Root AST harus berupa Program.");
        return;
    }

    int pIdx = symEnter(node->sval, OBJ_PROGRAM, TYPE_VOID, -1, 1, 0);
    if (pIdx == -1) {
        semError("Redeklarasi identifier program '%s'.",
                 node->sval ? node->sval : "?");
        return;
    }
    node->tabIdx = pIdx;
    node->lexLevel = 0;

    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_DECL_PART) {
            visitDeclPart(c);
        }
        if (g_hasError) {
            return;
        }
    }
    for (size_t i = 0; i < node->childCount; i++) {
        AstNode *c = node->children[i];
        if (c->type == AST_COMPOUND) {
            symEnterScope();
            visitStatement(c);
            symExitScope();
        }
        if (g_hasError) {
            return;
        }
    }
}

bool decorateAst(AstNode *root, char *message, size_t messageSize) {
    g_message = message;
    g_messageSize = messageSize;
    g_hasError = false;
    if (message != NULL && messageSize > 0) {
        message[0] = '\0';
    }

    visit(root);

    return !g_hasError;
}

static const char *decoratedTypeName(AstNodeType type) {
    switch (type) {
        case AST_PROGRAM:       return "Program";
        case AST_BLOCK:         return "Block";
        case AST_CONST_DECL:    return "ConstDecl";
        case AST_TYPE_DECL:     return "TypeDecl";
        case AST_VAR_DECL:      return "VarDecl";
        case AST_PARAM_GROUP:   return "ParamGroup";
        case AST_PROC_DECL:     return "ProcDecl";
        case AST_FUNC_DECL:     return "FuncDecl";
        case AST_TYPE_IDENT:    return "TypeIdent";
        case AST_ARRAY_TYPE:    return "ArrayType";
        case AST_RECORD_TYPE:   return "RecordType";
        case AST_RANGE:         return "Range";
        case AST_ENUMERATED:    return "Enumerated";
        case AST_FIELD_PART:    return "FieldPart";
        case AST_COMPOUND:      return "Compound";
        case AST_ASSIGN:        return "Assign";
        case AST_IF:            return "If";
        case AST_WHILE:         return "While";
        case AST_FOR:           return "For";
        case AST_REPEAT:        return "Repeat";
        case AST_CASE:          return "Case";
        case AST_CASE_BLOCK:    return "CaseBlock";
        case AST_PROC_CALL:     return "ProcCall";
        case AST_EMPTY_STMT:    return "EmptyStmt";
        case AST_BINOP:         return "BinOp";
        case AST_UNOP:          return "UnOp";
        case AST_VAR:           return "Var";
        case AST_ARRAY_ACCESS:  return "ArrayAccess";
        case AST_FIELD_ACCESS:  return "FieldAccess";
        case AST_FUNC_CALL:     return "FuncCall";
        case AST_INT_LIT:       return "IntLit";
        case AST_REAL_LIT:      return "RealLit";
        case AST_CHAR_LIT:      return "CharLit";
        case AST_STRING_LIT:    return "StringLit";
        case AST_BOOL_LIT:      return "BoolLit";
        case AST_IDENT_LIST:    return "IdentList";
        case AST_PARAM_LIST:    return "ParamList";
        case AST_DECL_PART:     return "DeclPart";
        case AST_STMT_LIST:     return "StmtList";
        default:                return "Unknown";
    }
}

static const char *shortTypeName(int typeCode) {
    switch ((BaseType)typeCode) {
        case TYPE_INTEGER: return "integer";
        case TYPE_REAL:    return "real";
        case TYPE_BOOLEAN: return "boolean";
        case TYPE_CHAR:    return "char";
        case TYPE_STRING:  return "string";
        case TYPE_ARRAY:   return "array";
        case TYPE_RECORD:  return "record";
        case TYPE_VOID:    return "void";
        default:           return "none";
    }
}

static void printDecoratedNode(const AstNode *node, FILE *stream) {
    fprintf(stream, "%s", decoratedTypeName(node->type));

    if (node->sval != NULL) {
        fprintf(stream, "('%s')", node->sval);
    } else if (node->type == AST_INT_LIT) {
        fprintf(stream, "(%lld)", node->ival);
    } else if (node->type == AST_REAL_LIT) {
        fprintf(stream, "(%g)", node->rval);
    }

    if (node->typeIdx >= 0 || node->tabIdx >= 0 || node->lexLevel >= 0) {
        int first = 1;
        fprintf(stream, "  -> ");
        if (node->typeIdx >= 0) {
            fprintf(stream, "type:%s", shortTypeName(node->typeIdx));
            first = 0;
        }
        if (node->tabIdx >= 0) {
            if (!first) fprintf(stream, ", ");
            fprintf(stream, "tab_index:%d", node->tabIdx);
            first = 0;
        }
        if (node->lexLevel >= 0) {
            if (!first) fprintf(stream, ", ");
            fprintf(stream, "lev:%d", node->lexLevel);
        }
    }

    fprintf(stream, "\n");
}

static void printDecoratedAstRec(const AstNode *node, FILE *stream,
                                 int depth, int isLast, const char *prefix) {
    char nextPrefix[1024];

    if (node == NULL) {
        return;
    }

    if (depth == 0) {
        printDecoratedNode(node, stream);
        prefix = "";
    } else {
        fprintf(stream, "%s%s", prefix, isLast ? "`-- " : "|-- ");
        printDecoratedNode(node, stream);
    }

    if ((size_t)snprintf(nextPrefix, sizeof(nextPrefix), "%s%s",
                         prefix, isLast ? "    " : "|   ") >= sizeof(nextPrefix)) {
        return;
    }

    for (size_t i = 0; i < node->childCount; i++) {
        printDecoratedAstRec(node->children[i], stream, depth + 1,
                             (i + 1 == node->childCount), nextPrefix);
    }
}

void printDecoratedAst(const AstNode *node, FILE *stream) {
    if (node == NULL || stream == NULL) {
        return;
    }
    printDecoratedAstRec(node, stream, 0, 1, "");
}
