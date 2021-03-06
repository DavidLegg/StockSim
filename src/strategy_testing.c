#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define __USE_XOPEN
#include <time.h>

#include "batch_execution.h"
#include "rng.h"
#include "display_tools.h"

#include "strategy_testing.h"

#define PG2_LABEL_WIDTH 14
#define PG2_DISPLAY_WIDTH 90

/**
 * Testing
 */

struct rsResultsArgs {
    int n;
    const struct DataCollectionSystem *dcs;
};
void *randomizedStartCollectResults(void *args) {
    struct rsResultsArgs *rsArgs = (struct rsResultsArgs *)args;
    while (rsArgs->n > 0) {
        getJobResult(rsArgs->dcs->collect);
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

    struct SimState *state = malloc(sizeof(*state));
    copySimState(state, args->baseScenario);
    for (int i = 0; i < args->n; ++i) {
        state->time = args->minStart + (time_t)( tsRand() * (double)(args->maxStart - args->minStart) / RAND_MAX );
        addJob(state);
    }

    pthread_join(resultThread, NULL);
    free(state);
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
    struct SimState *state = malloc(sizeof(*state));
    void *tempOut, *tempOutEnd;
    int sz;
    struct rsResultsArgs *resultsArgs = malloc(sizeof(*resultsArgs));
    for (int j = 0; j < numScenarios; ++j) {
        resultsArgs->n   = args->n;
        resultsArgs->dcs = args->dcs;
        // Set up results collection
        pthread_create(&resultThread, NULL, randomizedStartCollectResults, resultsArgs);
        // Submit all jobs, using baseScenarios[j] and startTimes
        copySimState(state, args->baseScenario + j);
        for (int i = 0; i < args->n; ++i) {
            state->time = startTimes[i];
            addJob(state);
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
    free(state);
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
 * Optimization / Parameter Testing
 */

double meanMetric_l(void *dataStart, void *dataEnd) {
    long *start = (long*)dataStart;
    long *end   = (long*)dataEnd;

    // Iterative mean algorithm, as given by Heiko Hoffman at http://www.heikohoffmann.de/htmlthesis/node134.html
    // Numerically stable, and avoids the overflow issue of total-then-divide
    double mean = 0.0;
    int t = 1;
    for (long *p = start; p < end; ++p, ++t) {
        mean += (*p - mean) / t;
    }
    return mean;
}

double stddevMetric_l(void *dataStart, void *dataEnd) {
    double mean = meanMetric_l(dataStart, dataEnd);
    long *start = (long*)dataStart;
    long *end   = (long*)dataEnd;

    // Iterative mean algorithm, as given by Heiko Hoffman at http://www.heikohoffmann.de/htmlthesis/node134.html
    // Numerically stable, and avoids the overflow issue of total-then-divide
    double meanSquares = 0.0;
    int t = 1;
    for (long *p = start; p < end; ++p, ++t) {
        meanSquares += (((*p - mean) * (*p - mean)) - meanSquares) / t;
    }
    return sqrt(meanSquares);
}

double *grid2Test(struct SimState *(*stateInitFn)(double p1, double p2), const struct OptimizerMetricSystem *metric, double p1Min, double p1Max, double p2Min, double p2Max, int divisions1, int divisions2) {
    double *summaries = malloc(sizeof(*summaries) * divisions1 * divisions2);
    initProgressBar(divisions1 * divisions2);
    for (int k1 = 0; k1 < divisions1; ++k1) {
        for (int k2 = 0; k2 < divisions2; ++k2) {
            struct SimState *state = stateInitFn(
                (divisions1 > 1 ? p1Min + (p1Max - p1Min) * k1 / (divisions1 - 1) : p1Min),
                (divisions2 > 1 ? p2Min + (p2Max - p2Min) * k2 / (divisions2 - 1) : p2Min));
            metric->rsArgs->baseScenario = state;
            void *resultsEnd;
            void *results = randomizedStart(metric->rsArgs, &resultsEnd);
            summaries[k2 + k1*divisions2] = metric->metric(results, resultsEnd);
            free(state);
            updateProgressBar();
        }
    }
    return summaries;
}

void displayGrid2(double *results, double p1Min, double p1Max, double p2Min, double p2Max, int divisions1, int divisions2, const char *p1Name, const char *p2Name) {
    char buf[PG2_DISPLAY_WIDTH];
    snprintf(buf, PG2_LABEL_WIDTH, "%.2f", (p2Min + p2Max) / 2);
    int tempLen = strlen(buf);
    int i;

    int cellWidth = (PG2_DISPLAY_WIDTH - PG2_LABEL_WIDTH) / divisions2;
    double mn, mx;
    mn = mx = *results;
    for (i = 1; i < divisions1 * divisions2; ++i) {
        if (results[i] < mn) mn = results[i];
        else if (results[i] > mx) mx = results[i];
    }

    // Print header rows
    printf("%s vs. %s\n  Min: %*.2f\n  Max: %*.2f\n%*s%s\n",
        p1Name, p2Name,
        PG2_LABEL_WIDTH, mn, PG2_LABEL_WIDTH, mx,
        PG2_LABEL_WIDTH, "", p2Name);
    printf("%*s%-*.2f%s%*.2f\n",
        PG2_LABEL_WIDTH, "",
        (PG2_DISPLAY_WIDTH - PG2_LABEL_WIDTH - tempLen) / 2, p2Min,
        buf,
        (divisions2*cellWidth) - ((PG2_DISPLAY_WIDTH - PG2_LABEL_WIDTH - tempLen) / 2), p2Max);
    for (i = 0; i < cellWidth; ++i) buf[i] = '-';
    buf[i] = '\0';
    printf("%-*s", PG2_LABEL_WIDTH, p1Name);
    for (i = 0; i < divisions2; ++i) printf("+%s", buf);
    printf("+\n");

    // Print data rows
    for (int rowNum = 0; rowNum < divisions1; ++rowNum) {
        // Print label
        if (rowNum == 0) {
            printf("%*.2f ", PG2_LABEL_WIDTH - 1, p1Min);
        } else if (rowNum == divisions1 - 1) {
            printf("%*.2f ", PG2_LABEL_WIDTH - 1, p1Max);
        } else if (rowNum == divisions1 / 2) {
            printf("%*.2f ", PG2_LABEL_WIDTH - 1, (p1Min + p1Max) / 2);
        } else {
            printf("%*s", PG2_LABEL_WIDTH, "");
        }

        for (int colNum = 0; colNum < divisions2; ++colNum) {
            tempLen = (int) ( round( cellWidth * (results[colNum + divisions2*rowNum] - mn) / (mx - mn) ) );
            printf("|");
            for (i = 0; i < tempLen - 1; ++i) printf("=");
            printf(">%*s", cellWidth - i - 1, "");
        }
        printf("|\n%*s", PG2_LABEL_WIDTH, "");
        for (i = 0; i < divisions2; ++i) printf("+%s", buf);
        printf("+\n");
    }
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
