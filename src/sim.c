#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "execution.h"
#include "load_prices.h"
#include "strategies.h"
#include "batch_execution.h"
#include "strategy_testing.h"
#include "histogram.h"

int main(__attribute__ ((unused)) int argc, __attribute__ ((unused)) char const *argv[])
{
    printf("Initializing workers...\n");
    initJobQueue();
    unsigned int seed = time(0);
    printf("Seed value: %d\n", seed);
    srand(seed);

    struct tm structSimStartTime;
    strptime("1/1/2010 00:00", "%m/%d/%Y%n%H:%M", &structSimStartTime);
    structSimStartTime.tm_isdst = -1;
    structSimStartTime.tm_sec = 0;

    printf("Constructing base scenario...\n");
    int startCash = 10000*DOLLAR;
    struct SimState state;
    initSimState(&state, 0);
    state.cash = startCash;
    state.priceFn = getHistoricalPrice;
    state.time = mktime(&structSimStartTime);

    // Create and configure a portfolio-rebalancing strategy
    struct RandomPortfolioRebalanceArgs *prArgs = (struct RandomPortfolioRebalanceArgs *)makeCustomOrder(&state, NULL, 0, randomPortfolioRebalance)->aux;
    prArgs->numSymbols = 10;
    prArgs->maxAssetValue = startCash;

    // Create a time horizon
    struct TimeHorizonArgs *thArgs = (struct TimeHorizonArgs *)makeCustomOrder(&state, NULL, 0, timeHorizon)->aux;
    thArgs->offset = 1*YEAR;
    thArgs->cutoff = 0;

    // Create a single-thread execution price cache, and attach it to the state
    struct PriceCache *priceCache = malloc(sizeof(struct PriceCache));
    initializePriceCache(priceCache);
    state.priceCache = priceCache;

    printf("Executing...\n");
    graphScenario(&state);
    printf("Starting cash  $%.2f\n", startCash / 100.0);
    printf("Ending cash    $%.2f (%.1f%%)\n", state.cash / 100.0, (100.0 * (state.cash - startCash)) / startCash);

    return 0;
}

