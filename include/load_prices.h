#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>
#include "types.h"

// 2^20
#define PRICE_SERIES_LENGTH 1048576
#define PRICE_CACHE_ENTRIES 64

/**
 * Structs
 */

struct Prices {
    time_t times[PRICE_SERIES_LENGTH];
    unsigned int prices[PRICE_SERIES_LENGTH];
    unsigned long validRows;
    unsigned long lastUsage;
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