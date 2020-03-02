#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>
#include <semaphore.h>

#include "types.h"

// 2^20
#define PRICE_SERIES_LENGTH 1048576
#define PRICE_CACHE_ENTRIES 16

/**
 * Structs
 */

struct Prices {
    time_t times[PRICE_SERIES_LENGTH];
    int prices[PRICE_SERIES_LENGTH];
    long validRows;
    long lastUsage;
    union Symbol symbol;
};

struct PriceCache {
    struct Prices entries[PRICE_CACHE_ENTRIES];
    long usageCounter;
};

/**
 * Public Accessors
 */

int getHistoricalPrice(const union Symbol *symbol, const time_t time, struct PriceCache *priceCache);

/**
 * Helpers
 */

void initializePriceCache(struct PriceCache *priceCache);
void loadHistoricalPrice(struct Prices *p, const time_t time);

#endif // ifndef LOAD_PRICES_H