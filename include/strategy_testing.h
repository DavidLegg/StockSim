#ifndef STRATEGY_TESTING_H
#define STRATEGY_TESTING_H

#include "types.h"

/**
 * Testing:
 *   These functions run a scenario, or variations on a scenario,
 *   repeatedly to obtain data about a base strategy.
 */

/**
 * Runs the same baseScenario n times, with randomly chosen starting times in the range specified.
 */
void randomizedStart(struct SimState *baseScenario, int n, time_t minStart, time_t maxStart, void (*dataProcessor)(struct SimState *));

/**
 * Data collection:
 *   These functions collect standard statistics on a strategy,
 *   used in conjunction with the testing routines above.
 */

/**
 * Collects the ending cash for a strategy.
 */
void collectFinalCash(struct SimState *state);
// returns the collected results, outputs number of results to location pointed to by n
long *finalCashResults(int *n);
// resets the accumulator
void resetFinalCashCollector(void);

#endif // ifndef STRATEGY_TESTING_H