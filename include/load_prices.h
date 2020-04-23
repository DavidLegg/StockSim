#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>

#include "types.h"

// 2^18
#define PRICE_SERIES_LENGTH 262144
#define PRICE_CACHE_ENTRIES 64

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

/**
 * Public Accessors
 */

/**
 * Returns the price stored in the price file, at the given time.
 * Automatically handles caching in an efficient, thread-safe way
 */
long getHistoricalPrice(const union Symbol *symbol, const time_t time);
/**
 * Determines start/end times for historical pricing data for given symbol.
 * If no historical price data for symbol exists, returns 0.
 * Otherwise, returns 1 and stores start/end times in *start and *end.
 */
long getHistoricalPriceTimePeriod(const union Symbol *symbol, time_t *start, time_t *end);
/**
 * Returns all available symbols, as a statically allocated array.
 * Writes size of array to address pointed to by n.
 * Do not call free on result of this function.
 */
const union Symbol *getAllSymbols(int *n);

/**
 * Public Modifiers
 */

/**
 * This method needs to be called once by the master thread,
 * prior to worker thread creation.
 */
void historicalPriceInit();

/**
 * This method needs to be called by the master thread
 * with the thread id of every thread that can call getHistoricalPrice
 */
void historicalPriceAddThread(pthread_t tid);


/**
 * Helpers
 */

void loadHistoricalPrice(struct Prices *p, const time_t time);

#endif // ifndef LOAD_PRICES_H