#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>

#include "types.h"

// 2^20
#define PRICE_SERIES_LENGTH 1048576
#define PRICE_CACHE_ENTRIES 16

/**
 * Structs
 */

struct Prices {
    time_t times[PRICE_SERIES_LENGTH];
    long prices[PRICE_SERIES_LENGTH];
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

/**
 * Returns the price stored in the price file, at the given time.
 * Uses the given priceCache to accelerate lookups.
 */
long getHistoricalPrice(const union Symbol *symbol, const time_t time, struct PriceCache *priceCache);
/**
 * Determines start/end times for historical pricing data for given symbol.
 * If no historical price data for symbol exists, returns 0.
 * Otherwise, returns 1 and stores start/end times in *start and *end.
 */
long getHistoricalPriceTimePeriod(const union Symbol *symbol, time_t *start, time_t *end);

/**
 * Helpers
 */

void initializePriceCache(struct PriceCache *priceCache);
void loadHistoricalPrice(struct Prices *p, const time_t time);

#endif // ifndef LOAD_PRICES_H