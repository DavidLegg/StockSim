#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "load_prices.h"
#include "execution.h"
#include "rng.h"

#include "strategies.h"

#define RANDOM_SYMBOL_BUFFER_SIZE 8192

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order) {
    static const int MAX_ITERS = 5;
    static int iters = 0;
    static int havePosition = 0;
    static long price;

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
    long price = state->priceFn(&(order->symbol), state->time, state->priceCache);

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
    long currentPrices[REBALANCING_MAX_SYMBOLS];
    long values[REBALANCING_MAX_SYMBOLS];
    long buyingPower = state->cash;

    struct PortfolioRebalanceArgs *args = (struct PortfolioRebalanceArgs *) order->aux;

    // Get current prices, current values for each asset class,
    //   and tally total available value
    for (int i = 0; i < args->symbolsUsed; ++i) {
        currentPrices[i] = state->priceFn(args->assets + i, state->time, state->priceCache);
        values[i] = 0;
        for (int j = 0; j < state->maxActivePosition; ++j) {
            if (state->positions[j].symbol.id == args->assets[i].id) {
                values[i] = state->positions[j].quantity * currentPrices[i];
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

    // Place all sell orders first, then place all buy orders
    // That way, the money from the sell orders can cover the buys
    // Calculate amount to buy/sell as difference between
    //   desired and current values, divided by price per unit of asset
    for (int i = 0; i < args->symbolsUsed; ++i) {
        values[i] = (long)round( ((buyingPower * args->weights[i]) - (double)values[i]) / currentPrices[i] );
        if (values[i] < 0) {
            sell(state, args->assets + i, -values[i]);
        }
    }
    for (int i = 0; i < args->symbolsUsed; ++i) {
        if (values[i] > 0) {
            buy(state, args->assets + i, values[i]);
        }
    }

    return Active;
}

enum OrderStatus randomPortfolioRebalance(struct SimState *state, struct Order *order) {
    struct RandomPortfolioRebalanceArgs *args = (struct RandomPortfolioRebalanceArgs *) order->aux;

    union Symbol *symbols = randomSymbols(args->numSymbols, state->time, 0);

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

enum OrderStatus buyBalanced(struct SimState *state, struct Order *order) {
    struct BuyBalancedArgs *args = (struct BuyBalancedArgs *)order->aux;
    const long value = (long)(args->totalValue * REBALANCING_BUFFER_FACTOR);
    long price;
    for (int i = 0; i < args->symbolsUsed; ++i) {
        price = state->priceFn(args->assets + i, state->time, state->priceCache);
        buy(state, args->assets + i, (int)round( (args->weights[i] * value) / (args->symbolsUsed * price) ));
    }
    return None;
}

/**
 * Misc. Helpful Tools
 */

union Symbol *randomSymbols(int n, time_t requiredDataStart, time_t requiredDataEnd) {
    int numSymbols, numViableSymbols = 0;
    const union Symbol *allSymbols = getAllSymbols(&numSymbols);
    union Symbol viableSymbols[RANDOM_SYMBOL_BUFFER_SIZE];
    int i;

    // Filter all symbols down to viable symbols
    time_t start, end;
    for (i = 0; i < numSymbols; ++i) {
        if ( getHistoricalPriceTimePeriod(allSymbols + i, &start, &end) &&
            (!requiredDataStart || start <= requiredDataStart) &&
            (!requiredDataEnd   || end   >= requiredDataEnd  ) ) {
            viableSymbols[numViableSymbols++].id = allSymbols[i].id;
        }
    }
    if (numViableSymbols < n) {
        fprintf(stderr, "Cannot choose %d symbols from %d viable symbols\n", n, numViableSymbols);
        exit(1);
    }

    union Symbol *chosenSymbols = malloc(sizeof(union Symbol) * n);
    int symbolsChosen = 0;

    while (symbolsChosen < n) {
        // Choose a random index for available symbols
        i = (int)(( (long)tsRand() * numViableSymbols ) / RAND_MAX );
        // Reject if already chosen:
        for (int j = 0; j < symbolsChosen; ++j) {
            if (chosenSymbols[j].id == viableSymbols[i].id) {
                i = -1;
                break;
            }
        }
        if (i >= 0) {
            chosenSymbols[symbolsChosen++].id = viableSymbols[i].id;
        }
    }

    return chosenSymbols;
}
