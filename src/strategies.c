#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "load_prices.h"
#include "execution.h"

#include "strategies.h"

#define RANDOM_SYMBOL_BUFFER_SIZE 8192

const char *DEFAULT_SYMBOLS_FILE = "resources/symbols.txt";

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order) {
    static const int MAX_ITERS = 5;
    static int iters = 0;
    static int havePosition = 0;
    static int price;

    if (!havePosition) {
        price = state->priceFn(&(order->symbol), state->time, state->priceCache);
        buy(state, &(order->symbol), order->quantity);
        havePosition = 1;
    } else if (state->priceFn(&(order->symbol), state->time, state->priceCache) > price) {
        sell(state, &(order->symbol), order->quantity);
        havePosition = 0;
        ++iters;
        if (iters >= MAX_ITERS) {
            return None;
        }
    }

    return Active;
}

enum OrderStatus timeHorizon(struct SimState *state, struct Order *order) {
    struct TimeHorizonArgs *aux = (struct TimeHorizonArgs*)order->aux;
    if (!aux->cutoff) {
        aux->cutoff = state->time + aux->offset;
    }
    if (state->time >= aux->cutoff) {
        // Remove all current orders
        for (int i = 0; i < state->maxActiveOrder; ++i) {
            state->orders[i].status = None;
        }
        // Liquidate all current positions
        for (int i = 0; i < state->maxActivePosition; ++i) {
            if (state->positions[i].quantity > 0) {
                sell(state, &(state->positions[i].symbol), state->positions[i].quantity);
            }
        }
        return None;
    } else {
        // Otherwise, don't do anything at all.
        return Active;
    }
}

enum OrderStatus meanReversion(struct SimState *state, struct Order *order) {
    struct MeanReversionArgs *aux = (struct MeanReversionArgs*)order->aux;
    int price = state->priceFn(&(order->symbol), state->time, state->priceCache);

    aux->ema = aux->ema * aux->emaDiscount + price * (1 - aux->emaDiscount);
    if (aux->initialSamples) {
        --aux->initialSamples;
    } else if (!aux->boughtQuantity && price < aux->buyFactor * aux->ema && price < state->cash) {
        aux->boughtPrice    = price;
        // Mathematically, price + price * FEE / 10000 is exactly
        // the price that we would pay.
        // Here, we add 1 to cover (conservatively) truncation error.
        aux->boughtQuantity = state->cash / (price + 1 + price * TRANSACTION_FEE / 10000);
        buy(state, &(order->symbol), aux->boughtQuantity);
    } else if (aux->boughtQuantity && (price > aux->sellFactor * aux->ema || price < aux->stopFactor * aux->boughtPrice)) {
        sell(state, &(order->symbol), aux->boughtQuantity);
        aux->boughtQuantity = 0;
    }

    return Active;
}

enum OrderStatus portfolioRebalance(struct SimState *state, struct Order *order) {
    int currentPrices[REBALANCING_MAX_SYMBOLS];
    int values[REBALANCING_MAX_SYMBOLS];
    int buyingPower = state->cash;

    struct PortfolioRebalanceArgs *args = (struct PortfolioRebalanceArgs *) order->aux;

    // Get current prices, current values for each asset class,
    //   and tally total available value
    for (int i = 0; i < args->symbolsUsed; ++i) {
        currentPrices[i] = state->priceFn(args->assets + i, state->time, state->priceCache);
        values[i] = 0;
        for (int j = 0; j < state->maxActivePosition; ++j) {
            if (state->positions[i].symbol.id == args->assets[i].id) {
                values[i] = state->positions[i].quantity * currentPrices[i];
                break;
            }
        }
        buyingPower += values[i];
    }

    // Calculate desired values for each asset class
    //   and place appropriate orders to achieve them
    if (buyingPower > args->maxAssetValue) {
        buyingPower = args->maxAssetValue;
    }
    buyingPower *= REBALANCING_BUFFER_FACTOR;
    int orderQuantity = 0;
    for (int i = 0; i < args->symbolsUsed; ++i) {
        // Calculate amount to buy/sell as difference between
        //   desired and current values, divided by price per unit of asset
        orderQuantity = (int)round( ((buyingPower * args->weights[i]) - (double)values[i]) / currentPrices[i] );
        if (orderQuantity > 0) {
            buy(state, args->assets + i, orderQuantity);
        } else if (orderQuantity < 0) {
            sell(state, args->assets + i, -orderQuantity);
        } // else orderQuantity == 0, no action needed
    }

    return Active;
}

enum OrderStatus randomPortfolioRebalance(struct SimState *state, struct Order *order) {
    struct RandomPortfolioRebalanceArgs *args = (struct RandomPortfolioRebalanceArgs *) order->aux;

    union Symbol *symbols = randomSymbols(args->numSymbols, NULL, state->time, 0);

    struct PortfolioRebalanceArgs *prArgs = (struct PortfolioRebalanceArgs *)makeCustomOrder(state, NULL, 0, portfolioRebalance)->aux;
    prArgs->symbolsUsed = args->numSymbols;
    prArgs->maxAssetValue = args->maxAssetValue;
    for (int i = 0; i < args->numSymbols; ++i) {
        prArgs->assets[i].id = symbols[i].id;
        prArgs->weights[i] = 1.0 / args->numSymbols;
    }

    free(symbols);

    return None;
}

/**
 * Misc. Helpful Tools
 */

union Symbol *randomSymbols(int n, const char *symbolsFile, time_t requiredDataStart, time_t requiredDataEnd) {
    if (!symbolsFile) symbolsFile = DEFAULT_SYMBOLS_FILE;

    FILE *fp = NULL;
    if (!(fp = fopen(symbolsFile, "r"))) {
        fprintf(stderr, "Cannot open symbols file %s\n", symbolsFile);
        exit(1);
    }

    union Symbol *allSymbols = malloc(sizeof(union Symbol) * RANDOM_SYMBOL_BUFFER_SIZE);

    // Figure out how many symbols there are to choose from:
    int numSymbols = 0;
    char c;
    int i = 0;
    time_t start, end;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
            for (; i < SYMBOL_LENGTH; ++i) {
                allSymbols[numSymbols].name[i] = '\0';
            }
            // If non-viable, don't increment numSymbols, thus don't logically record it
            if ( getHistoricalPriceTimePeriod(allSymbols + numSymbols, &start, &end) &&
                (!requiredDataStart || start <= requiredDataStart) &&
                (!requiredDataEnd   || end   >= requiredDataEnd  ) ) ++numSymbols;
            i = 0;
        } else {
            allSymbols[numSymbols].name[i] = c;
            ++i;
        }
    }
    if (numSymbols < n) {
        fprintf(stderr, "Cannot choose %d symbols from symbol file %s with %d symbols\n", n, symbolsFile, numSymbols);
    }

    // Move the file pointer back to the beginning of the file, and choose the symbols:
    rewind(fp);

    union Symbol *chosenSymbols = malloc(sizeof(union Symbol) * n);
    int symbolsChosen = 0;

    while (symbolsChosen < n) {
        // Choose a random index for available symbols
        i = (int)(( (long)rand() * numSymbols ) / RAND_MAX );
        // Reject if already chosen:
        for (int j = 0; j < symbolsChosen; ++j) {
            if (chosenSymbols[j].id == allSymbols[i].id) {
                i = -1;
                break;
            }
        }
        if (i >= 0) {
            chosenSymbols[symbolsChosen++].id = allSymbols[i].id;
        }
    }

    fclose(fp);
    return chosenSymbols;
}
