#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "load_prices.h"

/**
 * Globals
 */

static struct Prices PRICE_CACHE[PRICE_CACHE_ENTRIES];
static int INITIALIZED_PRICE_CACHE = 0;
static unsigned long globalUsageCounter = 0;

unsigned int getHistoricalPrice(const union Symbol *symbol, const time_t time) {
    if (!INITIALIZED_PRICE_CACHE) {
        for (unsigned long i = 0; i < PRICE_CACHE_ENTRIES; ++i) {
            PRICE_CACHE[i].symbol.id = 0;
        }
        INITIALIZED_PRICE_CACHE = 1;
    }

    struct Prices *p = PRICE_CACHE;
    struct Prices *lruEntry = p;

    while (p < PRICE_CACHE + PRICE_CACHE_ENTRIES &&
           p->symbol.id &&
           (p->symbol.id != symbol->id ||
            p->times[0] > time ||
            p->times[p->validRows - 1] < time)) {
        if (p->lastUsage < lruEntry->lastUsage) {
            lruEntry = p;
        }
        ++p;
    }
    if (p >= PRICE_CACHE + PRICE_CACHE_ENTRIES) {
        // No match found, no empty entry available.
        // Replace the LRU entry with needed chunk
        lruEntry->symbol.id = symbol->id;
        loadHistoricalPrice(lruEntry, time);
        p = lruEntry;
    } else if (!p->symbol.id) {
        // No match found, empty entry available.
        // Load that entry with needed chunk
        p->symbol.id = symbol->id;
        loadHistoricalPrice(p, time);
    } // else, match found, don't do anything

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

    p->lastUsage = ++globalUsageCounter;
    return p->prices[mn - p->times];
}

void loadHistoricalPrice(struct Prices *p, const time_t time) {
    const char fileFormat[] = "resources/gemini_%sUSD_2019_1min.csv";
    const unsigned int bufSize = 256;
    char buf[bufSize];
    long prevLineStart, thisLineStart;
    prevLineStart = thisLineStart = 0;

    // Make a null-terminated string:
    char symbolName[SYMBOL_LENGTH + 1];
    strncpy(symbolName, p->symbol.name, SYMBOL_LENGTH);
    symbolName[SYMBOL_LENGTH] = 0;

    snprintf(buf, bufSize, fileFormat, symbolName);
    FILE *fp = fopen(buf, "r");
    if (!fp) {
        fprintf(stderr, "Error opening data file %s.\n", buf);
        exit(1);
    }

    int timeCol, priceCol, col, maxCol;
    timeCol = priceCol = -1;

    char *back, *front;

    time_t *nextTime = p->times;
    unsigned int *nextPrice = p->prices;
    unsigned long loadedRows = 0;

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
                sscanf(back, "%u", nextPrice++);
            } // else, not a column we care about, keep going
            back = ++front;
            ++col;
        }
        ++loadedRows;
    }

    fclose(fp);
    p->validRows = loadedRows;
}