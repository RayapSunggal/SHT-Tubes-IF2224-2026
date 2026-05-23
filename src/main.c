#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast/ast_builder.h"
#include "ast/ast_node.h"
#include "fileio/fileio.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/ast_decorator.h"
#include "semantic/symbol_table.h"

#define NAME_MAX_LEN 256
#define SYNTAX_INPUT_DIR  "test/milestone-2/input"
#define SYNTAX_OUTPUT_DIR "test/milestone-2/output"
#define SEMANTIC_INPUT_DIR  "test/milestone-3/input"
#define SEMANTIC_OUTPUT_DIR "test/milestone-3/output"

static bool fileExists(const char *path) {
    FILE *fp=fopen(path, "r");
    if (fp==NULL) {
        return false;
    }

    fclose(fp);
    return true;
}

static void clearInputBuffer(void) {
    int ch;

    do {
        ch=getchar();
    } while (ch!='\n' && ch!=EOF);
}

static bool promptInputPath(char *inputPath, size_t inputPathSize) {
    bool validInput;
    char inputName[NAME_MAX_LEN];

    do {
        validInput=false;
        printf("Masukkan nama file input (ketik 'back' untuk kembali): ");
        if (scanf("%255s", inputName)!=1) {
            clearInputBuffer();
            return false;
        }

        if (strcmp(inputName, "back")==0) {
            return false;
        }

        if (!buildPathFromName(IO_INPUT_DIR, inputName, inputPath, inputPathSize)) {
            fprintf(stderr, "Nama file input tidak valid.\n\n");
            continue;
        }

        validInput=fileExists(inputPath);
        if (!validInput) {
            fprintf(stderr, "File input tidak ditemukan: %s\n\n", inputPath);
        }
    } while (!validInput);

    return true;
}

static bool promptOutputPath(char *outputPath, size_t outputPathSize) {
    char outputName[NAME_MAX_LEN];

    printf("Masukkan nama file output: ");
    if (scanf("%255s", outputName)!=1) {
        clearInputBuffer();
        return false;
    }

    if (!buildPathFromName(IO_OUTPUT_DIR, outputName, outputPath, outputPathSize)) {
        fprintf(stderr, "Nama file output tidak valid.\n\n");
        return false;
    }

    return true;
}

static bool promptSyntaxPaths(char *inputPath, size_t inputPathSize, char *outputPath, size_t outputPathSize) {
    bool validInput;
    char inputName[NAME_MAX_LEN];

    do {
        validInput=false;
        printf("Masukkan nama file input syntax (ketik 'back' untuk kembali): ");
        if (scanf("%255s", inputName)!=1) {
            clearInputBuffer();
            return false;
        }

        if (strcmp(inputName, "back")==0) {
            return false;
        }

        if (!buildPathFromName(SYNTAX_INPUT_DIR, inputName, inputPath, inputPathSize)) {
            fprintf(stderr, "Nama file input tidak valid.\n\n");
            continue;
        }

        validInput=fileExists(inputPath);
        if (!validInput) {
            fprintf(stderr, "File input tidak ditemukan: %s\n\n", inputPath);
        }
    } while (!validInput);

    if (!buildPathFromName(SYNTAX_OUTPUT_DIR, inputName, outputPath, outputPathSize)) {
        fprintf(stderr, "Nama file output tidak valid.\n\n");
        return false;
    }

    return true;
}

static void writeToken(FILE *stream, const Token *tk) {
    if (tk->type==TOKEN_STRING) {
        fprintf(stream, "%s ('%s')\n", tokenTypeToString(tk->type), tk->lexeme);
        return;
    }

    switch (tk->type) {
        case TOKEN_INTCON:
        case TOKEN_REALCON:
        case TOKEN_CHARCON:
        case TOKEN_IDENT:
        case TOKEN_COMMENT:
        case TOKEN_UNKNOWN:
            fprintf(stream, "%s (%s)\n", tokenTypeToString(tk->type), tk->lexeme);
            return;
        default:
            fprintf(stream, "%s\n", tokenTypeToString(tk->type));
            return;
    }
}

static void tokenize(Lexer *lx, FILE *out) {
    while (true) {
        Token tk=getToken(lx);

        if (tk.type==TOKEN_EOF) {
            return;
        }

        writeToken(stdout, &tk);
        writeToken(out, &tk);
    }
}

static void runLexicalAnalysis(void) {
    Lexer lx;
    FILE *out;
    char inputPath[IO_MAX_PATH];
    char outputPath[IO_MAX_PATH];

    if (!promptInputPath(inputPath, sizeof(inputPath))) {
        return;
    }

    if (!promptOutputPath(outputPath, sizeof(outputPath))) {
        return;
    }

    (void)ensureMilestoneDirectories();

    initLexer(&lx, inputPath);
    if (!lx.ready) {
        fprintf(stderr, "Gagal membuka file input: %s\n\n", inputPath);
        return;
    }

    out=fopen(outputPath, "w");
    if (out==NULL) {
        fprintf(stderr, "Gagal membuat file output: %s\n\n", outputPath);
        closeLexer(&lx);
        return;
    }

    printf("\nHasil lexical analysis:\n\n");
    tokenize(&lx, out);
    printf("\nOutput tersimpan di: %s\n\n", outputPath);

    fclose(out);
    closeLexer(&lx);
}

static void runSyntaxAnalysis(void) {
    char inputPath[IO_MAX_PATH];
    char outputPath[IO_MAX_PATH];
    FILE *out;
    SyntaxResult syntaxResult;

    if (!promptSyntaxPaths(inputPath, sizeof(inputPath), outputPath, sizeof(outputPath))) {
        return;
    }

    if (analyzeSyntaxFile(inputPath, &syntaxResult)) {
        printf("\nSyntax analysis success.\n");
        printf("%s\n\n", syntaxResult.message);
        printf("Parse tree:\n");
        parseTreePrint(syntaxResult.tree, stdout);
        printf("\n");

        out=fopen(outputPath, "w");
        if (out==NULL) {
            fprintf(stderr, "Gagal membuat file output: %s\n\n", outputPath);
            freeSyntaxResult(&syntaxResult);
            return;
        }

        parseTreePrint(syntaxResult.tree, out);
        fprintf(out, "\n");
        fclose(out);
        printf("Output parse tree tersimpan di: %s\n\n", outputPath);

        freeSyntaxResult(&syntaxResult);
        return;
    }

    fprintf(stderr, "\nSyntax analysis failed.\n");
    fprintf(stderr, "%s\n\n", syntaxResult.message);
    freeSyntaxResult(&syntaxResult);
}

static bool promptSemanticPaths(char *inputPath, size_t inputPathSize,
                                char *outputPath, size_t outputPathSize) {
    bool validInput;
    char inputName[NAME_MAX_LEN];

    do {
        validInput = false;
        printf("Masukkan nama file source semantic (ketik 'back' untuk kembali): ");
        if (scanf("%255s", inputName) != 1) {
            clearInputBuffer();
            return false;
        }

        if (strcmp(inputName, "back") == 0) {
            return false;
        }

        if (!buildPathFromName(SEMANTIC_INPUT_DIR, inputName, inputPath, inputPathSize)) {
            fprintf(stderr, "Nama file input tidak valid.\n\n");
            continue;
        }

        validInput = fileExists(inputPath);

        if (!validInput) {
            fprintf(stderr, "File input tidak ditemukan: %s\n\n", inputPath);
        }
    } while (!validInput);

    if (!buildPathFromName(SEMANTIC_OUTPUT_DIR, inputName, outputPath, outputPathSize)) {
        fprintf(stderr, "Nama file output tidak valid.\n\n");
        return false;
    }

    return true;
}

static void printSymTableToFile(FILE *out) {
    int tabN = symTabCount();
    int btabN = symBtabCount();
    int atabN = symAtabCount();

    fprintf(out, "TAB:\n");
    fprintf(out, " idx | identifier | link | obj | type | ref | nrm | lev | adr\n");
    fprintf(out, "-----+------------+------+------+------+-----+-----+-----+-----\n");
    for (int i = 0; i < tabN; i++) {
        if (tab[i].identifier == NULL) continue;
        fprintf(out, " %3d | %10s | %4d | %s | %s | %3d | %3d | %3d | %3d\n",
                i, tab[i].identifier, tab[i].link < 0 ? 0 : tab[i].link,
                objClassToString(tab[i].obj), baseTypeToString(tab[i].type),
                tab[i].ref < 0 ? 0 : tab[i].ref, tab[i].nrm, tab[i].lev, tab[i].adr);
    }

    fprintf(out, "\nBTAB:\n");
    fprintf(out, " idx | last | lpar | psze | vsze\n");
    fprintf(out, "-----+------+------+------+------\n");
    for (int i = 0; i < btabN; i++) {
        fprintf(out, " %3d | %4d | %4d | %4d | %4d\n",
                i,
                btab[i].last < 0 ? 0 : btab[i].last,
                btab[i].lpar < 0 ? 0 : btab[i].lpar,
                btab[i].psze,
                btab[i].vsze);
    }

    fprintf(out, "\nATAB:\n");
    fprintf(out, " idx | xtyp | etyp | eref | low | high | elsz | size\n");
    fprintf(out, "-----+------+------+------+-----+------+------+------\n");
    if (atabN == 0) {
        fprintf(out, " (kosong - tidak ada array)\n");
    }
    for (int i = 0; i < atabN; i++) {
        fprintf(out, " %3d | %5s | %5s | %4d | %3d | %4d | %4d | %4d\n",
                i, baseTypeToString(atab[i].xtyp), baseTypeToString(atab[i].etyp),
                atab[i].eref < 0 ? 0 : atab[i].eref,
                atab[i].low, atab[i].high, atab[i].elsz, atab[i].size);
    }

    fprintf(out, "\ncurrentLevel = %d\n", currentLevel);
    for (int i = 0; i <= currentLevel; i++) {
        fprintf(out, "display[%d] = %d\n", i, display[i]);
    }
}

static void writeSemanticReport(FILE *stream, bool semOk, const char *semMessage,
                                const AstNode *decoratedAst) {
    if (!semOk) {
        fprintf(stream, "Semantic error: %s\n", semMessage);
        fprintf(stream, "\n");
    }

    fprintf(stream, "=== Decorated AST ===\n");
    if (decoratedAst != NULL) {
        printDecoratedAst(decoratedAst, stream);
    } else {
        fprintf(stream, "(AST tidak dapat dibentuk dari parse tree)\n");
    }

    fprintf(stream, "\n=== Symbol Table ===\n");
    printSymTableToFile(stream);
}

static void runSemanticAnalysis(void) {
    char inputPath[IO_MAX_PATH];
    char outputPath[IO_MAX_PATH];
    SyntaxResult syntaxResult;
    char semMessage[2048];
    char astMessage[2048];
    AstNode *ast;
    bool decorateOk;
    bool semOk;
    FILE *out;

    if (!promptSemanticPaths(inputPath, sizeof(inputPath),
                             outputPath, sizeof(outputPath))) {
        return;
    }

    (void)ensureMilestoneDirectories();

    if (!analyzeSyntaxFile(inputPath, &syntaxResult)) {
        fprintf(stderr, "\nSyntax analysis gagal:\n%s\n\n", syntaxResult.message);
        freeSyntaxResult(&syntaxResult);
        return;
    }

    ast = buildAst(syntaxResult.tree);
    freeSyntaxResult(&syntaxResult);

    if (ast == NULL) {
        printf("\nGagal membangun AST dari parse tree.\n");
        semOk = false;
        snprintf(semMessage, sizeof(semMessage),
                 "Gagal membangun AST dari parse tree.");
    } else {
        symInit();
        astMessage[0] = '\0';
        decorateOk = decorateAst(ast, astMessage, sizeof(astMessage));
        semOk = decorateOk;
        if (!decorateOk) {
            snprintf(semMessage, sizeof(semMessage), "%s", astMessage);
        }
    }

    if (semOk) {
        printf("\nSemantic analysis berhasil.\n");
    } else {
        printf("\nSemantic error: %s\n", semMessage);
    }

    writeSemanticReport(stdout, semOk, semMessage, ast);

    out = fopen(outputPath, "w");
    if (out == NULL) {
        fprintf(stderr, "Gagal membuat file output: %s\n\n", outputPath);
    } else {
        writeSemanticReport(out, semOk, semMessage, ast);
        fclose(out);
        printf("\nOutput tersimpan di: %s\n\n", outputPath);
    }

    astFree(ast);
    printf("\n");
}

static bool promptAnalysisMode(int *mode) {
    printf("Pilih mode analisis:\n");
    printf("1. Lexical Analysis\n");
    printf("2. Syntax Analysis\n");
    printf("3. Semantic Analysis\n");
    printf("0. Exit\n");
    printf("Masukkan pilihan: ");

    if (scanf("%d", mode)!=1) {
        fprintf(stderr, "Input pilihan harus berupa angka.\n\n");
        clearInputBuffer();
        return false;
    }

    return true;
}

int main(void) {
    int mode;

    while (true) {
        if (!promptAnalysisMode(&mode)) {
            continue;
        }

        if (mode==0) {
            printf("Program dihentikan.\n");
            return EXIT_SUCCESS;
        }

        if (mode==1) {
            runLexicalAnalysis();
        }
        else if (mode==2) {
            runSyntaxAnalysis();
        }
        else if (mode==3) {
            runSemanticAnalysis();
        }
        else {
            fprintf(stderr, "Pilihan tidak valid. Gunakan 0, 1, 2, atau 3.\n\n");
        }
    }
}
