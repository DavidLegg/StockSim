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
    int numBins;   // number of bins on histogram
    int numSymbols;
    long startCash;
    time_t testLength;
    time_t periodStart;
    time_t periodEnd;
    double param1Min;
    double param1Max;
    double param2Min;
    double param2Max;
    int divisions;
} OPTIONS;

const char HELP_STR[] =
    "Usage: stock-sim [options]\n"
    "    Run a simulation of a stock trading strategy\n"
    "\n"
    "    In particular, run a grid-test of target volatility vs. number of assets in portfolio.\n"
    "\n"
    "Options:\n"
    "    -v x    Set minimum target volatility of x\n"
    "    -V x    Set maximum target volatility of x\n"
    "    -s n    Set minimum of n assets in portfolio\n"
    "    -S n    Set maximum of n assets in portfolio\n"
    "    -T n    Run n tests on each grid cell, at randomized start times\n"
    "    -d n    Use n divisions for each axis of the grid\n"
    "    -t      Display text log for an example run, rather than doing a full test\n"
    "    -g      Graph an example run, rather than doing a full test\n"
    "    -h      Display this help message and exit\n"
    ;

const char param1Name[] = "Target Volatility";
const char param2Name[] = "Number Assets";
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
    rsArgs.dcs = &FinalCashDCS;
    rsArgs.n   = OPTIONS.numTests;

    initSimState(&BASE_STATE, 0);
    BASE_STATE.priceFn = getHistoricalPrice;
    BASE_STATE.cash    = OPTIONS.startCash;

    // Create a time horizon
    struct TimeHorizonArgs *thArgsControl = (struct TimeHorizonArgs *)makeCustomOrder(&BASE_STATE, NULL, 0, timeHorizon)->aux;
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
        struct SimState *state = stateInit(
                (OPTIONS.param1Min + OPTIONS.param1Max) / 2,
                (OPTIONS.param2Min + OPTIONS.param2Max) / 2
            );
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
                OPTIONS.divisions
            );
        displayGrid2(
                results,
                OPTIONS.param1Min,
                OPTIONS.param1Max,
                OPTIONS.param2Min,
                OPTIONS.param2Max,
                OPTIONS.divisions,
                "Param 1",
                "Param 2"
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
    OPTIONS.param2Min        = 0;
    OPTIONS.param2Max        = 0;
    OPTIONS.divisions        = 5;

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

void parseArgs(int argc, char *argv[]) {
    initOptions();
    int opt;
    while ((opt = getopt(argc, argv, "v:V:s:S:T:d:tg")) != -1) {
        switch (opt) {
            case 'u':
                sscanf(optarg, "%lf", &OPTIONS.param1Min);
                break;
            case 'v':
                sscanf(optarg, "%lf", &OPTIONS.param1Max);
                break;
            case 'r':
                sscanf(optarg, "%d", &opt);
                OPTIONS.param2Min = (double)opt;
                break;
            case 's':
                sscanf(optarg, "%d", &opt);
                OPTIONS.param2Max = (double)opt;
                break;
            case 'T':
                sscanf(optarg, "%d", &OPTIONS.numTests);
                break;
            case 'd':
                sscanf(optarg, "%d", &OPTIONS.divisions);
                break;
            case 't':
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

struct SimState *stateInit(double p1, double p2) {
    struct SimState *state = malloc(sizeof(*state));
    copySimState(state, &BASE_STATE);

    struct VolatilityPortfolioRebalanceArgs *args = (struct VolatilityPortfolioRebalanceArgs *)makeCustomOrder(state, NULL, 1, volatilityPortfolioRebalance)->aux;
    args->targetVolatility = p1;
    args->epsilon          = 0.1;
    args->history          = 1*YEAR;
    args->sampleFrequency  = 12*HOUR;
    args->maxAssetValue    = 0;
    args->numSymbols       = (int)p2;

    return state;
}
