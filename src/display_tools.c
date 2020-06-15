#include <stdlib.h>
#include <stdio.h>

#include "display_tools.h"

#define PROGRESS_BAR_SIZE 80
static int PB_TARGET;
static int PB_STATUS;
static int PB_LENGTH;

void initProgressBar(int size) {
    PB_TARGET = size;
    PB_STATUS = 0;
    PB_LENGTH = 1;
    printf("[>");
    for (int i = 0; i < PROGRESS_BAR_SIZE - 3; ++i) printf(" ");
    printf("]");
    for (int i = 0; i < PROGRESS_BAR_SIZE - 2; ++i) printf("\b");
    fflush(stdout);
}

void updateProgressBar() {
    ++PB_STATUS;
    int length = (PROGRESS_BAR_SIZE - 2) * PB_STATUS / PB_TARGET;
    if (length > PB_LENGTH) {
        printf("\b");
        for (int i = 0; i < length - PB_LENGTH; ++i) printf("=");
        printf(">");
        fflush(stdout);
    }
    PB_LENGTH = length;
    if (PB_STATUS >= PB_TARGET) printf("\n");
}
