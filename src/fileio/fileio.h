#ifndef FILEIO_H
#define FILEIO_H

#include <stdbool.h>
#include <stddef.h>

#define IO_MAX_PATH 512
#define IO_INPUT_DIR "test/milestone-1/input"
#define IO_OUTPUT_DIR "test/milestone-1/output"

bool ensureMilestoneDirectories(void);
bool buildPathFromName(const char *baseDir, const char *rawName, char *path, size_t pathSize);
bool buildInputOutputPaths(const char *rawName, char *inputPath, size_t inputPathSize, char *outputPath, size_t outputPathSize);

#endif