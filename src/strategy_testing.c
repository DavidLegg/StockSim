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

struct rsResultsArgs {
    int n;
    const struct DataCollectionSystem *dcs;
};
void *randomizedStartCollectResults(void *args) {
    struct rsResultsARgs *rsArgs = (struct rsResultsArgs *)args;
    while (rsArgs->n > 0) {
        rsArgs->dcs->collect(getJobResult());
        --(rsArgs->n);
    }
    return NULL;
}
void *randomizedStart(struct RandomizedStartArgs *args, void **resultsEnd) {
    initJobQueue();

    struct rsResultsArgs *resultsArgs = malloc(sizeof(*resultsArgs));
    resultsArgs->n = args->n;
    resultsArgs->dcs = args->dcs;
    pthread_t resultThread;
    pthread_create(&resultThread, NULL, randomizedStartCollectResults, resultsArgs);

    struct SimState *scenarios = malloc(sizeof(*scenarios) * args->n);
    // This memset should be unnecessary; the scenarios in use
    //   should be init'd by the copySimState call below
    memset(scenarios, 0, sizeof(*scenarios) * args->n);
    for (int i = 0; i < args->n; ++i) {
        copySimState(scenarios + i, args->baseScenario);
        scenarios[i].time = args->minStart + (time_t)( tsRand() * (double)(args->maxStart - args->minStart) / RAND_MAX );
        addJob(scenarios + i);
    }

    pthread_join(resultThread, NULL);
    free(scenarios);
    free(resultsArgs);

    void *tempOutEnd;
    void *tempOut = args->dcs->results(&tempOutEnd);
    int sz = (char*)tempOutEnd - (char*)tempOut;
    void *output = malloc(sz);
    memcpy(output, tempOut, sz);
    args->dcs->reset();
    *resultsEnd = (char*)output + sz;

    return output;
}

void **randomizedStartComparison(struct RandomizedStartArgs *args, int numScenarios, void ***resultEnds) {
    time_t startTimes[args->n];
    for (int i = 0; i < args->n; ++i) {
        startTimes[i] = args->minStart + (time_t)( tsRand() * (double)(args->maxStart - args->minStart) / RAND_MAX );
    }
    void **output     = malloc(sizeof(void *) * numScenarios);
    void **outputEnds = malloc(sizeof(void *) * numScenarios);

    initJobQueue();

    pthread_t resultThread;
    struct SimState *scenarios = malloc(sizeof(*scenarios) * args->n);
    // This memset should be unnecessary; the scenarios in use
    //   should be init'd by the copySimState call below
    memset(scenarios, 0, sizeof(*scenarios) * args->n);
    void *tempOut, *tempOutEnd;
    int sz;
    struct rsResultsArgs *resultsArgs = malloc(sizeof(*resultsArgs));
    for (int j = 0; j < numScenarios; ++j) {
        resultsArgs-n    = args->n;
        resultsArgs->dcs = args->dcs;
        // Set up results collection
        pthread_create(&resultThread, NULL, randomizedStartCollectResults, resultsArgs);
        // Submit all jobs, using baseScenarios[j] and startTimes
        for (int i = 0; i < args->n; ++i) {
            copySimState(scenarios + i, args->baseScenario + j);
            scenarios[i].time = startTimes[i];
            addJob(scenarios + i);
        }
        // Allocate array for results, store in output[j]
        pthread_join(resultThread, NULL);
        tempOut = args->dcs->results(&tempOutEnd);
        sz = (char*)tempOutEnd - (char*)tempOut;
        output[j] = malloc(sz);
        memcpy(output[j], tempOut, sz);
        outputEnds[j] = (char*)output[j] + sz;
        args->dcs->reset();
    }

    free(resultsArgs);
    free(scenarios);
    *resultEnds = outputEnds;
    return output;
}

long *randomizedStartDelta(struct RandomizedStartArgs *args, struct SimState *changeScenario, long **resultsEnd) {
    struct SimState scenarios[2];
    copySimState(scenarios, args->baseScenario);
    copySimState(scenarios + 1, changeScenario);
    struct RandomizedStartArgs *newArgs = malloc(sizeof(*newArgs));
    memcpy(newArgs, args, sizeof(*newArgs));
    newArgs->baseScenario = scenarios;

    long **tempResults, **tempResultsEnd;
    tempResults = (long**)randomizedStartComparison(newArgs, 2, (void***)&tempResultsEnd);

    long *output = malloc(sizeof(long) * args->n);

    for (int i = 0; i < args->n; ++i) {
        output[i] = tempResults[1][i] - tempResults[0][i];
    }
    free(newArgs);
    free(tempResults[0]);
    free(tempResults[1]);
    free(tempResults);

    *resultsEnd = output + args->n;
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
