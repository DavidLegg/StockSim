#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "rng.h"
#include "execution.h"
#include "load_prices.h"
#include "strategies.h"
#include "batch_execution.h"
#include "strategy_testing.h"
#include "histogram.h"

int main(__attribute__ ((unused)) int argc, __attribute__ ((unused)) char const *argv[])
{
    unsigned int seed = time(0);
    printf("Seed value: %d\n", seed);
    tsRandInit(seed);

    printf("Initializing TPC...\n");
    initializeTimePeriodCache();

    printf("Constructing base scenario...\n");
    long startCash = 10000*DOLLAR;
    struct SimState state;
    initSimState(&state, 0);
    state.cash = startCash;
    state.priceFn = getHistoricalPrice;

    // Create and configure a portfolio-rebalancing strategy
    struct RandomPortfolioRebalanceArgs *prArgs = (struct RandomPortfolioRebalanceArgs *)makeCustomOrder(&state, NULL, 0, randomPortfolioRebalance)->aux;
    prArgs->numSymbols = 10;
    prArgs->maxAssetValue = startCash;

    // Create a time horizon
    struct TimeHorizonArgs *thArgs = (struct TimeHorizonArgs *)makeCustomOrder(&state, NULL, 0, timeHorizon)->aux;
    thArgs->offset = 1*YEAR;
    thArgs->cutoff = 0;

    // // Create a single-thread execution price cache, and attach it to the state
    // struct PriceCache *priceCache = malloc(sizeof(struct PriceCache));
    // initializePriceCache(priceCache);
    // state.priceCache = priceCache;

    // printf("Executing...\n");
    // graphScenario(&state);
    // printf("Starting cash  $%.2f\n", startCash / 100.0);
    // printf("Ending cash    $%.2f (%.1f%%)\n", state.cash / 100.0, (100.0 * (state.cash - startCash)) / startCash);

    // Create a start and end time
    struct tm structStartTime, structEndTime;
    strptime("1/1/1981 00:00", "%m/%d/%Y%n%H:%M", &structStartTime);
    structStartTime.tm_isdst = -1;
    structStartTime.tm_sec = 0;
    strptime("1/1/2010 00:00", "%m/%d/%Y%n%H:%M", &structEndTime);
    structEndTime.tm_isdst = -1;
    structEndTime.tm_sec = 0;

    // Execute the test
    printf("Executing test...\n");
    randomizedStart(&state, 1000, mktime(&structStartTime), mktime(&structEndTime), collectFinalCash);
    // state.time = mktime(&structStartTime);
    // struct SimState original;
    // copySimState(&original, &state);
    // for (int i = 0; i < 100; ++i) {
    //     copySimState(&state, &original);
    //     runScenario(&state);
    //     printf("Starting cash  $%.2f\n", startCash / (double) DOLLAR);
    //     printf("Ending cash    $%.2f (%.1f%%)\n", state.cash / (double)DOLLAR, (100.0 * (state.cash - startCash)) / startCash);
    // }

    // Get and plot results
    int n;
    long *results;
    results = finalCashResults(&n);
    drawHistogram(results, results + n, 10);

    return 0;
}

