#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileio/fileio.h"
#include "lexer/lexer.h"

#define NAME_MAX_LEN 256

static bool fileExists(const char *path) {
    FILE *fp=fopen(path, "r");
    if (fp==NULL) {
        return false;
    }

    fclose(fp);
    return true;
}

static bool hasTxtExtension(const char *name) {
    size_t len=strlen(name);

    if (len < 4) {
        return false;
    }

    return tolower((unsigned char)name[len - 4])=='.' &&
           tolower((unsigned char)name[len - 3])=='t' &&
           tolower((unsigned char)name[len - 2])=='x' &&
           tolower((unsigned char)name[len - 1])=='t';
}

static void buildMilestonePath(const char *baseDir,
                               const char *name,
                               char *path,
                               size_t pathSize) {
    if (hasTxtExtension(name)) {
        snprintf(path, pathSize, "%s/%s", baseDir, name);
    }
    else {
        snprintf(path, pathSize, "%s/%s.txt", baseDir, name);
    }
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

static void tokenizeRecursive(Lexer *lx, FILE *out) {
    Token tk=getNextToken(lx);

    if (tk.type==TOKEN_EOF) {
        return;
    }

    writeToken(stdout, &tk);
    writeToken(out, &tk);
    tokenizeRecursive(lx, out);
}

int main(void) {
    while (true) {
        Lexer lx;
        FILE *out;
        bool validInput;
        char inputName[NAME_MAX_LEN];
        char outputName[NAME_MAX_LEN];
        char inputPath[IO_MAX_PATH];
        char outputPath[IO_MAX_PATH];

        do {
            validInput=false;
            printf("Masukkan nama file input (ketik 'exit' untuk berhenti): ");
            (void)scanf("%255s", inputName);

            if (strcmp(inputName, "exit")==0) {
                printf("Program dihentikan.\n");
                return EXIT_SUCCESS;
            }

            buildMilestonePath(IO_INPUT_DIR, inputName, inputPath, sizeof(inputPath));
            validInput=fileExists(inputPath);
            if (!validInput) {
                fprintf(stderr, "File input tidak ditemukan: %s\n\n", inputPath);
            }
        } while (!validInput);

        printf("Masukkan nama file output: ");
        (void)scanf("%255s", outputName);

        (void)ensureMilestoneDirectories();
        buildMilestonePath(IO_OUTPUT_DIR, outputName, outputPath, sizeof(outputPath));

        initLexer(&lx, inputPath);
        if (!lx.ready) {
            fprintf(stderr, "Gagal membuka file input: %s\n\n", inputPath);
            continue;
        }

        out=fopen(outputPath, "w");
        if (out==NULL) {
            fprintf(stderr, "Gagal membuat file output: %s\n\n", outputPath);
            closeLexer(&lx);
            continue;
        }

        printf("\nHasil tokenisasi:\n\n");
        tokenizeRecursive(&lx, out);

        fclose(out);
        closeLexer(&lx);
    }
}