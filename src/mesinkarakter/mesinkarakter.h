#ifndef MESINKARAKTER_H
#define MESINKARAKTER_H

#include <stdbool.h>

extern char CC;
extern bool EOP;

bool START(void);
bool STARTFILE(const char *filename);
void ADV(void);
void CLOSE(void);

#endif