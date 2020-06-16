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
#include "args_parser.h"

static struct Options {
    int textDemo;  // 1 to run text-based demo of test strategy
    int graphDemo; // 1 to graph a demo of test strategy
    int numIters;  // number of random setups
    int numTests;  // number of random starts per setup
    int numBins;   // number of bins on histogram
    int numSymbols;
    long startCash;
    time_t testLength;
    time_t periodStart;
    time_t periodEnd;
    double param1Min;
    double param1Max;
    long   param1MinInt;
    long   param1MaxInt;
    double param2Min;
    double param2Max;
    int    param2MinInt;
    int    param2MaxInt;
    double epsilon;
    int divisions1;
    int divisions2;
} OPTIONS;

const char param1Name[] = "Target Price";
const char param2Name[] = "Portfolio Size";
struct SimState BASE_STATE;
struct RandomizedStartArgs rsArgs;

void parseArgs(int argc, char *argv[]);
struct SimState *stateInit(double p1, double p2);

int main(int argc, char *argv[]) {
    parseArgs(argc, argv);

    unsigned int seed = time(0);
    printf("Seed value: %d\n", seed);
    tsRandInit(seed);
    historicalPriceInit();

    rsArgs.baseScenario = &BASE_STATE;
    rsArgs.dcs          = &FinalCashDCS;
    rsArgs.n            = OPTIONS.numTests;
    rsArgs.minStart     = OPTIONS.periodStart;
    rsArgs.maxStart     = OPTIONS.periodEnd;

    initSimState(&BASE_STATE, 0);
    BASE_STATE.priceFn = getHistoricalPrice;
    BASE_STATE.cash    = OPTIONS.startCash;

    // Create a time horizon
    union Symbol thSymbol;
    strncpy(thSymbol.name, "HRZN", SYMBOL_LENGTH);
    struct TimeHorizonArgs *thArgsControl = (struct TimeHorizonArgs *)makeCustomOrder(&BASE_STATE, &thSymbol, 1, timeHorizon)->aux;
    thArgsControl->offset = OPTIONS.testLength;
    thArgsControl->cutoff = 0;

    struct OptimizerMetricSystem oms;
    oms.rsArgs = &rsArgs;
    oms.metric = meanMetric_l;

    if (OPTIONS.textDemo) {
        // Do a text demo run of the test strategy and exit
        printf("Running text demo...\n");
        struct SimState *state = stateInit(
                (OPTIONS.param1Min + OPTIONS.param1Max) / 2,
                (OPTIONS.param2Min + OPTIONS.param2Max) / 2
            );
        state->time = OPTIONS.periodStart + (time_t)( tsRand() * (double)(OPTIONS.periodEnd - OPTIONS.periodStart) / RAND_MAX );
        historicalPriceAddThread(pthread_self());
        runScenarioDemo(state, 100);
        free(state);
        return 0;
    } else if (OPTIONS.graphDemo) {
        // Do a graphed demo run of the test strategy and exit
        printf("Running graphing demo...\n");
        double p1 = (OPTIONS.param1Min + OPTIONS.param1Max) / 2;
        double p2 = (OPTIONS.param2Min + OPTIONS.param2Max) / 2;
        printf("  %s: %.2lf\n  %s: %.2lf\n", param1Name, p1, param2Name, p2);
        struct SimState *state = stateInit(p1, p2);
        state->time = OPTIONS.periodStart + (time_t)( tsRand() * (double)(OPTIONS.periodEnd - OPTIONS.periodStart) / RAND_MAX );
        historicalPriceAddThread(pthread_self());
        graphScenario(state);
        free(state);
        return 0;
    } else {
        // Execute the test
        printf("Executing test...\n");
        double *results = grid2Test(
                stateInit, &oms,
                OPTIONS.param1Min,
                OPTIONS.param1Max,
                OPTIONS.param2Min,
                OPTIONS.param2Max,
                OPTIONS.divisions1,
                OPTIONS.divisions2
            );
        displayGrid2(
                results,
                OPTIONS.param1Min,
                OPTIONS.param1Max,
                OPTIONS.param2Min,
                OPTIONS.param2Max,
                OPTIONS.divisions1,
                OPTIONS.divisions2,
                param1Name,
                param2Name
            );
        free(results);
    }

    return 0;
}

void initOptions(void) {
    OPTIONS.textDemo         = 0;
    OPTIONS.graphDemo        = 0;
    OPTIONS.numIters         = 100;
    OPTIONS.numTests         = 1000;
    OPTIONS.numBins          = 15;
    OPTIONS.numSymbols       = 20;
    OPTIONS.startCash        = 10000*DOLLAR;
    OPTIONS.testLength       = 1*YEAR;
    OPTIONS.param1Min        = 0;
    OPTIONS.param1Max        = 0;
    OPTIONS.param1MinInt     = 0;
    OPTIONS.param1MaxInt     = 0;
    OPTIONS.param2Min        = 0;
    OPTIONS.param2Max        = 0;
    OPTIONS.param2MinInt     = 0;
    OPTIONS.param2MaxInt     = 0;
    OPTIONS.epsilon          = 0.1;
    OPTIONS.divisions1       = 5;
    OPTIONS.divisions2       = 5;

    struct tm structPeriodStart, structPeriodEnd;
    strptime("1/1/1981 00:00", "%m/%d/%Y%n%H:%M", &structPeriodStart);
    structPeriodStart.tm_isdst = -1;
    structPeriodStart.tm_sec   = 0;
    strptime("1/1/2000 00:00", "%m/%d/%Y%n%H:%M", &structPeriodEnd);
    structPeriodEnd.tm_isdst = -1;
    structPeriodEnd.tm_sec   = 0;

    OPTIONS.periodStart = mktime(&structPeriodStart);
    OPTIONS.periodEnd   = mktime(&structPeriodEnd);
}

void initParser(void) {
    struct CommandLineArg cla;

    addUsageLine("stock-sim [options]");
    addSummary("Run a simulation of a stock trading strategy\n"
        "In particular, run a grid-test of target price vs. number of assets in portfolio.");
    
    cla.description   = "Set minimum target price of x";
    cla.parameter     = 'x';
    cla.shortName     = 'u';
    cla.type          = CLA_DOLLAR;
    cla.valuePtr.lptr = &OPTIONS.param1MinInt;
    addArg(&cla);

    cla.description   = "Set maximum target price of x";
    cla.parameter     = 'x';
    cla.shortName     = 'v';
    cla.type          = CLA_DOLLAR;
    cla.valuePtr.lptr = &OPTIONS.param1MaxInt;
    addArg(&cla);

    cla.description   = "Set tolerance factor on price selection to x";
    cla.parameter     = 'x';
    cla.shortName     = 'e';
    cla.type          = CLA_DOUBLE;
    cla.valuePtr.dptr = &OPTIONS.epsilon;
    addArg(&cla);

    cla.description   = "Set minimum of n assets in portfolio";
    cla.parameter     = 'n';
    cla.shortName     = 'r';
    cla.type          = CLA_INT;
    cla.valuePtr.iptr = &OPTIONS.param2MinInt;
    addArg(&cla);

    cla.description   = "Set maximum of n assets in portfolio";
    cla.parameter     = 'n';
    cla.shortName     = 's';
    cla.type          = CLA_INT;
    cla.valuePtr.iptr = &OPTIONS.param2MaxInt;
    addArg(&cla);

    cla.description   = "Run n tests on each grid cell, at randomized start times";
    cla.parameter     = 'n';
    cla.shortName     = 'T';
    cla.type          = CLA_INT;
    cla.valuePtr.iptr = &OPTIONS.numTests;
    addArg(&cla);

    cla.description   = "Use n divisions on price";
    cla.parameter     = 'n';
    cla.shortName     = 'c';
    cla.type          = CLA_INT;
    cla.valuePtr.iptr = &OPTIONS.divisions1;
    addArg(&cla);

    cla.description   = "Use n divisions on number of assets in portfolio";
    cla.parameter     = 'n';
    cla.shortName     = 'd';
    cla.type          = CLA_INT;
    cla.valuePtr.iptr = &OPTIONS.divisions2;
    addArg(&cla);

    cla.description   = "Display text log for an example run, rather than doing a full test";
    cla.parameter     = 0;
    cla.shortName     = 't';
    cla.type          = CLA_FLAG;
    cla.valuePtr.iptr = &OPTIONS.textDemo;
    addArg(&cla);

    cla.description   = "Graph an example run, rather than doing a full test";
    cla.parameter     = 0;
    cla.shortName     = 'g';
    cla.type          = CLA_FLAG;
    cla.valuePtr.iptr = &OPTIONS.graphDemo;
    addArg(&cla);
}

void parseArgs(int argc, char *argv[]) {
    initOptions();
    initParser();
    parseCommandLineArgs(argc, argv);
    OPTIONS.param1Min = (double)OPTIONS.param1MinInt;
    OPTIONS.param1Max = (double)OPTIONS.param1MaxInt;
    OPTIONS.param2Min = (double)OPTIONS.param2MinInt;
    OPTIONS.param2Max = (double)OPTIONS.param2MaxInt;

    if (OPTIONS.graphDemo && OPTIONS.textDemo) {
        fprintf(stderr, "Cannot specify text demo and graphing demo. Specify one or the other only.\n");
        printHelpMessage();
        exit(1);
    }
}

struct SimState *stateInit(double p1, double p2) {
    struct SimState *state = malloc(sizeof(*state));
    copySimState(state, &BASE_STATE);

    union Symbol prSymbol;
    strncpy(prSymbol.name, "V-REBAL", SYMBOL_LENGTH);
    struct MeanPricePortfolioRebalanceArgs *args = (struct MeanPricePortfolioRebalanceArgs *)makeCustomOrder(state, &prSymbol, 1, meanPricePortfolioRebalance)->aux;
    args->targetPrice      = p1;
    args->epsilon          = OPTIONS.epsilon;
    args->history          = 6*MONTH;
    args->sampleFrequency  = 12*HOUR;
    args->maxAssetValue    = -1; // must be negative to not act as a limit
    args->numSymbols       = (int)p2;

    return state;
}
