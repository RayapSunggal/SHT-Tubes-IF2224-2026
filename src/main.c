#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileio/fileio.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#define NAME_MAX_LEN 256
#define SYNTAX_INPUT_DIR "test/milestone-2/input"
#define SYNTAX_OUTPUT_DIR "test/milestone-2/output"

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

static bool promptAnalysisMode(int *mode) {
    printf("Pilih mode analisis:\n");
    printf("1. Lexical Analysis\n");
    printf("2. Syntax Analysis\n");
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
        else {
            fprintf(stderr, "Pilihan tidak valid. Gunakan 0, 1, atau 2.\n\n");
        }
    }
}