#ifndef STRATEGY_TESTING_H
#define STRATEGY_TESTING_H

#include "types.h"

struct DataCollectionSystem {
    // Collect a data point from the state
    void (*collect)(struct SimState *state);
    // Return the aggregated results
    void *(*results)(void **end);
    // Reset the aggregator
    void (*reset)(void);
};

/**
 * Testing:
 *   These functions run a scenario, or variations on a scenario,
 *   repeatedly to obtain data about a base strategy.
 */

/**
 * Runs the same baseScenario n times, with randomly chosen starting times in the range specified.
 * Uses the given DCS, returns start of results and stores end of results into *resultsEnd
 */
void *randomizedStart(struct SimState *baseScenario, int n, time_t minStart, time_t maxStart, const struct DataCollectionSystem *dcs, void **resultsEnd);
/**
 * Runs each baseScenario for the same collection of randomly chosen starting times.
 * Returns a 2-dimensional ragged array of results, as collected by given DCS. First dimension is scenario, second is start time.
 * Stores a pointer to an array of end-pointers in resultEnds
 */
void **randomizedStartComparison(struct SimState *baseScenarios, int numScenarios, int n, time_t minStart, time_t maxStart, const struct DataCollectionSystem *dcs, void ***resultEnds);

/**
 * Data collection:
 *   These functions collect standard statistics on a strategy,
 *   used in conjunction with the testing routines above.
 */

/**
 * Collects the ending cash for a strategy.
 */
void collectFinalCash(struct SimState *state);
long *finalCashResults(long **end);
void resetFinalCashCollector(void);
const struct DataCollectionSystem FinalCashDCS;

#endif // ifndef STRATEGY_TESTING_H