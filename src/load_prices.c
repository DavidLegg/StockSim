#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "load_prices.h"

// All available names for a data file, with %s indicating ticker symbol.
// Names listed from most preferred to least
const char *PRICE_FILENAME_FORMATS[] = {
    "resources/gemini_%sUSD_2019_1min.csv",
    "resources/%s_daily_bars_san.csv"
};
const int NUM_PRICE_FILENAME_FORMATS = sizeof(PRICE_FILENAME_FORMATS) / sizeof(*PRICE_FILENAME_FORMATS);

long getHistoricalPrice(const union Symbol *symbol, const time_t time, struct PriceCache *priceCache) {
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

void initializePriceCache(struct PriceCache *priceCache) {
    for (long i = 0; i < PRICE_CACHE_ENTRIES; ++i) {
        priceCache->entries[i].symbol.id = 0;
    }
    priceCache->usageCounter = 0;
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

long getHistoricalPriceTimePeriod(const union Symbol *symbol, time_t *start, time_t *end) {
    const int bufSize = 256;
    char buf[bufSize];
    memset(buf, 0, bufSize);
    char *back, *front;
    int col, timeCol = -1;
    time_t temp;

    // Make a null-terminated string:
    char symbolName[SYMBOL_LENGTH + 1];
    strncpy(symbolName, symbol->name, SYMBOL_LENGTH);
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

    // If no file was found, indicate no such data is available
    if (!fp) return 0;

    // Look for column header, and find timestamp column
    while (timeCol < 0 && fgets(buf, bufSize, fp)) {
        back = front = buf;
        col = 0;
        while (*back) {
            while (*front && *front != ',') ++front;
            if (!strncmp(back, "Unix Timestamp", front - back)) {
                timeCol = col;
                break;
            }
            back = ++front;
            ++col;
        }
    }

    // Read next line, and report timestamp as start of data
    // Warning! For simplicity, code assumes you have more than one datapoint in the file,
    // and the the first line after the header row is a valid data row
    if (!fgets(buf, bufSize, fp)) {
        fprintf(stderr, "Error while skimming data file for %.*s\n", SYMBOL_LENGTH, symbol->name);
        exit(1);
    }
    back  = buf;
    front = buf - 1;
    col   = timeCol + 1;
    while (*back && col) {
        back = ++front;
        while (*front && *front != ',') ++front;
        --col;
    }
    *front = 0;
    sscanf(back, "%ld", &temp);
    *start = temp / 1000;

    // Read the last line. Assumes that buf is more than long enough for it.
    fseek(fp, 1-bufSize, SEEK_END);
    if (!fread(buf, 1, bufSize, fp)) {
        fprintf(stderr, "Error finding last line in data file for %.*s\n", SYMBOL_LENGTH, symbol->name);
        exit(1);
    }
    // Replace the trailing newline with null
    *strrchr(buf, '\n') = 0;
    // Find start of last line
    front = strrchr(buf, '\n');
    back  = front + 1;
    col   = timeCol + 1;
    while (*back && col) {
        back = ++front;
        while (*front && *front != ',') ++front;
        --col;
    }
    *front = 0;
    sscanf(back, "%ld", &temp);
    *end = temp / 1000;

    fclose(fp);
    
    // Return success
    return 1;
}
