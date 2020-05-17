#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "rng.h"
#include "execution.h"
#include "load_prices.h"
#include "strategies.h"
#include "batch_execution.h"
#include "strategy_testing.h"
#include "stats.h"

static struct Options {
    int textDemo;  // 1 to run text-based demo of test strategy
    int graphDemo; // 1 to graph a demo of test strategy
    int numIters;  // number of random setups
    int numTests;  // number of random starts per setup
} OPTIONS = {
    .textDemo = 0,
    .graphDemo = 0,
    .numIters = 100,
    .numTests = 1000
};

const char HELP_STR[] =
    "Usage: stock-sim [options]\n"
    "    Run a simulation of a stock trading strategy\n"
    "\n"
    "Options:\n"
    "  -I n  Run n iterations, each with randomized starting conditions\n"
    "  -T n  Run n tests on each iteration, at randomized start times\n"
    "  -d    Display text log for an example run, rather than doing a full test\n"
    "  -g    Graph an example run, rather than doing a full test\n"
    "  -h    Display this help message and exit\n"
    ;

void parseArgs(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    parseArgs(argc, argv);

    long startCash    = 10000*DOLLAR;
    time_t testLength = 1*YEAR;
    int numSymbols    = 20;
    int numBins       = 15;
    struct SimState scenarios[2];
    struct SimState *controlState = scenarios, *testState = scenarios + 1;
    long *resultsDelta = malloc(sizeof(long) * OPTIONS.numTests * OPTIONS.numIters);

    unsigned int seed = time(0);
    printf("Seed value: %d\n", seed);
    tsRandInit(seed);
    historicalPriceInit();

    for (int iter = 0; iter < OPTIONS.numIters; ++iter) {
        printf("---=== Iteration %d ===---\n", iter);
        printf("Choosing test time period...\n");
        struct tm structPeriodStart, structPeriodEnd;
        strptime("1/1/1981 00:00", "%m/%d/%Y%n%H:%M", &structPeriodStart);
        structPeriodStart.tm_isdst = -1;
        structPeriodStart.tm_sec = 0;
        strptime("1/1/2000 00:00", "%m/%d/%Y%n%H:%M", &structPeriodEnd);
        structPeriodEnd.tm_isdst = -1;
        structPeriodEnd.tm_sec = 0;

        time_t periodStart = mktime(&structPeriodStart);
        time_t periodEnd   = mktime(&structPeriodEnd);
        time_t testStart   = periodStart + (time_t)( tsRand() * (double)(periodEnd - periodStart) / RAND_MAX );
        time_t testEnd     = testStart + 5*YEAR;

        printf("Choosing assets...\n");
        union Symbol *symbols = randomSymbols(numSymbols, testStart, testEnd + testLength);
        for (int i = 0; i < numSymbols; ++i) {
            printf("%*.*s ", SYMBOL_LENGTH, SYMBOL_LENGTH, symbols[i].name);
        }
        printf("\n");

        printf("Constructing base scenario (control)...\n");
        initSimState(controlState, 0);
        controlState->cash    = startCash;
        controlState->priceFn = getHistoricalPrice;

        // Create a time horizon
        struct TimeHorizonArgs *thArgsControl = (struct TimeHorizonArgs *)makeCustomOrder(controlState, NULL, 0, timeHorizon)->aux;
        thArgsControl->offset = testLength;
        thArgsControl->cutoff = 0;

        // Use a balanced buy, equivalent to start of PortfolioRebalancing
        struct BuyBalancedArgs *bbArgsControl = (struct BuyBalancedArgs *)makeCustomOrder(controlState, NULL, 0, buyBalanced)->aux;
        for (int i = 0; i < numSymbols; ++i) {
            bbArgsControl->assets[i].id = symbols[i].id;
            bbArgsControl->weights[i]   = 1.0 / numSymbols;
        }
        bbArgsControl->totalValue  = startCash;
        bbArgsControl->symbolsUsed = numSymbols;

        printf("Constructing base scenario (test)...\n");
        initSimState(testState, 0);
        testState->cash    = startCash;
        testState->priceFn = getHistoricalPrice;

        // Create a time horizon
        struct TimeHorizonArgs *thArgsTest = (struct TimeHorizonArgs *)makeCustomOrder(testState, NULL, 0, timeHorizon)->aux;
        thArgsTest->offset = testLength;
        thArgsTest->cutoff = 0;

        // Portfolio Rebalancing, the strategy being tested
        struct PortfolioRebalanceArgs *prArgsTest = (struct PortfolioRebalanceArgs *)makeCustomOrder(testState, NULL, 0, portfolioRebalance)->aux;
        for (int i = 0; i < numSymbols; ++i) {
            prArgsTest->assets[i].id = symbols[i].id;
            prArgsTest->weights[i]   = 1.0 / numSymbols;
        }
        prArgsTest->maxAssetValue = -1; // disable allocation capping
        prArgsTest->symbolsUsed   = numSymbols;

        // Cleanup
        free(symbols);
        symbols = NULL;

        // Package various arguments together
        struct RandomizedStartArgs rsArgs;
        rsArgs.baseScenario = controlState;
        rsArgs.dcs          = &FinalCashDCS;
        rsArgs.minStart     = testStart;
        rsArgs.maxStart     = testEnd;
        rsArgs.n            = OPTIONS.numTests;

        long *results, *resultsEnd;
        if (OPTIONS.textDemo) {
            // Do a text demo run of the test strategy and exit
            printf("Running text demo...\n");
            testState->time = testStart;
            historicalPriceAddThread(pthread_self());
            runScenarioDemo(testState, 100);
            return 0;
        } else if (OPTIONS.graphDemo) {
            // Do a graphed demo run of the test strategy and exit
            printf("Running graphing demo...\n");
            testState->time = testStart;
            historicalPriceAddThread(pthread_self());
            graphScenario(testState);
            return 0;
        } else {
            // Execute the test
            printf("Executing test...\n");
            results = randomizedStartDelta(&rsArgs, testState, &resultsEnd);
        }

        // Process results
        memcpy(resultsDelta + OPTIONS.numTests * iter, results, sizeof(long) * OPTIONS.numTests);
        free(results);
        resultsEnd = results = NULL;
        printf("Iteration %d Summary:\n", iter);
        drawHistogram(resultsDelta + OPTIONS.numTests * iter, resultsDelta + OPTIONS.numTests * (iter + 1), numBins, SF_Currency);
    }

    // Display results
    printf("Summary:\n");
    drawHistogram(resultsDelta, resultsDelta + OPTIONS.numTests * OPTIONS.numIters, numBins, SF_Currency);
    printStats(resultsDelta, resultsDelta + OPTIONS.numTests * OPTIONS.numIters, MEAN | STDDEV, SF_Currency);

    return 0;
}

void parseArgs(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "dgT:I:h")) != -1) {
        switch (opt) {
            case 'T':
                sscanf(optarg, "%d", &OPTIONS.numTests);
                break;
            case 'I':
                sscanf(optarg, "%d", &OPTIONS.numIters);
                break;
            case 'd':
                OPTIONS.textDemo = 1;
                break;
            case 'g':
                OPTIONS.graphDemo = 1;
                break;
            case 'h':
            case '?':
                printf("%s", HELP_STR);
                exit(0);
        }
    }

    if (OPTIONS.graphDemo && OPTIONS.textDemo) {
        fprintf(stderr, "Cannot specify text demo and graphing demo. Specify one or the other only.\n");
        exit(1);
    }
}

