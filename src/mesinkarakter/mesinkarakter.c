#include "mesinkarakter.h"

#include <stdio.h>

char CC='\0';
bool EOP=true;

static FILE *activeInput=NULL;

static void readCurrentChar(void) {
    int ch;

    if (activeInput==NULL) {
        EOP=true;
        CC='\0';
        return;
    }

    ch=fgetc(activeInput);
    if (ch==EOF) {
        EOP=true;
        CC='\0';
    }
    else {
        EOP=false;
        CC=(char)ch;
    }
}

bool START(void) {
    CLOSE();
    activeInput=stdin;
    readCurrentChar();
    return true;
}

bool STARTFILE(const char *filename) {
    CLOSE();
    activeInput=fopen(filename, "r");
    if (activeInput==NULL) {
        EOP=true;
        CC='\0';
        return false;
    }

    readCurrentChar();
    return true;
}

void ADV(void) {
    if (EOP) {
        return;
    }

    readCurrentChar();
}

void CLOSE(void) {
    if (activeInput!=NULL && activeInput!=stdin) {
        fclose(activeInput);
    }

    activeInput=NULL;
    CC='\0';
    EOP=true;
}