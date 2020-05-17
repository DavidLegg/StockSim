#ifndef STRATEGY_TESTING_H
#define STRATEGY_TESTING_H

#include "types.h"

struct RandomizedStartArgs {
    struct SimState *baseScenario;
    const struct DataCollectionSystem *dcs;
    time_t minStart;
    time_t maxStart;
    int n;
};

struct DataCollectionSystem {
    // Collect a data point from the state
    void (*collect)(struct SimState *state);
    // Return the aggregated results
    void *(*results)(void **end);
    // Reset the aggregator
    void (*reset)(void);
};

struct OptimizerMetricSystem {
    struct DataCollectionSystem dcs;
    double (*metric)(void *dataStart, void *dataEnd);
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
void *randomizedStart(struct RandomizedStartArgs *args, void **resultsEnd);
/**
 * Runs each baseScenario for the same collection of randomly chosen starting times.
 * Returns a 2-dimensional ragged array of results, as collected by given DCS. First dimension is scenario, second is start time.
 * Stores a pointer to an array of end-pointers in resultEnds
 */
void **randomizedStartComparison(struct RandomizedStartArgs *args, int numScenarios, void ***resultEnds);
/**
 * Specialization of randomizedStartComparison to 2 scenarios, and a DCS that reports long values.
 * Returns an array of the differences (changeScenario's result - baselineScenario's result) for each run.
 */
long *randomizedStartDelta(struct RandomizedStartArgs *args, struct SimState *changeScenario, long **resultsEnd);

/**
 * Optimization / Parameter Testing
 */

/**
 * Calculates arithmetic mean of dataset.
 */
double meanMetric_l(void *dataStart, void *dataEnd);
/**
 * Calculates standard deviation of dataset
 */
double stddevMetric_l(void *dataStart, void *dataEnd);

/**
 * 
 */
double *grid2Test(struct SimState *(*stateInitFn)(double p1, double p2), double p1Min, double p1Max, double p2Min, double p2Max, int k);

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