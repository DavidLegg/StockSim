#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "args_parser.h"

#define MAX_CLA_ARGS 64

static struct CommandLineArg CL_ARGS[MAX_CLA_ARGS];
static int NUM_CL_ARGS = 0;

static const char *USAGE   = NULL;
static const char *SUMMARY = NULL;

static const int HELP_INDENT  = 4;
static const int DESCR_INDENT = 4;

void addUsageLine(const char *usage) {
    USAGE = usage;
}

void addSummary(const char *summary) {
    SUMMARY = summary;
}

void addArg(struct CommandLineArg *cla) {
    memcpy(&CL_ARGS[NUM_CL_ARGS++], cla, sizeof(*cla));
}

void parseCommandLineArgs(int argc, char *argv[]) {
    char buf[MAX_CLA_ARGS*3];
    char *p = buf;
    for (int i = 0; i < NUM_CL_ARGS; ++i) {
        *p++ = CL_ARGS[i].shortName;
        if (CL_ARGS[i].type != CLA_FLAG) *p++ = ':';
    }
    *p++ = 'h';
    *p++ = '\0';
    
    int opt;
    while ((opt = getopt(argc, argv, buf)) != -1) {
        if (opt == '?' || opt == 'h') {
            printHelpMessage();
            exit(0);
        }
        for (int i = 0; i < NUM_CL_ARGS; ++i) {
            if (opt == CL_ARGS[i].shortName) {
                switch (CL_ARGS[i].type) {
                    case CLA_FLAG:
                        *(CL_ARGS[i].valuePtr.iptr) ^= 1; // toggle bool value at valuePtr
                        break;
                    case CLA_INT:
                        sscanf(optarg, "%d", CL_ARGS[i].valuePtr.iptr);
                        break;
                    case CLA_LONG:
                        sscanf(optarg, "%ld", CL_ARGS[i].valuePtr.lptr);
                        break;
                    case CLA_FLOAT:
                        sscanf(optarg, "%f", CL_ARGS[i].valuePtr.fptr);
                        break;
                    case CLA_DOUBLE:
                        sscanf(optarg, "%lf", CL_ARGS[i].valuePtr.dptr);
                        break;
                    default:
                        fprintf(stderr, "Unknown CLA type\n");
                        exit(1);
                }
                break;
            }
        }
    }
}

void printHelpMessage() {
    if (USAGE) printf("Usage: %s\n", USAGE);
    if (SUMMARY) {
        const char *start = SUMMARY;
        const char *end;
        while ((end = strchr(start, '\n'))) {
            printf("%*s%.*s\n", HELP_INDENT, "", (int)(end - start), start);
            start = ++end;
        }
        printf("%*s%s\n", HELP_INDENT, "", start);
    }
    if (USAGE || SUMMARY) {
        printf("\n");
    }
    printf("Options:\n");
    for (int i = 0; i < NUM_CL_ARGS; ++i) {
        printf("%*s-%c %c%*s%s\n",
            HELP_INDENT, "",
            CL_ARGS[i].shortName,
            (CL_ARGS[i].parameter ? CL_ARGS[i].parameter : ' '),
            DESCR_INDENT, "",
            CL_ARGS[i].description
            );
    }
    printf("%*s-h  %*sPrint this help message and exit\n",
        HELP_INDENT, "",
        DESCR_INDENT, "");
}
