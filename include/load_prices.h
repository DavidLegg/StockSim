#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>
#include "types.h"

// 2^20
#define PRICE_DATA_LENGTH 1048576
#define NUM_SYMBOLS 64

/**
 * Structs
 */

struct Prices {
    time_t times[PRICE_DATA_LENGTH];
    unsigned int prices[PRICE_DATA_LENGTH];
    unsigned long validRows;
    union Symbol symbol;
};

/**
 * Public Accessors
 */

unsigned int getHistoricalPrice(const union Symbol *symbol, const time_t time);

/**
 * Helpers
 */

void loadHistoricalPrice(struct Prices *p, const time_t time);

#endif // ifndef LOAD_PRICES_H