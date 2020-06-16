#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

enum CLA_Type {
    CLA_FLAG, // toggles a boolean (int) when flag is seen (doesn't take argument)
    CLA_INT, CLA_LONG, CLA_FLOAT, CLA_DOUBLE, // sets value as given on command line (takes argument)
    CLA_DOLLAR // interprets float-formatted dollar amount on command line, sets long (internal money format)
};

struct CommandLineArg {
    const char *description; // Text description of this option
    char parameter;          // Single character displayed for parameter given to this setting
    char shortName;          // Single character name for command line use
    enum CLA_Type type;      // How to parse argument
    union {
        int*    iptr;
        long*   lptr;
        float*  fptr;
        double* dptr;
    } valuePtr;              // Where to store argument
};

void addUsageLine(const char *usage);
void addSummary(const char *summary);
void addArg(struct CommandLineArg *cla);
void parseCommandLineArgs(int argc, char *argv[]);
void printHelpMessage();

#endif // ifndef ARGS_PARSER_H