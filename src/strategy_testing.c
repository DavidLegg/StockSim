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
    const struct DataCollectionSystem *dcs;
};
void *randomizedStartCollectResults(void *args) {
    struct randomizedStartArgs *rsArgs = (struct randomizedStartArgs *)args;
    while (rsArgs->n > 0) {
        rsArgs->dcs->collect(getJobResult());
        --(rsArgs->n);
    }
    return NULL;
}
void *randomizedStart(struct SimState *baseScenario, int n, time_t minStart, time_t maxStart, const struct DataCollectionSystem *dcs, void **resultsEnd) {
    initJobQueue();

    struct randomizedStartArgs rsArgs;
    rsArgs.n = n;
    rsArgs.dcs = dcs;
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

    void *tempOutEnd;
    void *tempOut = dcs->results(&tempOutEnd);
    int sz = (char*)tempOutEnd - (char*)tempOut;
    void *output = malloc(sz);
    memcpy(output, tempOut, sz);
    dcs->reset();
    *resultsEnd = (char*)output + sz;

    return output;
}

void **randomizedStartComparison(struct SimState *baseScenarios, int numScenarios, int n, time_t minStart, time_t maxStart, const struct DataCollectionSystem *dcs, void ***resultEnds) {
    time_t startTimes[n];
    for (int i = 0; i < n; ++i) {
        startTimes[i] = minStart + (time_t)( tsRand() * (double)(maxStart - minStart) / RAND_MAX );
    }
    void **output     = malloc(sizeof(void *) * numScenarios);
    void **outputEnds = malloc(sizeof(void *) * numScenarios);

    initJobQueue();

    pthread_t resultThread;
    struct randomizedStartArgs *rsArgs = malloc(sizeof(*rsArgs));
    rsArgs->dcs = dcs;
    struct SimState *scenarios = malloc(sizeof(*scenarios) * n);
    // This memset should be unnecessary; the scenarios in use
    //   should be init'd by the copySimState call below
    memset(scenarios, 0, sizeof(*scenarios) * n);
    void *tempOut, *tempOutEnd;
    int sz;
    for (int j = 0; j < numScenarios; ++j) {
        // Set up results collection
        rsArgs->n = n;
        pthread_create(&resultThread, NULL, randomizedStartCollectResults, rsArgs);
        // Submit all jobs, using baseScenarios[j] and startTimes
        for (int i = 0; i < n; ++i) {
            copySimState(scenarios + i, baseScenarios + j);
            scenarios[i].time = startTimes[i];
            addJob(scenarios + i);
        }
        // Allocate array for results, store in output[j]
        pthread_join(resultThread, NULL);
        tempOut = dcs->results(&tempOutEnd);
        sz = (char*)tempOutEnd - (char*)tempOut;
        output[j] = malloc(sz);
        memcpy(output[j], tempOut, sz);
        outputEnds[j] = (char*)output[j] + sz;
        dcs->reset();
    }

    free(rsArgs);
    free(scenarios);
    *resultEnds = outputEnds;
    return output;
}

long *randomizedStartDelta(struct SimState *baselineScenario, struct SimState *changeScenario, int n, time_t minStart, time_t maxStart, const struct DataCollectionSystem *dcs, long **resultsEnd) {
    struct SimState scenarios[2];
    copySimState(scenarios, baselineScenario);
    copySimState(scenarios + 1, changeScenario);

    long **tempResults, **tempResultsEnd;
    tempResults = (long**)randomizedStartComparison(scenarios, 2, n, minStart, maxStart, dcs, (void***)&tempResultsEnd);

    long *output = malloc(sizeof(long) * n);

    for (int i = 0; i < n; ++i) {
        output[i] = tempResults[1][i] - tempResults[0][i];
    }
    free(tempResults[0]);
    free(tempResults[1]);
    free(tempResults);

    *resultsEnd = output + n;
    return output;
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
long *finalCashResults(long **end) {
    *end = finalCashAcc + finalCashAccUsed;
    return finalCashAcc;
}
void resetFinalCashCollector(void) {
    finalCashAccUsed = 0;
    finalCashAccCap  = 16;
    finalCashAcc = realloc(finalCashAcc, finalCashAccCap * sizeof(*finalCashAcc));
}

const struct DataCollectionSystem FinalCashDCS = {collectFinalCash, (void *(*)(void **)) finalCashResults, resetFinalCashCollector};
