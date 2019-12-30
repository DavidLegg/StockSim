#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "load_prices.h"

/**
 * Globals
 */

static struct Prices PRICE_LIST[NUM_SYMBOLS];
static int INITIALIZED_PRICE_LIST = 0;

unsigned int getHistoricalPrice(const union Symbol *symbol, const time_t time) {
    if (!INITIALIZED_PRICE_LIST) {
        for (unsigned long i = 0; i < NUM_SYMBOLS; ++i) {
            PRICE_LIST[i].symbol.id = 0;
        }
        INITIALIZED_PRICE_LIST = 1;
    }

    struct Prices *p = PRICE_LIST;
    while (p->symbol.id &&
           p < PRICE_LIST + PRICE_DATA_LENGTH &&
           p->symbol.id != symbol->id) ++p;
    if (!p->symbol.id) {
        p->symbol.id = symbol->id;
        loadHistoricalPrice(p, time);
    }
    if (p->times[0] > time || p->times[p->validRows - 1] < time) {
        loadHistoricalPrice(p, time);
    }

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

    return p->prices[mn - p->times];
}

void loadHistoricalPrice(struct Prices *p, const time_t time) {
    const char fileFormat[] = "resources/gemini_%sUSD_2019_1min.csv";
    const unsigned int bufSize = 256;
    char buf[bufSize];

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

    int time_col, price_col, col;
    time_col = price_col = -1;

    char *back, *front;

    time_t *nextTime = p->times;
    unsigned int *nextPrice = p->prices;
    unsigned long loadedRows = 0;

    time_t rowTime;
    int foundTime = 0;

    // Look for column header, and find timestamp & open columns
    while ((time_col < 0 || price_col < 0) &&
           fgets(buf, bufSize, fp)) {
        back = front = buf;
        col = 0;
        while (*back) {
            while (*front && *front != ',') ++front;
            if (!strncmp(back, "Unix Timestamp", front - back)) {
                time_col = col;
            }
            if (!strncmp(back, "Open", front - back)) {
                price_col = col;
            }
            back = ++front;
            ++col;
        }
    }

    while (loadedRows < PRICE_DATA_LENGTH &&
           fgets(buf, bufSize, fp)) {
        back = front = buf;
        col = 0;
        while (*front) {
            while (*front && *front != ',') ++front;
            *front = 0;
            if (col == time_col) {
                sscanf(back, "%ld", &rowTime);
                rowTime /= 1000;
                if (foundTime || rowTime >= time) {
                    *nextTime++ = rowTime;
                    foundTime = 1;
                } else {
                    // Skip processing rest of row
                    continue;
                }
            } else if (col == price_col && foundTime) {
                sscanf(back, "%u", nextPrice++);
                ++loadedRows;
            } // else, not a column we care about, keep going
            back = ++front;
            ++col;
        }
    }

    fclose(fp);
    p->validRows = loadedRows;
}