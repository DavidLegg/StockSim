#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "execution.h"
#include "load_prices.h"
#include "strategies.h"
#include "batch_execution.h"

// unsigned int dummy_price(const union Symbol* symbol, const time_t time) {
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
    time_t simStartTime, simEndTime;
    simStartTime = mktime(&structSimStartTime);
    simEndTime   = simStartTime + 300*24*60*60;
    unsigned long skipSeconds = 60*60;
    struct SimState state;
    initSimState(&state, simStartTime);
    state.cash = 100000;
    state.priceFn = getHistoricalPrice;
    printf("Initialized state.\n");
    union Symbol symbol;
    strncpy(symbol.name, "BTC", SYMBOL_LENGTH);
    makeCustomOrder(&state, &symbol, 1, dummyStrat);
    printf("Running scenarios...\n");
    unsigned int *resultsEnd = NULL;
    unsigned int *results = runTimes(&state, simStartTime, simEndTime, skipSeconds, &resultsEnd);
    printf("Execution complete. Results:\n");
    for (unsigned int *r = results; r < resultsEnd; ++r) {
        printf("%c", (*r > 100000 ? '^' : '.'));
    }
    printf("\n");
    return 0;
}

