#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
        price = state->priceFn(&(order->symbol), state->time);
        buy(state, &(order->symbol), order->quantity);
        havePosition = 1;
    } else if (state->priceFn(&(order->symbol), state->time) > price) {
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
    long price = state->priceFn(&(order->symbol), state->time);

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
    int  quantities[REBALANCING_MAX_SYMBOLS];
    int  changeQuantities[REBALANCING_MAX_SYMBOLS];
    long values[REBALANCING_MAX_SYMBOLS];
    long buyingPower = state->cash;

    struct PortfolioRebalanceArgs *args = (struct PortfolioRebalanceArgs *) order->aux;

    // Get current prices, current values for each asset class,
    //   and tally total available value
    for (int i = 0; i < args->symbolsUsed; ++i) {
        currentPrices[i] = state->priceFn(args->assets + i, state->time);
        quantities[i] = 0;
        values[i] = 0;
        for (int j = 0; j < state->maxActivePosition; ++j) {
            if (state->positions[j].symbol.id == args->assets[i].id) {
                quantities[i] = state->positions[j].quantity;
                values[i]     = quantities[i] * currentPrices[i];
                break;
            }
        }
        buyingPower += values[i];
    }

    // Calculate desired values for each asset class
    //   and place appropriate orders to achieve them
    if (args->maxAssetValue >= 0 && buyingPower > args->maxAssetValue) {
        buyingPower = args->maxAssetValue;
    }
    buyingPower *= REBALANCING_BUFFER_FACTOR;

    // Place all sell orders first, then place all buy orders
    // That way, the money from the sell orders can cover the buys
    for (int i = 0; i < args->symbolsUsed; ++i) {
        changeQuantities[i] = (currentPrices[i] ? (int)floor( (buyingPower * args->weights[i]) / currentPrices[i] ) : 0) - quantities[i];
        if (changeQuantities[i] < 0) {
            sell(state, args->assets + i, -changeQuantities[i]);
        }
    }
    for (int i = 0; i < args->symbolsUsed; ++i) {
        if (changeQuantities[i] > 0) {
            buy(state, args->assets + i, changeQuantities[i]);
        }
    }

    return Active;
}

enum OrderStatus randomPortfolioRebalance(struct SimState *state, struct Order *order) {
    struct RandomPortfolioRebalanceArgs *args = (struct RandomPortfolioRebalanceArgs *) order->aux;

    union Symbol *symbols = randomSymbols(args->numSymbols, state->time, 0);

    union Symbol prSymbol;
    strncpy(prSymbol.name, "R-REBAL", SYMBOL_LENGTH);
    struct PortfolioRebalanceArgs *prArgs = (struct PortfolioRebalanceArgs *)makeCustomOrder(state, &prSymbol, 1, portfolioRebalance)->aux;
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
 * Calculates standard deviation divided by mean, to normalize
 */
double volatility(union Symbol *symbol, time_t start, time_t end, time_t sampleInterval) {
    double mean = 0.0;
    int k = 1;
    for (time_t t = start; t < end; t += sampleInterval, ++k) {
        mean += (getHistoricalPrice(symbol, t) - mean) / k;
    }

    double meanSquares = 0.0;
    long p;
    k = 1;
    for (time_t t = start; t < end; t += sampleInterval, ++k) {
        p = getHistoricalPrice(symbol, t);
        meanSquares += (((p - mean) * (p - mean)) - meanSquares) / k;
    }
    return sqrt(meanSquares) / mean;
}

enum OrderStatus volatilityPortfolioRebalance(struct SimState *state, struct Order *order) {
    struct VolatilityPortfolioRebalanceArgs *args = (struct VolatilityPortfolioRebalanceArgs *) order->aux;

    union Symbol prSymbol;
    strncpy(prSymbol.name, "V-REBAL", SYMBOL_LENGTH);
    struct PortfolioRebalanceArgs *prArgs = (struct PortfolioRebalanceArgs *)makeCustomOrder(state, &prSymbol, 1, portfolioRebalance)->aux;
    prArgs->maxAssetValue = args->maxAssetValue;
    prArgs->symbolsUsed   = 0;

    int attempts = 0;
    int duplicate;

    while (prArgs->symbolsUsed < args->numSymbols) {
        int sampleSize = 3*args->numSymbols;
        union Symbol *symbols = randomSymbols(sampleSize, state->time - args->history, state->time);
        for (int i = 0; i < sampleSize && prArgs->symbolsUsed < args->numSymbols; ++i) {
            duplicate = 0;
            for (int j = 0; j < prArgs->symbolsUsed; ++j) {
                if (symbols[i].id == prArgs->assets[j].id) {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate) continue;

            double v = volatility(symbols + i, state->time - args->history, state->time, args->sampleFrequency);
            if (abs(v - args->targetVolatility) < args->epsilon * args->targetVolatility) {
                prArgs->assets[prArgs->symbolsUsed].id = symbols[i].id;
                prArgs->weights[prArgs->symbolsUsed] = 1.0 / args->numSymbols;
                prArgs->symbolsUsed++;
            }
        }
        free(symbols);
        ++attempts;
        if (attempts > 10) {
            fprintf(stderr, "Failed 10 attempts to compile %d assets with volatility %.1lf%% +/- %.0lf%%, time %ld\n",
                args->numSymbols, args->targetVolatility*100, args->epsilon*100, state->time
                );
            exit(1);
        }
    }

    return None;
}

double meanPrice(union Symbol *symbol, time_t start, time_t end, time_t sampleInterval) {
    double mean = 0.0;
    int k = 1;
    for (time_t t = start; t < end; t += sampleInterval, ++k) {
        mean += (getHistoricalPrice(symbol, t) - mean) / k;
    }
    return mean;
}

enum OrderStatus meanPricePortfolioRebalance(struct SimState *state, struct Order *order) {
    struct MeanPricePortfolioRebalanceArgs *args = (struct MeanPricePortfolioRebalanceArgs *) order->aux;

    union Symbol prSymbol;
    strncpy(prSymbol.name, "MP-REBAL", SYMBOL_LENGTH);
    struct PortfolioRebalanceArgs *prArgs = (struct PortfolioRebalanceArgs *)makeCustomOrder(state, &prSymbol, 1, portfolioRebalance)->aux;
    prArgs->maxAssetValue = args->maxAssetValue;
    prArgs->symbolsUsed   = 0;

    int attempts = 0;
    int duplicate;

    while (prArgs->symbolsUsed < args->numSymbols) {
        int sampleSize = 5*args->numSymbols;
        union Symbol *symbols = randomSymbols(sampleSize, state->time - args->history, state->time);
        for (int i = 0; i < sampleSize && prArgs->symbolsUsed < args->numSymbols; ++i) {
            duplicate = 0;
            for (int j = 0; j < prArgs->symbolsUsed; ++j) {
                if (symbols[i].id == prArgs->assets[j].id) {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }

            double p = meanPrice(symbols + i, state->time - args->history, state->time, args->sampleFrequency);
            if (abs(p - args->targetPrice) < args->epsilon * args->targetPrice) {
                prArgs->assets[prArgs->symbolsUsed].id = symbols[i].id;
                prArgs->weights[prArgs->symbolsUsed] = 1.0 / args->numSymbols;
                prArgs->symbolsUsed++;
            }
        }
        free(symbols);
        ++attempts;
        if (attempts > 10) {
            fprintf(stderr, "Failed 10 attempts to compile %d assets with mean price $%.2lf +/- %.0lf%%, time %ld\n",
                args->numSymbols, args->targetPrice/(double)DOLLAR, args->epsilon*100, state->time
                );
            exit(1);
        }
    }

    return None;
}

enum OrderStatus buyBalanced(struct SimState *state, struct Order *order) {
    struct BuyBalancedArgs *args = (struct BuyBalancedArgs *)order->aux;
    const long value = (long)(args->totalValue * REBALANCING_BUFFER_FACTOR);
    long price;
    for (int i = 0; i < args->symbolsUsed; ++i) {
        price = state->priceFn(args->assets + i, state->time);
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
