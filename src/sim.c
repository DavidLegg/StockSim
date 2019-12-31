#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "types.h"
#include "execution.h"
#include "load_prices.h"
#include "strategies.h"

// unsigned int dummy_price(const union Symbol* symbol, const time_t time) {
//     return 2000;
// }

int main(int argc, char const *argv[])
{
    struct tm simStartTime;
    strptime("11/1/2019 01:00", "%m/%d/%Y%n%H:%M", &simStartTime);
    struct SimState state;
    initSimState(&state, mktime(&simStartTime));
    state.cash = 100000;
    state.priceFn = getHistoricalPrice;
    printf("Initialized state.\n");
    printf("Adding orders...\n");
    union Symbol symbol;
    strncpy(symbol.name, "BTC", SYMBOL_LENGTH);
    makeCustomOrder(&state, &symbol, 1, emaStrat);
    printf("Running scenario...\n");
    // runScenarioDemo(&state, 100);
    printSimState(&state);
    runScenario(&state);
    printSimState(&state);
    printf("Scenario complete.\n");
    return 0;
}

