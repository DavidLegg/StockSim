#include <stdlib.h>
#include <stdio.h>

#include "display_tools.h"

#define PROGRESS_BAR_SIZE 80
static int PB_DONE;
static int PB_STATUS;

void initProgressBar(int size) {
    PB_DONE   = size;
    PB_STATUS = 0;
    printf("[>");
    for (int i = 0; i < PROGRESS_BAR_SIZE - 3; ++i) printf(" ");
    printf("]");
    for (int i = 0; i < PROGRESS_BAR_SIZE - 1; ++i) printf("\b");
    fflush(stdout);
}

void updateProgressBar() {
    ++PB_STATUS;
    int length = (PROGRESS_BAR_SIZE - 2) * PB_STATUS / PB_DONE;
    for (int i = 0; i < length - 1; ++i) printf("=");
    printf(">");
    for (int i = 0; i < length; ++i) printf("\b");
    if (PB_STATUS >= PB_DONE) printf("\n");
    fflush(stdout);
}
