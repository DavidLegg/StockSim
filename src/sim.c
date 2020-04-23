#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
    long startCash    = 10000*DOLLAR;
    time_t testLength = 1*YEAR;
    int numSymbols    = 20;
    int numTests      = 1000;
    int numBins       = 15;
    struct SimState scenarios[2];
    struct SimState *controlState = scenarios, *testState = scenarios + 1;

    unsigned int seed = time(0);
    printf("Seed value: %d\n", seed);
    tsRandInit(seed);
    historicalPriceInit();

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

    // Execute the test
    printf("Executing test...\n");
    long **results, **resultsEnd;
    results = (long**)randomizedStartComparison(scenarios, 2, numTests, testStart, testEnd, &FinalCashDCS, (void***)&resultsEnd);

    // Process results
    printf("Processing results...\n");
    long *resultsDelta = malloc(sizeof(long) * (*resultsEnd - *results));
    for (int i = 0; *results + i < *resultsEnd; ++i) {
        resultsDelta[i] = results[1][i] - results[0][i];
    }
    free(results[0]);
    free(results[1]);
    free(results);
    resultsEnd = results = NULL;

    // Display results
    printf("Summary:\n");
    drawHistogram(resultsDelta, resultsDelta + numTests, numBins, HF_Currency);

    return 0;
}

