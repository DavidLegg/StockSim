#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "batch_execution.h"
#include "rng.h"

#include "strategy_testing.h"

/**
 * Testing
 */

struct randomizedStartArgs {
    int n;
    void (*dataProcessor)(struct SimState *);
};
void *randomizedStartCollectResults(void *args) {
    struct randomizedStartArgs *rsArgs = (struct randomizedStartArgs *)args;
    while (rsArgs->n > 0) {
        rsArgs->dataProcessor(getJobResult());
        --rsArgs->n;
    }
    return NULL;
}
void randomizedStart(struct SimState *baseScenario, int n, time_t minStart, time_t maxStart, void (*dataProcessor)(struct SimState *)) {
    initJobQueue();

    struct randomizedStartArgs rsArgs;
    rsArgs.n = n;
    rsArgs.dataProcessor = dataProcessor;
    pthread_t resultThread;
    pthread_create(&resultThread, NULL, randomizedStartCollectResults, &rsArgs);

    struct SimState *scenarios = malloc(sizeof(*scenarios) * n);
    // This memset should be unnecessary; the scenarios in use
    //   should be init'd by the copySimState call below
    memset(scenarios, 0, sizeof(*scenarios) * n);
    for (int i = 0; i < n; ++i) {
        copySimState(scenarios + i, baseScenario);
        scenarios[i].time = minStart + (time_t)( tsRand() * (double)(maxStart - minStart) / RAND_MAX );
        addJob(scenarios + i);
    }

    pthread_join(resultThread, NULL);
    free(scenarios);
}

/**
 * Data collection
 */

static long *finalCashAcc   = NULL;
static int finalCashAccCap  = 16;
static int finalCashAccUsed = 0;
void collectFinalCash(struct SimState *state) {
    if (!finalCashAcc) {
        finalCashAcc = malloc(finalCashAccCap * sizeof(*finalCashAcc));
    }
    if (finalCashAccUsed >= finalCashAccCap) {
        finalCashAccCap *= 2;
        finalCashAcc = realloc(finalCashAcc, finalCashAccCap * sizeof(*finalCashAcc));
    }
    finalCashAcc[finalCashAccUsed++] = state->cash;
}
long *finalCashResults(int *n) {
    *n = finalCashAccUsed;
    return finalCashAcc;
}
void resetFinalCashCollector(void) {
    finalCashAccUsed = 0;
    finalCashAccCap  = 16;
    finalCashAcc = realloc(finalCashAcc, finalCashAccCap * sizeof(*finalCashAcc));
}
