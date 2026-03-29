#include "fileio.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static bool normalizeName(const char *rawName, char *normalized, size_t normalizedSize) {
    int written;

    if (rawName==NULL || rawName[0]=='\0') {
        return false;
    }

    if (hasTxtExtension(rawName)) {
        written=snprintf(normalized, normalizedSize, "%s", rawName);
    }
    else {
        written=snprintf(normalized, normalizedSize, "%s.txt", rawName);
    }

    return written >= 0 && (size_t)written < normalizedSize;
}

bool ensureMilestoneDirectories(void) {
    return true;
}

bool buildPathFromName(const char *baseDir, const char *rawName, char *path, size_t pathSize) {
    char normalized[IO_MAX_PATH];
    int written;

    if (baseDir==NULL || path==NULL) {
        return false;
    }

    if (!normalizeName(rawName, normalized, sizeof(normalized))) {
        return false;
    }

    written=snprintf(path, pathSize, "%s/%s", baseDir, normalized);
    return written >= 0 && (size_t)written < pathSize;
}

bool buildInputOutputPaths(const char *rawName, char *inputPath, size_t inputPathSize, char *outputPath, size_t outputPathSize) {
    return buildPathFromName(IO_INPUT_DIR, rawName, inputPath, inputPathSize) &&
           buildPathFromName(IO_OUTPUT_DIR, rawName, outputPath, outputPathSize);
}