#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "execution.h"
#include "load_prices.h"

// unsigned int dummy_price(const union Symbol* symbol, const time_t time) {
//     return 2000;
// }

int main(int argc, char const *argv[])
{
    struct tm simStartTime;
    strptime("12/1/2019 13:00", "%m/%d/%Y%n%H:%M", &simStartTime);
    struct SimState state;
    initSimState(&state, mktime(&simStartTime));
    state.cash = 100000;
    state.priceFn = getHistoricalPrice;
    printf("Initialized state.\n");
    printSimState(&state);
    printf("Adding orders...\n");
    union Symbol symbol;
    strncpy(symbol.name, "BTC", SYMBOL_LENGTH);
    buy(&state, &symbol, 2);
    sell(&state, &symbol, 1);
    strncpy(symbol.name, "ETH", SYMBOL_LENGTH);
    buy(&state, &symbol, 1);
    printSimState(&state);
    printf("Stepping...\n");
    step(&state);
    printSimState(&state);
    step(&state);
    step(&state);
    step(&state);
    step(&state);
    printSimState(&state);
    return 0;
}

