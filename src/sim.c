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

// int dummy_price(const union Symbol* symbol, const time_t time) {
//     return 2000;
// }

enum OrderStatus dummyStrat(struct SimState *state, struct Order *order) {
    if (order->quantity > 0) {
        buy(state, &(order->symbol), order->quantity);
        order->quantity = -order->quantity;
    } else {
        sell(state, &(order->symbol), -order->quantity);
        return None;
    }
    return Active;
}

int main(__attribute__ ((unused)) int argc, __attribute__ ((unused)) char const *argv[])
{
    initJobQueue();

    struct tm structSimStartTime;
    strptime("1/1/2019 01:00", "%m/%d/%Y%n%H:%M", &structSimStartTime);
    structSimStartTime.tm_isdst = -1;
    structSimStartTime.tm_sec = 0;
    time_t simStartTime, simEndTime;
    simStartTime = mktime(&structSimStartTime);
    simEndTime   = simStartTime + 300*24*60*60; // 300 days
    long skipSeconds = 24*60*60; // 24 hours
    int startCash = 10000; // $100.00
    struct SimState state;
    initSimState(&state, simStartTime);
    state.cash = startCash;
    state.priceFn = getHistoricalPrice;
    printf("Initialized state.\n");
    union Symbol symbol;
    strncpy(symbol.name, "BTC", SYMBOL_LENGTH);
    makeCustomOrder(&state, &symbol, 1, emaStrat);


    printf("Running scenarios...\n");
    int *resultsEnd = NULL;
    int *results = runTimes(&state, simStartTime, simEndTime, skipSeconds, &resultsEnd);
    long numResults = resultsEnd - results;
    printf("Execution complete.\n");
    int numSuccesses = 0;
    double avgProfit = 0;
    for (int *r = results; r < resultsEnd; ++r) {
        if (*r > startCash) {
            ++numSuccesses;
        }
        avgProfit += *(int*)r - (int)startCash;
    }
    avgProfit /= numResults;
    printf("Profit on %d/%ld (%%%.0f)\n",
        numSuccesses,
        numResults,
        100.0 * numSuccesses / numResults);
    printf("Average profit: ");
    if (avgProfit > 0) {
        printf("$%.4f (+%.1f%%)\n", avgProfit / 100, 100 * avgProfit / startCash);
    } else {
        printf("-$%.4f (-%.1f%%)\n", -avgProfit / 100, -100 * avgProfit / startCash);
    }
    free(results);


    // printf("Running scenario...\n");
    // runScenario(&state);
    // printf("Execution complete.\n");
    // printf("Start cash: $%.2f\n", startCash / 100.0);
    // printf("Final cash: $%.2f (", state.cash / 100.0);
    // if (state.cash >= startCash) {
    //     printf("+ $%.2f, %.1f%%)\n", (state.cash - startCash) / 100.0, 100.0 * (state.cash - startCash) / startCash);
    // } else {
    //     printf("- $%.2f, %.1f%%)\n", (startCash - state.cash) / 100.0, 100.0 * (startCash - state.cash) / startCash);
    // }
    // return 0;
}

