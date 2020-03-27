#ifndef STRATEGY_TESTING_H
#define STRATEGY_TESTING_H

#include "types.h"

const char *DEFAULT_SYMBOLS_FILE;

/**
 * Runs a grid test of variants of baseScenario.
 * Varies double parameters, using linearly spaced values in a given range
 * Warning! Destroys baseScenario as it goes
 *   params is an array of pointers to memory cells in baseScenario that will be varied
 *   lows & highs are arrays of the low/high values for each param
 *   ns is an array of integers, for the number of values each param will take on
 *   numParams is the number of parameters, the length of the params array, as well as the lows, highs, and ns arrays
 *   numShots is the number of times to run each variation of parameters
 *   paramNames is an array of names, one for each parameter. Used in printing progress messages
 */
void gridTest(struct SimState *baseScenario, double * const *params, const double *lows, const double *highs, const int *ns, int numParams, int numShots, const char **paramNames);
void printGridResults(int *results, const char **paramNames, const double *lows, const double *highs, const int *ns, int offset, int startCash, int k, const char *labelInProgress);

/**
 * Returns an array of n random symbols from the symbols file.
 * If symbolsFile is NULL, then uses DEFAULT_SYMBOLS_FILE
 * If requiredDataStart and/or end are non-zero, only selects symbols
 *   for which there is data starting on or before the required start,
 *   ending on or after the required end
 */
union Symbol *randomSymbols(int n, const char *symbolsFile, time_t requiredDataStart, time_t requiredDataEnd);

#endif // ifndef STRATEGY_TESTING_H