#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "load_prices.h"

#define TIME_PERIOD_CACHE_SIZE 16384
#define MAX_PC_THREADS 16

// All available names for a data file, with %s indicating ticker symbol.
// Names listed from most preferred to least
static const char *PRICE_FILENAME_FORMATS[] = {
    "resources/%s_daily_bars_san.csv"
};
static const int NUM_PRICE_FILENAME_FORMATS = sizeof(PRICE_FILENAME_FORMATS) / sizeof(*PRICE_FILENAME_FORMATS);

struct PriceCache {
    struct Prices entries[PRICE_CACHE_ENTRIES];
    long usageCounter;
    pthread_t thread_id;
};

struct TimePeriod {
    time_t start, end;
    union Symbol symbol;
    struct TimePeriod *next;
};

static struct PriceCache **PRICE_CACHES = NULL;
static struct TimePeriod TIME_PERIOD_CACHE[TIME_PERIOD_CACHE_SIZE];
static int TPC_MAX_USED = 0;
static const char *ALL_SYMBOLS_FILE = "resources/symbols.txt";
static union Symbol ALL_SYMBOLS[TIME_PERIOD_CACHE_SIZE];

/**
 * Forward Declarations
 */

void initializeTimePeriodCache(void);
void quicksortTPC(int start, int end);
long getPriceFromCache(const union Symbol *symbol, const time_t time, struct PriceCache *priceCache);

/**
 * Initializers & Modifiers
 */

void historicalPriceInit() {
    if (PRICE_CACHES) return;

    PRICE_CACHES = malloc(sizeof(*PRICE_CACHES) * MAX_PC_THREADS);

    for (int i = 0; i < MAX_PC_THREADS; ++i) {
        PRICE_CACHES[i] = NULL;
    }
    initializeTimePeriodCache();
}

void initializePriceCache(struct PriceCache *priceCache) {
    for (long i = 0; i < PRICE_CACHE_ENTRIES; ++i) {
        priceCache->entries[i].symbol.id = 0;
    }
    priceCache->usageCounter = 0;
}

void historicalPriceAddThread(pthread_t tid) {
    int i;
    for (i = 0; i < MAX_PC_THREADS && PRICE_CACHES[i]; ++i) ;
    if (i >= MAX_PC_THREADS) {
        fprintf(stderr, "Max number of price caches exceeded\n");
        exit(1);
    }

    PRICE_CACHES[i] = malloc(sizeof(*PRICE_CACHES[i]));
    initializePriceCache(PRICE_CACHES[i]);
    PRICE_CACHES[i]->thread_id = tid;
}

void initializeTimePeriodCache(void) {
    const int bufSize = 256;
    char buf[bufSize];
    memset(buf, 0, bufSize * sizeof(char));
    char *front, *back;

    int symbolCol, startCol, endCol, col;
    symbolCol = startCol = endCol = -1;

    FILE *fp = NULL;
    if (!(fp = fopen(ALL_SYMBOLS_FILE, "r"))) {
        fprintf(stderr, "Cannot open symbols file %s\n", ALL_SYMBOLS_FILE);
        exit(1);
    }

    // Read in column headers
    while (fgets(buf, bufSize, fp) &&
           (symbolCol < 0 || startCol < 0 || endCol < 0)) {
        col = 0;
        for (back = buf; *back; back = front + 1) {
            for (front = back; *front && *front != ',' && *front != '\n'; ++front) ;
            *front = 0;
            if (!strcmp(back, "Symbol")) {
                symbolCol = col;
            } else if (!strcmp(back, "Start Time")) {
                startCol = col;
            } else if (!strcmp(back, "End Time")) {
                endCol = col;
            }
            ++col;
        }
    }

    // Read in data
    while (fgets(buf, bufSize, fp)) {
        col = 0;
        for (back = buf; *back; back = front + 1) {
            for (front = back; *front && *front != ','; ++front) ;
            *front = 0;
            if (col == symbolCol) {
                strncpy(TIME_PERIOD_CACHE[TPC_MAX_USED].symbol.name, back, SYMBOL_LENGTH);
            } else if (col == startCol) {
                sscanf(back, "%ld", &TIME_PERIOD_CACHE[TPC_MAX_USED].start);
            } else if (col == endCol) {
                sscanf(back, "%ld", &TIME_PERIOD_CACHE[TPC_MAX_USED].end);
            }
            ++col;
        }
        ++TPC_MAX_USED;
        if (TPC_MAX_USED >= TIME_PERIOD_CACHE_SIZE) {
            fprintf(stderr, "Exceeded maximum size of time period cache\n");
            exit(1);
        }
    }

    fclose(fp);

    // Do an in-place quicksort of symbols
    quicksortTPC(0, TPC_MAX_USED - 1);

    // Load the all-symbols array
    for (int i = 0; i < TPC_MAX_USED; ++i) {
        ALL_SYMBOLS[i].id = TIME_PERIOD_CACHE[i].symbol.id;
    }
}

/**
 * Public Accessors
 */

long getHistoricalPrice(const union Symbol *symbol, const time_t time) {
    pthread_t tid = pthread_self();
    int i;
    for (i = 0; i < MAX_PC_THREADS && PRICE_CACHES[i]; ++i) {
        if (pthread_equal(PRICE_CACHES[i]->thread_id, tid)) {
            return getPriceFromCache(symbol, time, PRICE_CACHES[i]);
        }
    }
    fprintf(stderr, "No price cache initialized for thread %lu\n", tid);
    exit(1);
}

long getHistoricalPriceTimePeriod(const union Symbol *symbol, time_t *start, time_t *end) {
    if (!TPC_MAX_USED) {
        fprintf(stderr, "Time Period Cache not initialized. Call initializeTimePeriodCache() first\n");
        exit(1);
    }

    // Binary search for symbol id
    int mn, mx, split;
    mn = 0;
    mx = TPC_MAX_USED - 1;
    while (mn < mx) {
        split = (mx + mn) / 2;
        if (TIME_PERIOD_CACHE[split].symbol.id < symbol->id) {
            mn = split + 1;
        } else {
            mx = split;
        }
    }

    // Check if symbol was found
    if (TIME_PERIOD_CACHE[mx].symbol.id != symbol->id) return 0;

    *start = TIME_PERIOD_CACHE[mx].start;
    *end   = TIME_PERIOD_CACHE[mx].end;
    return 1;
}

const union Symbol *getAllSymbols(int *n) {
    if (!TPC_MAX_USED) {
        fprintf(stderr, "Time Period Cache not initialized. Call initializeTimePeriodCache() first\n");
        exit(1);
    }

    *n = TPC_MAX_USED;
    return ALL_SYMBOLS;
}



/**
 * Helpers
 */

long getPriceFromCache(const union Symbol *symbol, const time_t time, struct PriceCache *priceCache) {
    struct Prices *p;
    struct Prices *pEnd = priceCache->entries + PRICE_CACHE_ENTRIES;
    struct Prices *lruEntry = priceCache->entries;
    long lrUsage = lruEntry->lastUsage;

    // Find correct entry, if possible
    for (p = priceCache->entries; p < pEnd && p->symbol.id; ++p) {
        if (p->symbol.id == symbol->id &&
            p->times[0] <= time &&
            p->times[p->validRows - 1] >= time) {
            // Found the entry we want to read.
            break;
        }
        if (p->lastUsage < lrUsage) {
            lruEntry = p;
            lrUsage  = p->lastUsage;
        }
    }

    if (p >= pEnd) {
        // No match found, no empty entry available.
        // Replace the LRU entry with needed chunk
        p = lruEntry;
        lruEntry->symbol.id = symbol->id;
        loadHistoricalPrice(lruEntry, time);
    } else if (!p->symbol.id) {
        // No match found, empty entry available.
        // Load that entry with needed chunk
        p->symbol.id = symbol->id;
        loadHistoricalPrice(p, time);
    } // else, match found, don't do anything

    // At this point, no matter which path was taken,
    // p points to a cache entry with the correct data

    time_t *mn, *mx, *split;
    mn = p->times;
    mx = p->times + p->validRows - 1;
    while (mx - mn > 1) {
        // Compute split as a guess of target location
        split = mn + ( ((mx - mn) * (time - *mn)) / (*mx - *mn) );
        split = (mx > split ? (mn < split ? split : mn + 1) : mx - 1);
        if (*split <= time) {
            mn = split;
        } else {
            mx = split;
        }
    }

    p->lastUsage = ++(priceCache->usageCounter);
    return p->prices[mn - p->times];
}

void loadHistoricalPrice(struct Prices *p, const time_t time) {
    const int bufSize = 256;
    char buf[bufSize];
    memset(buf, 0, bufSize);
    long prevLineStart, thisLineStart;
    prevLineStart = thisLineStart = 0;

    // Make a null-terminated string:
    char symbolName[SYMBOL_LENGTH + 1];
    strncpy(symbolName, p->symbol.name, SYMBOL_LENGTH);
    symbolName[SYMBOL_LENGTH] = 0;

    // Find a data file for this symbol
    FILE *fp = NULL;
    for (int i = 0; i < NUM_PRICE_FILENAME_FORMATS; ++i) {
        snprintf(buf, bufSize, PRICE_FILENAME_FORMATS[i], symbolName);
        if ((fp = fopen(buf, "r"))) {
            // File successfully opened. Stop searching.
            break;
        }
        // File wasn't successfully opened, move to next possible file.
    }
    if (!fp) {
        fprintf(stderr, "No data file found for symbol %s\n", symbolName);
        exit(1);
    }

    int timeCol, priceCol, col, maxCol;
    timeCol = priceCol = -1;

    char *back, *front;

    time_t *nextTime = p->times;
    long *nextPrice = p->prices;
    double tempPrice;
    long loadedRows = 0;

    time_t rowTime;

    // Look for column header, and find timestamp & open columns
    while ((timeCol < 0 || priceCol < 0) &&
           fgets(buf, bufSize, fp)) {
        back = front = buf;
        col = 0;
        while (*back) {
            while (*front && *front != ',') ++front;
            if (!strncmp(back, "Unix Timestamp", front - back)) {
                timeCol = col;
            }
            if (!strncmp(back, "Open", front - back)) {
                priceCol = col;
            }
            back = ++front;
            ++col;
        }
        prevLineStart = thisLineStart;
        thisLineStart = ftell(fp);
    }
    maxCol = (timeCol < priceCol ? priceCol : timeCol);

    // Skim forward through the file,
    // looking for the desired time
    while (fgets(buf, bufSize, fp)) {
        back  = buf;
        front = buf - 1;
        col   = timeCol + 1;
        while (*back && col) {
            back = ++front;
            while (*front && *front != ',') ++front;
            --col;
        }
        if (!col) {
            *front = 0;
            sscanf(back, "%ld", &rowTime);
            if (rowTime / 1000 >= time) {
                // Found the first line that's at least the desired time
                // Seek to the previous line (to catch a time prior to desired)
                // Then move on to the reading phase.
                fseek(fp, prevLineStart, SEEK_SET);
                break;
            }
        }
        prevLineStart = thisLineStart;
        thisLineStart = ftell(fp);
    }

    // Having adjusted the file pointer to the start
    // of the desired data, read in as much as we can
    while (loadedRows < PRICE_SERIES_LENGTH &&
           fgets(buf, bufSize, fp)) {
        back = front = buf;
        col = 0;
        while (col <= maxCol && *front) {
            while (*front && *front != ',') ++front;
            *front = 0;
            if (col == timeCol) {
                sscanf(back, "%ld", &rowTime);
                *nextTime++ = rowTime / 1000;
            } else if (col == priceCol) {
                // read price as fractional dollars, store as integer cents
                sscanf(back, "%lf", &tempPrice);
                *(nextPrice++) = (long)round(tempPrice * DOLLAR);
            } // else, not a column we care about, keep going
            back = ++front;
            ++col;
        }
        ++loadedRows;
    }

    fclose(fp);
    p->validRows = loadedRows;
}

// Hoare partition quicksort, as described at https://en.wikipedia.org/wiki/Quicksort#Hoare_partition_scheme
// Since values will be unique, we simplify the inner loop to a while, not a do-while.
void quicksortTPC(int start, int end) {
    if (start >= end) return;
    SYMBOL_ID_TYPE pivot = TIME_PERIOD_CACHE[(start + end) / 2].symbol.id;
    int low = start;
    int high = end;
    struct TimePeriod temp;
    while (low < high) {
        while (TIME_PERIOD_CACHE[low].symbol.id < pivot) ++low;
        while (TIME_PERIOD_CACHE[high].symbol.id > pivot) --high;
        temp = TIME_PERIOD_CACHE[low];
        TIME_PERIOD_CACHE[low]  = TIME_PERIOD_CACHE[high];
        TIME_PERIOD_CACHE[high] = temp;
    }
    quicksortTPC(start, high);
    quicksortTPC(high + 1, end);
}
