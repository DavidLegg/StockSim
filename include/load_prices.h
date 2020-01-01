#ifndef LOAD_PRICES_H
#define LOAD_PRICES_H

#include <time.h>
#include <semaphore.h>

#include "types.h"

// 2^20
#define PRICE_SERIES_LENGTH 1048576
#define PRICE_CACHE_ENTRIES 64

/**
 * Structs
 */

struct readerWriterLock {
    sem_t resourceLock;
    sem_t readCountLock;
    int readCount;
};

struct Prices {
    struct readerWriterLock readerWriterLock;
    time_t times[PRICE_SERIES_LENGTH];
    unsigned int prices[PRICE_SERIES_LENGTH];
    unsigned long validRows;
    unsigned long lastUsage;
    union Symbol symbol;
};
// TODO: add synchronization

/**
 * Public Accessors
 */

unsigned int getHistoricalPrice(const union Symbol *symbol, const time_t time);

/**
 * Helpers
 */

void loadHistoricalPrice(struct Prices *p, const time_t time);
void initReaderWriterLock(struct readerWriterLock *lock);
void readerWait(struct readerWriterLock *lock);
void readerPost(struct readerWriterLock *lock);
void writerWait(struct readerWriterLock *lock);
void writerPost(struct readerWriterLock *lock);

#endif // ifndef LOAD_PRICES_H